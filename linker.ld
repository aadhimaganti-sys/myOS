OUTPUT_ARCH(aarch64)
ENTRY(_start)

SECTIONS
{
    . = 0x40080000;

    .text.boot : {
        KEEP(*(.text.boot))
    }

    .text : ALIGN(8) {
        *(.text .text.*)
    }

    .rodata : ALIGN(8) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(8) {
        *(.data .data.*)
    }

    . = ALIGN(16);
    __bss_start = .;
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    }
    . = ALIGN(16);
    __bss_end = .;

    . = ALIGN(16);
    __stack_bottom = .;
    . += 0x10000; /* 64 KB Stack */
    __stack_top = .;

    . = ALIGN(4096);
    __heap_start = .;
}
