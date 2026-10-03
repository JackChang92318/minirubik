.text
.globl main
main:
    li t0, 1048576
    sub sp, sp, t0
    mv t1, sp
    srli t2, t0, 2
    li t3, 1

write_loop:
    sw t3, 0(t1)
    addi t1, t1, 4
    addi t2, t2, -1
    bne t2, zero, write_loop

    add sp, sp, t0
    li a7, 10
    ecall
