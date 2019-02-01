.code16
.intel_syntax noprefix                                        

.global _start

_start:
# Инициализация адресов сегментов. Эти операции требуется не для любого BIOS, но их рекомендуется проводить.
    cli # off interrupts ( for safe stack)
    mov ax, cs # Saving address of code segment in ax
    mov ds, ax # Saving this address as beginning of data segment
    mov ss, ax # And stack segment too
    mov sp, _start # Saving stack address as address of first instruction these code. Stack will grow up and doesn't block code
    sti # on interrupts

    mov al, 4 # get hours
    call _get_time
    mov [0x00000500], al

    mov al, 2 # get minutes
    call _get_time
    mov [0x00000520], al

    mov al, 0 #get seconds
    call _get_time
    mov [0x00000540], al

_load_drive:
    mov bx, 0x1000
    mov es, bx
    xor bx, bx
    mov al, 0x12 # sum of sectors to read
    mov dl, 0x01 # number of drive
    mov dh, 0x00 # number of head
    mov ch, 0x00 # number of track(cylinder)
    mov cl, 0x01 # number of sector
    mov ah, 0x02 # function for int 0x13
    int 0x13 # read data from 0x1000

enable_protected_mode:
    cli # off interrupts
    lgdt gdt_info # Loading volume and address of description table
    in al, 0x92 # Enabling A20 address line
    or al, 0x02 
    out 0x92, al
    mov eax, cr0 # Set PE bit of CR0 register
    or al, 0x01
    mov cr0, eax
    jmp 0x8:_protected_mode # Long jump in order to load correct information (because of architecture we cannot do It straight)

_get_time: # get time from port 0x71 (al==4 -- hours, al==2 -- minutes, al==0 --seconds)
    out 0x70, al
    in al, 0x71
    ret

gdt:
.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
.byte 0xff, 0xff, 0x00, 0x00, 0x00, 0x9A, 0xCF, 0x00
.byte 0xff, 0xff, 0x00, 0x00, 0x00, 0x92, 0xCF, 0x00
gdt_info:
.word gdt_info - gdt
.word gdt, 0

.code32
_protected_mode:
    mov ax, 0x10 # Loading DS and SS registers with the data segment selector
    mov es, ax
    mov ds, ax
    mov ss, ax
    call 0x10000

    .zero (512 - ($ - _start) - 2)
    .byte 0x55, 0xAA
