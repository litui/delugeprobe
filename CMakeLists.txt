cmake_minimum_required(VERSION 3.12)

# set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/toolchain.cmake)

#
# board definition: pico or pico_w, default is pico
#
if(PICO_BOARD STREQUAL "")
    set(PICO_BOARD pico)
endif()
set(PICO_STDIO_UART 0)

#
# below order is important
#
include(pico_sdk_import.cmake)

# set(CMAKE_SYSTEM_NAME Generic)

set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/freertos)
include(FreeRTOS_Kernel_import.cmake)

set(PICO_BOARD_HEADER_DIRS  ${CMAKE_CURRENT_LIST_DIR}/include/boards)
pico_sdk_init()

# set(CMAKE_C_COMPILER_FORCED TRUE)
# set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
# set(CMAKE_C_COMPILER_WORKS TRUE)
#set(CMAKE_CXX_COMPILER_WORKS TRUE)

project(${PROJECT} "C" "CXX")

string(TOUPPER ${PICO_BOARD} PICO_BOARD_UPPER)

#
# set some project specifica
#
add_definitions(
        -DPICO_CORE1_STACK_SIZE=0x400
        -DPICO_HEAP_SIZE=0x0
        -DPICO_STACK_SIZE=0x400
        -DTARGET_BOARD_${PICO_BOARD_UPPER}
        -DCMAKE_SYSTEM_NAME=Generic
)

#
# set some things inherited from Makefile / command line
#
if (PICOPROBE_VERSION_MAJOR)
        add_definitions(-DPICOPROBE_VERSION_MAJOR=${PICOPROBE_VERSION_MAJOR})
else()
        set(PICOPROBE_VERSION_MAJOR 0)
endif()
if (PICOPROBE_VERSION_MINOR)
        add_definitions(-DPICOPROBE_VERSION_MINOR=${PICOPROBE_VERSION_MINOR})
else()
        set(PICOPROBE_VERSION_MINOR 0)
endif()

# set version string to "x.yy"
# there are perhaps smarter ways...
if (PICOPROBE_VERSION_MINOR LESS "10")
        add_definitions(-DPICOPROBE_VERSION_STRING="${PICOPROBE_VERSION_MAJOR}.0${PICOPROBE_VERSION_MINOR}")
else()
        add_definitions(-DPICOPROBE_VERSION_STRING="${PICOPROBE_VERSION_MAJOR}.${PICOPROBE_VERSION_MINOR}")
endif()

if (GIT_HASH)
        add_definitions(-DGIT_HASH="${GIT_HASH}")
endif()


########################################################################################################################


add_executable(${PROJECT}
        src/cdc_uart.c
        src/dap_util.c
        src/get_serial.c
        src/led.c
        src/main.c
        src/misc_utils.c
        src/probe.c
        src/rtt_console.c
        src/sw_dp_pio.c
        src/sw_lock.c
        src/usb_descriptors.c
)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        target_sources(${PROJECT} PRIVATE
                src/cdc_debug.c
        )
endif()

target_sources(${PROJECT} PRIVATE
        CMSIS_5/CMSIS/DAP/Firmware/Source/DAP.c
        CMSIS_5/CMSIS/DAP/Firmware/Source/JTAG_DP.c
        CMSIS_5/CMSIS/DAP/Firmware/Source/DAP_vendor.c
        CMSIS_5/CMSIS/DAP/Firmware/Source/SWO.c
)

target_include_directories(${PROJECT} PRIVATE
        CMSIS_5/CMSIS/DAP/Firmware/Include/
        CMSIS_5/CMSIS/Core/Include/
        include/
)

