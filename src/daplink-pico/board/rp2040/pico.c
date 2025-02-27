/**
 * @file    pico.c
 * @brief   board code for Pico
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2009-2019, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Code/ideas are partially taken from pico_probe.  Other parts from DAPLink.
 *
 * Note that handling of the rescue DP has been dropped (no idea how to test this).
 *
 * Two important global variables:
 * - \a g_board_info contains information about the probe, e.g. how to perform
 *   probe initialization, send a HW reset to the target, etc
 * - \a g_target_family tells things like: what is the actual reset sequence for
 *   the target (family), how to set the state of the target (family), etc.
 *   This may differ really from target to target although all have "standard" DAPs
 *   included.  See RP2040 with dual cores and dormant sequence which does not
 *   like the JTAG2SWD sequence.  Others like the nRF52840 have other reset
 *   sequences.
 *
 * TODO I'm really not sure about the architectural idea of DAPLink:  is board the
 *      target board or the probe board?  So is g_board_info a probe or a target
 *      item?  Anyway I have decided to put some probe stuff into this files.
 */

#include "DAP_config.h"
#include "DAP.h"
#include "swd_host.h"

#include "target_family.h"
#include "target_board.h"
#include "target_rp2040.h"
#include "program_flash_generic.h"

#include "probe.h"



// these are IDs for target identification, required registers to identify may/do differ
const uint32_t swd_id_rp2040    = (0x927) + (0x0002 << 12);    // taken from RP2040 SDK platform.c
const uint32_t swd_id_rza1lu    = 0x3ba02477; // taken from DPIDR on MBed controller

// IDs for UF2 identification, use the following command to obtain recent list:
// curl https://raw.githubusercontent.com/microsoft/uf2/master/utils/uf2families.json | jq -r '.[] | "\(.id)\t\(.description)"' | sort -k 2
const uint32_t uf2_id_rza1lu    = 0x9517422f;  // random pick
const uint32_t uf2_id_rp2040    = 0xe48bff56;

// IDs for board identification (but whatfor?)
#define board_id_rza1lu_deluge    "5501"  // yoinked from the GR-LYCHEE
#define board_id_rp2040_pico      "7f01"

// here we can modify the otherwise constant board/target information
target_cfg_t target_device;
static char board_vendor[30];
static char board_name[30];



// target information for RP2040 (actually Pico), must be global
// because a special algo is used for flashing, corresponding fields below are empty.
target_cfg_t target_device_rp2040 = {
    .version                        = kTargetConfigVersion,
    .sectors_info                   = NULL,
    .sector_info_length             = 0,
    .flash_regions[0].start         = 0x10000000,
    .flash_regions[0].end           = 0x10000000 + MB(2),
    .flash_regions[0].flags         = kRegionIsDefault,
    .flash_regions[0].flash_algo    = NULL,
    .ram_regions[0].start           = 0x20000000,
    .ram_regions[0].end             = 0x20000000 + KB(256),
    .target_vendor                  = "RaspberryPi",
    .target_part_number             = "RP2040",
    .rt_family_id                   = TARGET_RP2040_FAMILY_ID,
    .rt_board_id                    = board_id_rp2040_pico,
    .rt_uf2_id                      = uf2_id_rp2040,
    .rt_max_swd_khz                 = 25000,
    .rt_swd_khz                     = 15000,
};


// target information for a generic device which allows at least RTT (if connected)
target_cfg_t target_device_generic = {
    .version                        = kTargetConfigVersion,
    .sectors_info                   = NULL,
    .sector_info_length             = 0,
    .flash_regions[0].start         = 0x00000000,
    .flash_regions[0].end           = 0x00000000 + MB(1),
    .flash_regions[0].flags         = kRegionIsDefault,
    .flash_regions[0].flash_algo    = NULL,
    .ram_regions[0].start           = 0x20000000,
    .ram_regions[0].end             = 0x20000000 + KB(256),
    .erase_reset                    = 1,
    .target_vendor                  = "Generic",
    .target_part_number             = "cortex_m",
    .rt_family_id                   = kStub_SWSysReset_FamilyID,
    .rt_board_id                    = "ffff",
    .rt_uf2_id                      = 0,                               // this also implies no write operation
    .rt_max_swd_khz                 = 10000,
    .rt_swd_khz                     = 2000,
};

