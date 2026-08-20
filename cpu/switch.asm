; C globals driving the switch.
[extern need_switch]
[extern switch_from]
[extern switch_to]

; Shared interrupt epilogue. Both interrupt stubs jump here after the C
; handler returns. If the scheduler requested a switch, the interrupted
; task's stack pointer (currently pointing at the ds slot of its saved
; frame) is parked in *switch_from and we resume the new task's frame.
; Otherwise we resume the current task right where it was interrupted.
global task_epilogue
task_epilogue:
    cmp byte [need_switch], 0
    je .no_switch
    mov eax, [switch_from]
    mov [eax], esp
    mov esp, [switch_to]
    mov byte [need_switch], 0
.no_switch:
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8
    sti
    iret

; Bootstrap context switch into the shell task. Called from C with the
; shell's frame esp; we never return along this path.
global first_switch
first_switch:
    mov eax, [esp+4]
    mov esp, eax
    jmp task_epilogue