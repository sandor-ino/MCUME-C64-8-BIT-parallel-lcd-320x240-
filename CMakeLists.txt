cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

set(TARGET pico64)

project(${TARGET} C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_TIME_CRITICAL_SECTION_NAME ".time_critical")
pico_sdk_init()



# FATFS
add_subdirectory(FatFs_SPI build)

# Include paths
include_directories(pico64)
include_directories(config)
include_directories(display)



# Sorgenti C64
set(PICO64_SOURCES 
    pico64/c64.cpp 
    pico64/cia1.cpp
    pico64/cia2.cpp
    pico64/cpu.cpp
    pico64/patches.cpp
    pico64/pla.cpp
    pico64/roms.cpp
    pico64/sid.cpp
    pico64/timerutil.cpp
    pico64/vic.cpp
    pico64/reSID.cpp
    pico64/pico64.cpp
)

# Display e periferiche
set(DISPLAY_SOURCES 
       display/pico_dsp.cpp
       display/emuapi.cpp
    display/AudioPlaySystem.cpp
    display/ili9486_parallel.pio
)

# Crea eseguibile
add_executable(${TARGET}
    ${PICO64_SOURCES}
    ${DISPLAY_SOURCES}
)



# Librerie
target_link_libraries(${TARGET} 
    pico_stdlib
    FatFs_SPI
    pico_multicore
    hardware_adc
    hardware_pio
    hardware_pwm
    hardware_dma
    hardware_irq
)

# Ottimizzazione
target_compile_options(${TARGET} PUBLIC 
    -O3 
    -Wall 
    -Wextra 
    -Wno-unused-function 
    -Wno-unused-parameter
)

# SD card
if(NOT DEFINED N_SD_CARDS)
    set(N_SD_CARDS 1)
endif()
target_compile_definitions(${TARGET} PUBLIC N_SD_CARDS=${N_SD_CARDS})

# Stdio
pico_enable_stdio_uart(${TARGET} 0)
pico_enable_stdio_usb(${TARGET} 0)

# Output
pico_add_extra_outputs(${TARGET})
set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME "pico64")

# PIO per ILI9486 parallelo
pico_generate_pio_header(${TARGET} ${CMAKE_CURRENT_LIST_DIR}/display/ili9486_parallel.pio)