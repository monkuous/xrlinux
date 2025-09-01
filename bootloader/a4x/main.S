.section ".header", "a", %progbits

.type BxBootMagic, @object
BxBootMagic: .long 0x676f646e
.size BxBootMagic, . - BxBootMagic

.section ".entrypoint", "ax", %progbits

.balign 4
.globl BxStartup
.type BxStartup, @function
BxStartup:
    # zero out bss
    lui t0, zero, BxBssStart - 4
    ori t0, t0, BxBssStart - 4
    lui t1, zero, BxBssSize
    ori t1, t1, BxBssSize
    beq t1, 2f
1:  mov long [t0 + t1], zero
    subi t1, t1, 4
    bne t1, 1b
2:  # save data necessary for BxReturnToFirmware
    lui t0, zero, BxFirmwareRegisters
    ori t0, t0, BxFirmwareRegisters
    mov long [t0 + 0x00], s0
    mov long [t0 + 0x04], s1
    mov long [t0 + 0x08], s2
    mov long [t0 + 0x0c], s3
    mov long [t0 + 0x10], s4
    mov long [t0 + 0x14], s5
    mov long [t0 + 0x18], s6
    mov long [t0 + 0x1c], s7
    mov long [t0 + 0x20], s8
    mov long [t0 + 0x24], s9
    mov long [t0 + 0x28], s10
    mov long [t0 + 0x2c], s11
    mov long [t0 + 0x30], s12
    mov long [t0 + 0x34], s13
    mov long [t0 + 0x38], s14
    mov long [t0 + 0x3c], s15
    mov long [t0 + 0x40], s16
    mov long [t0 + 0x44], s17
    mov long [t0 + 0x48], sp
    mov long [t0 + 0x4c], lr
    # jump to real entry point
    addi s0, zero, 0
    jal BxMain
    brk
.size BxStartup, . - BxStartup

.section ".text.BxReturnToFirmware", "ax", %progbits

.balign 4
.globl BxReturnToFirmware
.type BxReturnToFirmware, @function
BxReturnToFirmware:
    lui t0, zero, BxFirmwareRegisters
    ori t0, t0, BxFirmwareRegisters
    mov s0, long [t0 + 0x00]
    mov s1, long [t0 + 0x04]
    mov s2, long [t0 + 0x08]
    mov s3, long [t0 + 0x0c]
    mov s4, long [t0 + 0x10]
    mov s5, long [t0 + 0x14]
    mov s6, long [t0 + 0x18]
    mov s7, long [t0 + 0x1c]
    mov s8, long [t0 + 0x20]
    mov s9, long [t0 + 0x24]
    mov s10, long [t0 + 0x28]
    mov s11, long [t0 + 0x2c]
    mov s12, long [t0 + 0x30]
    mov s13, long [t0 + 0x34]
    mov s14, long [t0 + 0x38]
    mov s15, long [t0 + 0x3c]
    mov s16, long [t0 + 0x40]
    mov s17, long [t0 + 0x44]
    mov sp, long [t0 + 0x48]
    mov lr, long [t0 + 0x4c]
    addi a3, zero, 0
    jalr zero, lr, 0
.size BxReturnToFirmware, . - BxReturnToFirmware

.section ".bss.BxFirmwareRegisters", "aw", %nobits

.balign 4
.type BxFirmwareRegisters, @object
BxFirmwareRegisters: .space 0x50
.size BxFirmwareRegisters, . - BxFirmwareRegisters
