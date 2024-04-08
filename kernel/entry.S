        # qemu -kernel loads the kernel at 0x80000000
        # and causes each hart (i.e. CPU) to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x80000000.
.section .text
.global _entry
_entry:
        # set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + (hartid * 4096)
        la sp, stack0
        li a0, 1024*4
        csrr a1, mhartid
        # 这里加1的目的是为了指向每个栈的栈顶
        addi a1, a1, 1
        # 根据不同的hart选择不同的栈
        mul a0, a0, a1
        # 将栈地址放入sp
        add sp, sp, a0
        # jump to start() in start.c
        call start
spin:
        j spin
