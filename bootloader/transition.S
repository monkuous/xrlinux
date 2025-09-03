.section ".text.BlTransition", "ax", %progbits

.balign 4
.globl BlTransition
.type BlTransition, @function
BlTransition:
    # clear icache
    addi t0, zero, 3
    mtcr icachectrl, t0

    # canonicalize rs (T=0, M=0, I=0, U=0)
    mtcr rs, zero

    # jump to entrypoint
    jalr zero, a0, 0
.size BlTransition, . - BlTransition