#--------------------------------------------------------------------------------------------------
#
# this is for MSC/DAPLink operation
#
target_sources(${PROJECT} PRIVATE
        src/msc/msc_drive.c
        src/msc/msc_utils.c
        src/daplink/daplink/util.c
        src/daplink/daplink/drag-n-drop/flash_manager.c
        src/daplink/daplink/interface/swd_host.c
        src/daplink/daplink/interface/target_flash.c
        src/daplink/daplink/settings/settings_rom_stub.c
        src/daplink/family/renesas/target_reset_rza1lu.c
        src/daplink/family/renesas/rza1lu/target.c
        src/daplink/target/target_board.c
        src/daplink/target/target_family.c
        src/daplink-pico/board/rp2040/pico.c
        src/daplink-pico/board/rp2040/pico_target_utils.c
        src/daplink-pico/board/rp2040/program_flash_generic.c
        src/daplink-pico/family/raspberry/rp2040/target_reset_rp2040.c
)

target_include_directories(${PROJECT} PRIVATE
        src/daplink/daplink
        src/daplink/daplink/cmsis-dap
        src/daplink/daplink/drag-n-drop
        src/daplink/daplink/interface
        src/daplink/daplink/settings
        src/daplink/hic_hal
        src/daplink/target
        src/daplink-pico/board/rp2040
        src/daplink-pico/family/raspberry
        src/daplink-pico/hic_hal/raspberry/rp2040
        $ENV{PICO_SDK_PATH}/src/common/boot_uf2/include
)

# Both are actually dummies to prevent the DAPLink files from throwing errors
# Actually not required because picoprobes structure is different
add_compile_definitions(INTERFACE_RP2040=1
                        DAPLINK_IF=1)

# not sure if this is still required, but this equals to DAPLINK_HIC_ID_RP2040 in daplink.h
add_compile_definitions(DAPLINK_HIC_ID=0x08154711)

# required to get required DAPLink functions
add_compile_definitions(DRAG_N_DROP_SUPPORT=1)

# required for Renesas drag n drop
add_compile_definitions(TARGET_MCU_CORTEX_A=1)

#--------------------------------------------------------------------------------------------------
#
# this is for SIGROK operation (taken from https://github.com/pico-coder/sigrok-pico)
#
target_sources(${PROJECT} PRIVATE
        src/pico-sigrok/cdc_sigrok.c
        src/pico-sigrok/sigrok.c
        src/pico-sigrok/sigrok_int.c
)

target_link_libraries(${PROJECT} PRIVATE
        hardware_adc
        hardware_dma
)

pico_generate_pio_header(${PROJECT} ${CMAKE_CURRENT_LIST_DIR}/src/pico-sigrok/sigrok.pio)

#--------------------------------------------------------------------------------------------------

target_compile_options(${PROJECT} PRIVATE -Wall)

if(DEFINED ENV{PICOPROBE_LED})
    message("PICOPROBE_LED is defined as " $ENV{PICOPROBE_LED})
    target_compile_definitions(${PROJECT} PRIVATE PICOPROBE_LED=$ENV{PICOPROBE_LED})
endif()

set(DBG_PIN_COUNT=4)

pico_generate_pio_header(${PROJECT} ${CMAKE_CURRENT_LIST_DIR}/src/probe.pio)

target_include_directories(${PROJECT} PRIVATE src)

target_compile_definitions (${PROJECT} PRIVATE
	PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1
	PICO_RP2040_USB_DEVICE_UFRAME_FIX=1
)

target_link_libraries(${PROJECT} PRIVATE
        pico_multicore
        pico_stdlib
        pico_unique_id
        tinyusb_device
        tinyusb_board
        hardware_pio
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4
)

if(PICO_BOARD STREQUAL "pico_w")
    target_link_libraries(${PROJECT} PRIVATE
        #pico_cyw43_arch_lwip_sys_freertos
        #pico_lwip_iperf
        pico_cyw43_arch_none
    )
endif()


#
# code resides in flash (in former versions it was in RAM)
# if you want to force single functions into RAM, use "__no_inline_not_in_flash_func()" from platform.h
# 
pico_set_binary_type(${PROJECT} default)

pico_add_extra_outputs(${PROJECT})
