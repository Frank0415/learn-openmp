# SC4050 Lab 5：MPI Stencil 练习

原始 `stencil_skeleton.c` 保留全部五个 TODO，留给你完成。完整中文任务与验收说明见 [26FA 的 Lab 5 README](https://github.com/Frank0415/26FA/tree/main/SC4050/Lab/Lab5)。本目录的 `Lab5.pdf` 为原始讲义。

## 编译

本机已安装 Homebrew Open MPI，`mpicc` 包装 Apple Clang。沿用上一实验的 `-O0` 编译约定；Lab 5 手册没有规定优化级别，两版比较时保持一致。

```bash
cd /Users/franksair/Documents/learn-openmp/6-mpi-stencil
make -B
```

输出为 `build/stencil`。编译通过只证明环境和语法可用，原始骨架不能直接运行：`dims` 仍为零，列 datatype 和请求也尚未初始化。完成 TODO 1–5 后再执行下面的命令。

## 完成 TODO 后测试

```bash
make run RANKS=1
make run RANKS=4
make run RANKS=8
```

默认参数为 `N=32 MAX_ITERS=5000 EPSILON=1e-4`。例如：

```bash
make run RANKS=4 N=64 MAX_ITERS=20000 EPSILON=1e-5
```

如本机 slot 不足，功能测试可加 `MPIEXEC_FLAGS="--oversubscribe --bind-to none"`。正式 placement 比较使用足够资源，保持进程总数一致。

检查不同 ranks 下的坐标、四个邻居、迭代次数、全局最大变化和各 rank 的平均温度。不能仅凭程序退出码判断任务完成；原模板未收敛时也会正常返回。

## Task 4

完成工作版本后创建 `stencil_blocking.c`，把 `MPI_Waitall` 移到任何网格更新之前，随后一次更新整个局部自有区域。

```bash
make blocking
make run RANKS=4 N=256 MAX_ITERS=200000
make run-blocking RANKS=4 N=256 MAX_ITERS=200000
```

Makefile 不提前创建空的 blocking 文件；不影响你先完成第一份模板。Pi 上仍可使用 `make`；其他 MPI 平台按现场环境指定 `MPICC`、`MPIEXEC` 和 `MPIEXEC_FLAGS`。

## 验收与测验

实验四项分别为 0.5、1.5、1.5、1.5 分。Task 4 要测单物理节点和至少两个物理节点，单台 Mac 的测试不替代跨节点结果。另有 30 分钟闭卷 Quiz，覆盖 Weeks 8–10，占 5 分。