// target information for SWD not connected
target_cfg_t target_device_disconnected = {
    .version                        = kTargetConfigVersion,
    .sectors_info                   = NULL,
    .sector_info_length             = 0,
    .flash_regions[0].start         = 0x00000000,
    .flash_regions[0].end           = 0x00000000 + MB(1),
    .flash_regions[0].flags         = kRegionIsDefault,
    .flash_regions[0].flash_algo    = NULL,
    .ram_regions[0].start           = 0x20000000,
    .ram_regions[0].end             = 0x20000000 + KB(256),
    .erase_reset                    = 1,
    .target_vendor                  = "Disconnected",
    .target_part_number             = "Disconnected",
    .rt_family_id                   = kStub_SWSysReset_FamilyID,
    .rt_board_id                    = NULL,                            // indicates not connected
    .rt_uf2_id                      = 0,                               // this also implies no write operation
    .rt_max_swd_khz                 = 10000,
    .rt_swd_khz                     = 2000,
};

extern target_cfg_t target_device_rza1lu;

static void search_family(void)
{
    // force search of family
    g_target_family = NULL;

    // search family
    init_family();
}   // search_family



/**
 * Search the correct board / target / family.
 * Currently Renesas r7s7210xx and RP2040 are auto detected.
 *
 * Global outputs are \a g_board_info, \a g_target_family.  These are the only variables that should be (read) accessed
 * externally.
 *
 * \note
 *    I'm not sure if the usage of board_vendor/name is correct here.
 */
void pico_prerun_board_config(void)
{
    bool r;
    bool target_found = false;

    // printf("Running pico_prerun_board_config...\n");

    target_device = target_device_generic;
    probe_set_swclk_freq(target_device.rt_swd_khz);                            // slow down during target probing

    if ( !target_found) {
        // check for RP2040
        target_device = target_device_rp2040;
        search_family();
        if (target_set_state(ATTACH)) {
            uint32_t chip_id;

            r = swd_read_word(0x40000000, &chip_id);
            if (r  &&  (chip_id & 0x0fffffff) == swd_id_rp2040) {
                target_found = true;
                strcpy(board_vendor, "RaspberryPi");
                strcpy(board_name, "Pico");

                // get size of targets flash
                uint32_t size = target_rp2040_get_external_flash_size();
                if (size > 0) {
                    target_device.flash_regions[0].end = target_device.flash_regions[0].start + size;
                }
            }
        }
    }

    if (!target_found) {
        // printf("Searching for renesas target\n");

        target_device = target_device_rza1lu;
        target_device.rt_family_id   = kRenesas_FamilyID;
        target_device.rt_board_id    = board_id_rza1lu_deluge;
        target_device.rt_uf2_id      = uf2_id_rza1lu;
        target_device.rt_max_swd_khz = 10000;
        target_device.rt_swd_khz     = 6000;
        target_device.target_part_number = "r7s7210xx";
        strcpy(board_vendor, "Synthstrom");
        strcpy(board_name, "Deluge");

        search_family();
        // printf("Family ID found? %u\n", g_target_family->family_id);

        if (target_set_state(ATTACH)) {
            uint32_t dbg_id;

            // printf("Attached renesas target\n");
            // printf("Checking for renesas chip.\r");

            r = swd_read_dp(0x0, &dbg_id);
            // printf("DBGID: %lu", dbg_id);
            // printf("Comparing to: %lu\n", swd_id_rza1lu);

            if (dbg_id == swd_id_rza1lu) {
                target_found = true;
            }
        }
    }

    if ( !target_found) {
        if (target_set_state(ATTACH)) {
            // set generic device
            target_device = target_device_generic;         // holds already all values
            search_family();
            strcpy(board_vendor, "Generic");
            strcpy(board_name, "Generic");
        }
        else {
            // Disconnected!
            // Note that .rt_board_id is set to NULL to show the disconnect state.
            // This is actually a hack to provide other layers with some dummy g_board_info.target_cfg
            target_device = target_device_disconnected;    // holds already all values
            search_family();
            strcpy(board_vendor, "Disconnected");
            strcpy(board_name, "Disconnected");
        }
    }

    probe_set_swclk_freq(target_device.rt_swd_khz);

    target_set_state(RESET_RUN);
}   // pico_prerun_board_config



/**
 * This is the global variable holding information about probe and target.
 */
const board_info_t g_board_info = {
    .info_version        = kBoardInfoVersion,
    .board_id            = "0000",                // see e.g. https://github.com/pyocd/pyOCD/blob/main/pyocd/board/board_ids.py and https://os.mbed.com/request-board-id
    .daplink_url_name    = "-unknown-",
    .daplink_drive_name  = "-unknown-",
    .daplink_target_url  = "https://daplink.io",
    .target_cfg          = &target_device,
    .board_vendor        = board_vendor,
    .board_name          = board_name,
    .prerun_board_config = pico_prerun_board_config,
};
