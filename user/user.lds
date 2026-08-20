/* Link layout for the ring-3 demo program. The binary must start with
 * _start at 0x100000 (USER_BIN_ADDR) so the kernel entry point is the
 * program's real entry, not whatever function the compiler emitted first. */
ENTRY(_start)
SECTIONS
{
    . = 0x100000;
    .text : {
        *(.text.entry)
        *(.text*)
    }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) }
    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note.gnu.property) }
}