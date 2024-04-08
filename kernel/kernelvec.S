        #
        # interrupts and exceptions while in supervisor
        # mode come here.
        #
        # the current stack is a kernel stack.
        # push all registers, call kerneltrap().
        # when kerneltrap() returns, restore registers, return.
        #
.globl kerneltrap
.globl kernelvec
.align 4
kernelvec:
        # make room to save registers.
        # 腾出一些地方暂存数据
        addi sp, sp, -256

        # save the registers.
        # 保存上文，以便从中断函数出来后恢复环境继续运行
        sd ra, 0(sp)
        sd sp, 8(sp)
        sd gp, 16(sp)
        sd tp, 24(sp)
        sd t0, 32(sp)
        sd t1, 40(sp)
        sd t2, 48(sp)
        sd s0, 56(sp)
        sd s1, 64(sp)
        sd a0, 72(sp)
        sd a1, 80(sp)
        sd a2, 88(sp)
        sd a3, 96(sp)
        sd a4, 104(sp)
        sd a5, 112(sp)
        sd a6, 120(sp)
        sd a7, 128(sp)
        sd s2, 136(sp)
        sd s3, 144(sp)
        sd s4, 152(sp)
        sd s5, 160(sp)
        sd s6, 168(sp)
        sd s7, 176(sp)
        sd s8, 184(sp)
        sd s9, 192(sp)
        sd s10, 200(sp)
        sd s11, 208(sp)
        sd t3, 216(sp)
        sd t4, 224(sp)
        sd t5, 232(sp)
        sd t6, 240(sp)

        # call the C trap handler in trap.c
        # 调用c函数，在trap.c中
        call kerneltrap

        # restore registers.
        # 恢复上文，以便回到被中断的地方继续运行
        ld ra, 0(sp)
        ld sp, 8(sp)
        ld gp, 16(sp)
        # not tp (contains hartid), in case we moved CPUs
        ld t0, 32(sp)
        ld t1, 40(sp)
        ld t2, 48(sp)
        ld s0, 56(sp)
        ld s1, 64(sp)
        ld a0, 72(sp)
        ld a1, 80(sp)
        ld a2, 88(sp)
        ld a3, 96(sp)
        ld a4, 104(sp)
        ld a5, 112(sp)
        ld a6, 120(sp)
        ld a7, 128(sp)
        ld s2, 136(sp)
        ld s3, 144(sp)
        ld s4, 152(sp)
        ld s5, 160(sp)
        ld s6, 168(sp)
        ld s7, 176(sp)
        ld s8, 184(sp)
        ld s9, 192(sp)
        ld s10, 200(sp)
        ld s11, 208(sp)
        ld t3, 216(sp)
        ld t4, 224(sp)
        ld t5, 232(sp)
        ld t6, 240(sp)

	# 恢复腾出来的堆栈地址
        addi sp, sp, 256

        # return to whatever we were doing in the kernel.
        # 切换到supervisor模式返回到被中断的地方继续运行
        sret

        #
        # machine-mode timer interrupt.
        #
.globl timervec
.align 4
timervec:
        # start.c has set up the memory that mscratch points to:
        # scratch[0,8,16] : register save area.
        # scratch[24] : address of CLINT's MTIMECMP register.
        # scratch[32] : desired interval between interrupts.

        # 从mscratch里面读取值进来，mscratch在start.c里面设置为了一个数组缓冲区
        csrrw a0, mscratch, a0
        # 数组的前24个字节用来临时存放缓冲区，临时存一下这三个寄存器
        # 因为下面要用了
        sd a1, 0(a0)
        sd a2, 8(a0)
        sd a3, 16(a0)

        # schedule the next timer interrupt
        # by adding interval to mtimecmp.
        # CLINT_MTIMECMP寄存器存放着计数到多少触发中断
        # 第4个字在start.c里面被指明是CLINT_MTIMECMP寄存器地址
        ld a1, 24(a0) # CLINT_MTIMECMP(hart)
        # 第5个字指明是中断间隔
        ld a2, 32(a0) # interval
        # 将CLINT_MTIMECMP寄存器地址的值加载到a3上
        ld a3, 0(a1)
        # 将触发中断的值推迟一个interval以便下一次中断
        add a3, a3, a2
        sd a3, 0(a1)

        # arrange for a supervisor software interrupt
        # after this handler returns.
        # 手动出发一个supervisor级别的软件中断
        # sip就是表明supervisor是否有中断发生的，bit1是SSIP
        li a1, 2
        csrw sip, a1

        ld a3, 16(a0)
        ld a2, 8(a0)
        ld a1, 0(a0)
        csrrw a0, mscratch, a0

        mret
