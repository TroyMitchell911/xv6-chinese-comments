# xv6操作系统源码中文注释

本项目源码来自于[xv6-riscv](https://github.com/mit-pdos/xv6-riscv)，仅添加注释和自己的见解，并未修改源代码。

目前项目没有完结，正在持续阅读xv6项目和增加注释，如果您有什么不懂的地方欢迎提交Issue，很高兴能帮到您。

如果您有任何指正我的地方，欢迎提交PR，如果是想增加我还没有写的地方的注释，那还是算了，因为我想自己读一下源码，感谢您的理解。

# 译者

- TroyMitchell

# 更新进度

## 2024-04-08

- entry.S全注释
- start.c全注释
- kernelvec.S全注释
- trap.c部分注释
- spinlock.c部分注释
- proc.c部分注释
- 理清楚了xv6中是如何去进行的调度进程

## 2024-04-09

- spainlock.c全注释
- swtch.S全注释
- 搞清楚了进程如何切换的（还没搞清楚下文是哪里来的）

# TODO

- [ ] trap.c全注释
- [ ] spinlock.c全注释
- [ ] proc.c全注释
- [x] 注释切换进程的部分
- [ ] 搞清楚切换进程的下文是哪里来的
- [ ] 注释新建一个进程的部分