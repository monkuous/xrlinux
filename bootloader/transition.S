.section ".text.BlTransition", "ax", %progbits

.balign 4
.globl BlTransition
.type BlTransition, @function
BlTransition:
    # clear icache (assume dcache is already coherent)
    addi t0, zero, 3
    mtcr icachectrl, t0

    # load address of page table into t0
    lui s0, zero, %hi(BlPageTable)
    ori s0, s0, %lo(BlPageTable)

    # jump to entrypoint
    jalr zero, a0, 0
.size BlTransition, . - BlTransition
