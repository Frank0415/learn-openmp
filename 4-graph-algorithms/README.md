# SC4050 Lab 3：本机 OpenMP 开发环境

Mac 已安装 Homebrew GCC 15（编译和运行）与 libomp（供 clangd 解析 `omp.h`）。本目录是 Lab 3 的练习模板，算法函数留给你实现。完整实验说明见 [Lab 3 README](/Users/franksair/Documents/26FA/SC4050/Lab/Lab3/README.md)。

## 文件

```text
4-graph-algorithms/
├── floyd_warshall_template.c  # 两个算法函数与性能公式待填
├── triangle_template.c        # 两个算法函数与性能公式待填
├── Makefile                   # 默认使用 gcc-15
├── .clangd                    # Mac 的 omp.h 解析配置
├── .clang-format
├── .vscode/
├── data/                      # Lab 3 的全部七份测试图
└── build/                     # 编译输出，不纳入 Git
```

两份模板保留原有读取、计时、比较和打印逻辑。四个未实现函数都加了 TODO 退出提示，避免空实现误报 `PASS`。实现一个函数后，删除该函数尾部的占位 `(void)`、`fprintf` 和 `exit`。speedup 与 efficiency 仍为占位值，按完整实验说明填写。

## 编译

```bash
cd /Users/franksair/Documents/learn-openmp/4-graph-algorithms
make
```

输出为 `build/floyd` 和 `build/triangle`。只编译一个程序：

```bash
make build/floyd
make build/triangle
```

修改源码后重新执行 `make`，只重建需要更新的二进制。当前未完成模板能编译，但运行会以非零退出码显示 `TODO: implement ...()`，这是预期行为。

## 完成代码后测试

先运行 5 点小图：

```bash
make run-floyd THREADS=1
make run-floyd THREADS=2
make run-floyd THREADS=4

make run-triangle THREADS=1
make run-triangle THREADS=2
make run-triangle THREADS=4
```

两类程序都通过后，再使用大图：

```bash
make run-floyd THREADS=4 FLOYD_GRAPH=data/graph500.txt
make run-triangle THREADS=4 TRIANGLE_GRAPH=data/graph500_tri.txt
```

也可以直接运行二进制：

```bash
OMP_DYNAMIC=FALSE OMP_NUM_THREADS=4 ./build/floyd data/graph5.txt
OMP_DYNAMIC=FALSE OMP_NUM_THREADS=4 ./build/triangle data/graph5_tri.txt
```

`graph5.txt` 的最短路结果：

```text
0   22  52   5  44
29   0  81  34  22
7   29   0  12  51
INF INF INF  0 INF
94  80  87  99   0
```

`graph5_tri.txt` 的三角形数量为 **1**。Floyd 使用 `graph*.txt`，Triangle 使用 `graph*_tri.txt`，不要互换。

## 编辑器

用 VS Code 打开这个目录，安装工作区推荐的 clangd 和 clang-format 扩展。`.clangd` 使用 Apple Clang 加 libomp 的头文件路径；真正的可执行文件通过 Makefile 使用 GCC 的 OpenMP 运行库编译。不要把 Apple Clang 当作支持裸 `-fopenmp` 的 GCC。

源码、数据都可以在终端独立编译，不依赖 VS Code。若打开的是整个 `learn-openmp`，`.clangd` 仍按子目录生效；VS Code 的本目录工作区设置则需打开该目录作为工作区。

## 转到 Pi 或 Aspire2A

传输源码和需要的数据，在目标平台重新编译：

```bash
make CC=gcc
```

如果目标目录里已有 Mac 二进制，先 `make clean`，再 `make CC=gcc`。本机运行用于开发和正确性检查；Pi 和 Aspire2A 的实验时间需在各自平台、按 Slurm/PBS 要求测量。

5000 点 Floyd 和 10000 点三角形枚举的计算量很大，先在小图验证后再运行。数据约 476 MB；本机已有全部文件，五份大图在本目录 `.gitignore` 中排除，两份小图可随代码提交。
