cmake_minimum_required(VERSION 3.31)
project(bootloader VERSION 0.1.0 LANGUAGES C ASM)

include(cmake/utils.cmake)

set(BOOTLOADER_PLATFORM a4x CACHE STRING "The platform the bootloader will run on")
set_property(CACHE BOOTLOADER_PLATFORM PROPERTY STRINGS a4x)

add_compile_options(-fdata-sections -ffunction-sections -Wall -Wextra)
add_link_options(LINKER:--gc-sections LINKER:--sort-section=alignment)

add_executable(bootloader
        config.c
        dt.c
        filesystem.c
        intrinsics.c
        logging.c
        main.c
        memory.c
        partition.c
        transition.S
)
target_include_directories(bootloader PRIVATE .)
target_compile_options(bootloader PRIVATE -ffreestanding)
target_link_options(bootloader PRIVATE -nostdlib)

add_subdirectory("${BOOTLOADER_PLATFORM}")
