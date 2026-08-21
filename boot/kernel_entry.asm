[bits 32]
[extern main]
[extern __bss_start]
[extern _end]
global _start
_start:
    ; Zero .bss: the bootloader loads only the initialized image, so
    ; every static without an initializer would otherwise start as
    ; leftover RAM garbage.
    mov edi, __bss_start
    mov ecx, _end
    sub ecx, edi
    xor eax, eax
    rep stosb
call main
jmp $
