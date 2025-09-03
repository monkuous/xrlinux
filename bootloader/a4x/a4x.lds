ENTRY(BxStartup)

PHDRS {
    header PT_LOAD FLAGS(4);
    text   PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data   PT_LOAD FLAGS(6);
}

SECTIONS {
    . = 0x3000;
    .header : { KEEP(*(.header)) } :header
    .text   : { *(.entrypoint) *(.text .text.*) } :text
    .rodata : { *(.rodata .rodata.*) } :rodata
    .data   : { *(.data .data.*) } :data
    . = ALIGN(4);
    .bss    : { *(.bss .bss.* COMMON) . = ALIGN(4); } :data

    BxBssStart = ADDR(.bss);
    BxBssSize = SIZEOF(.bss);
    BxImageEnd = .;

    ASSERT(BxStartup == 0x3004, "invalid address for entrypoint")
    ASSERT(BxBssSize % 4 == 0, ".bss size is not a multiple of 4")

    /DISCARD/ : { *(.note* .eh_frame*) }
}
