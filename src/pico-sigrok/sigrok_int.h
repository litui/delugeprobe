/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef SIGROK_INT_H
#define SIGROK_INT_H


#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
    extern "C" {
#endif


//Pin usage
//GP0 and 1 are reserved for debug uart
//GP2-GP22 are digital inputs
//GP23 controls power supply modes and is not a board input
//GP24-25 are not on the board and not used
//GP26-28 are ADC.
#if 1
    // number of analog channels
    #define SR_NUM_A_CHAN   3
    // first digital channel port
    #define SR_BASE_D_CHAN  10
    // number of digital channels
    #define SR_NUM_D_CHAN   8
    // Storage size of the DMA buffer.  The buffer is split into two halves so that when the first
    // buffer fills we can send the trace data serially while the other buffer is DMA'd into
    #define SR_DMA_BUF_SIZE 100000
#else
    #define SR_NUM_A_CHAN   3
    // first digital channel port
    #define SR_BASE_D_CHAN  2
    // number of digital channels
    #define SR_NUM_D_CHAN   21
    // Storage size of the DMA buffer.  The buffer is split into two halves so that when the first
    // buffer fills we can send the trace data serially while the other buffer is DMA'd into
    #define SR_DMA_BUF_SIZE 220000
#endif

// Mask for the digital channels fetched from PIO
#define SR_PIO_D_MASK       ((1 << (SR_NUM_D_CHAN)) - 1)

// Mask for the digital channels at GPIO level
#define SR_GPIO_D_MASK      (((1 << (SR_NUM_D_CHAN)) - 1) << (SR_BASE_D_CHAN))


#if !defined(NDEBUG)
    int cdc_debug_printf(const char* format, ...) __attribute__ ((format (printf, 1, 2)));
#endif
#if 1  &&  !defined(NDEBUG)
    #define Dprintf(format,args...) cdc_debug_printf("(SR) " format, ## args)
#else
    #define Dprintf(format,...) ((void)0)
#endif


typedef struct sr_device {
    uint32_t sample_rate;
    uint32_t num_samples;
    uint32_t a_mask,d_mask;
    uint32_t samples_per_half; //number of samples for one of the 4 dma target arrays
    uint8_t a_chan_cnt;                              //!< count of enabled analog channels
    uint8_t d_chan_cnt;                              //!< count of enabled digital channels
    uint8_t d_tx_bps;   //Digital Transmit bytes per slice
    //Pins sampled by the PIO - 4,8,16 or 32
    uint8_t pin_count;
    uint8_t d_nps; //digital nibbles per slice from a PIO/DMA perspective.
    uint32_t scnt; //number of samples sent
    uint32_t d_size,a_size; //size of each of the two data buffers for each of a& d
    uint32_t dbuf0_start,dbuf1_start,abuf0_start,abuf1_start; //starting memory pointers of adc buffers
    uint32_t cmdstr_ndx;                             //!< index into \a cmdstr
    char cmdstr[20];                                 //!< used for parsing input
    char rspstr[20];                                 //!< response string of a \a cmdstr

    // mark key control variables volatile since multiple cores might access them
    volatile bool all_started;                       //!< sampling and transmission has been started (incl initialization)
    volatile bool sample_and_send;                   //!< sample and send data
    volatile bool continuous;                        //!< continuous sample mode
    volatile bool aborted;                           //!< abort sampling and transmission (due to host command or overflow)
    volatile bool send_resp;                         //!< send the response string contained in \a rspstr

    /*Depracated trigger logic
//If HW trigger enabled, uncomment all usages
  //volatile bool notfirst;  //Have we processed at least a first sample (so that lval is correct
  //  volatile bool triggered;
  //  uint32_t tlval; //last digital sample value - must keep it across multiple calls to send_slices for trigger
  //  uint32_t lvl0mask,lvl1mask,risemask,fallmask,chgmask;
  End depracated trigger logic*/

} sr_device_t;


//
// shared between modules
//
extern sr_device_t sr_dev;

void sigrok_tx_init(sr_device_t *d);
void sigrok_reset(sr_device_t *d);
void sigrok_full_reset(sr_device_t *d);

void sigrok_notify(void);

#ifdef __cplusplus
    }
#endif

#endif
