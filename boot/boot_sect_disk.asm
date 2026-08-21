; Load KERNEL_LOAD_SECTORS sectors starting at LBA 1 (the sector right
; after the boot sector) to ES:BX. The BIOS rejects int 13h reads that
; cross a floppy track boundary, so this loops in track-sized chunks,
; keeping all state in registers (the boot sector is tight on space):
;   CH=cyl  CL=sector  DH=head  DL=drive
;   DI=sectors remaining  BP=count for the current chunk  ES:BX=dest

SPT equ 18                          ; sectors per track
HPC equ 2                           ; heads per cylinder

disk_load:
    pusha
    push es

    mov di, KERNEL_LOAD_SECTORS
    mov dh, 0                       ; head
    mov cl, 2                       ; first sector to read
    mov dl, [BOOT_DRIVE]

.chunk:
    ; count = min(remaining, sectors left on this track)
    mov ax, cx
    and ax, 0x00FF                  ; current sector
    mov bp, SPT + 1
    sub bp, ax                      ; space incl. current sector
    cmp di, bp
    jae .have                       ; remaining >= space -> use space
    mov bp, di                      ; remaining < space -> use remaining
.have:
    mov ax, bp                        ; AL = count
    mov ah, 0x02
    int 0x13                          ; CH/CL/DH/DL already set
    jc disk_error
    cmp ax, bp                        ; success: AH=0 and AL == requested
    jnz disk_error                    ; (never trust a short transfer)

    ; advance destination by count*512, bumping ES on carry
    mov ax, bp                        ; advance by the REQUESTED count
    shl ax, 9
    add bx, ax
    jnc .no_seg_bump
    mov ax, es
    add ax, 0x1000
    mov es, ax
.no_seg_bump:
    ; next sector / head / cylinder
    mov ax, cx
    and ax, 0x00FF
    add ax, bp
    cmp ax, SPT
    jbe .pack
    sub ax, SPT
    inc dh
    cmp dh, HPC
    jb .pack
    mov dh, 0
    inc ch                          ; next cylinder
.pack:
    mov cl, al                      ; new sector; CH untouched
    sub di, bp
    jnz .chunk

    pop es
    popa
    ret

disk_error:
    mov bx, DISK_ERROR
    call print
    call print_nl
    jmp $

DISK_ERROR: db "Disk error", 0
