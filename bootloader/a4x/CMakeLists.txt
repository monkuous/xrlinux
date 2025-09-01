target_include_directories(bootloader PRIVATE .)
target_compile_options(bootloader PRIVATE -fno-pic)
target_link_options(bootloader PRIVATE -static)
target_link_script(bootloader a4x.lds)
target_sources(bootloader PRIVATE
        a4x.c
        main.c
        main.S
)

set(out "${CMAKE_CURRENT_BINARY_DIR}/a4x.bin")
add_custom_command(OUTPUT "${out}"
        COMMAND "${CMAKE_OBJCOPY}" "$<TARGET_FILE:bootloader>" "${out}" -Obinary
        DEPENDS bootloader
        VERBATIM)
add_custom_target(loader-image ALL DEPENDS "${out}")
