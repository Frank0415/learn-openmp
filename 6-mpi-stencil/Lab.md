# SC4050 Lab 4–5：MPI 代码理解与 Quiz 复习

这份笔记集中解释 Lab 4 四个 MPI 程序与 Lab 5 stencil 求解器。C 片段用于理解数据流、参数和同步位置，不包含编译、启动、提交作业或现场展示流程。

Lab 4 手册写的是“该周没有单独 quiz”；Lab 5 手册明确规定：**30 分钟闭卷纸笔 Quiz，范围 Weeks 8–10**。因此 Lab 4 的基础内容仍属于本次复习范围。最后的题型与优先级是根据实验和 slides 整理的复习判断，不是已知试卷。

## 资料与阅读顺序

下文页码均指 **PDF 实际页码**，不是页脚编号；部分 slides 有重复动画页。

| 资料 | 对应内容 | 重点页 |
|---|---|---|
| [Lab 4 手册](/Users/franksair/Documents/learn-openmp/5-mpi-fundamentals/handout.pdf) | Ping-Pong、Collector、Overlap、手写环归约 | PDF 1–3 |
| [Lab 5 手册](/Users/franksair/Documents/learn-openmp/6-mpi-stencil/Lab5.pdf) | Cartesian、vector、halo、Jacobi、overlap | PDF 1–3 |
| [Week 8：Distributed Programming Model](</Users/franksair/Documents/26FA/SC4050/Lecture/Week08-Distributed Programming Model.pdf>) | 消息成本、SPMD、消息匹配、阻塞、死锁、非阻塞 | PDF 7–9、16–34 |
| [Week 9：Collectives and Decomposition](</Users/franksair/Documents/26FA/SC4050/Lecture/Week09-Collective Communication and Data Decomposition.pdf>) | collective 语义、归约、树成本、数据划分 | PDF 7–25、29–38 |
| [Week 9b：Matrix–Matrix Multiplication](</Users/franksair/Documents/26FA/SC4050/Lecture/Week09b-Matrix-Matrix Multiplication.pdf>) | 行分块、广播、二维分块与 Cannon | PDF 4–15 |
| [Week 10：Advanced MPI and Performance](</Users/franksair/Documents/26FA/SC4050/Lecture/Week10-Advanced MPI and Performance.pdf>) | datatype、拓扑、重叠、stencil、混合线程支持 | PDF 5–33 |

建议先读共同基础，再读 Lab 4、Lab 5，最后用 Quiz 问答检查自己能否解释代码。

## 一、两个实验共用的 MPI 基础

### 1. rank、进程和节点不是同一个概念

MPI 进程拥有独立的地址空间。相同名字的变量在不同进程中是不同对象，不能靠一个进程给变量赋值来更新另一进程的变量。

`rank` 是进程在某个 communicator 内的编号；节点是物理机器。一台机器可以容纳多个 rank。MPI 也可以在同一台机器上通信，不是只有跨机器才用 MPI。

SPMD 表示所有进程执行同一个程序，但可根据 rank 走不同分支。代码中的：

~~~c
if (rank == 0) {
    /* Collector/root 的工作 */
} else {
    /* Worker 的工作 */
}
~~~

并不是创建进程；进程由启动环境创建，分支只是决定现有进程做什么。

### 2. 初始化与 communicator

~~~c
MPI_Init(&argc, &argv);

int rank, size;
MPI_Comm_rank(MPI_COMM_WORLD, &rank);
MPI_Comm_size(MPI_COMM_WORLD, &size);

/* MPI 计算与通信 */

MPI_Finalize();
~~~

- `MPI_Init` 初始化 MPI 环境；`MPI_Finalize` 结束它。
- `MPI_COMM_WORLD` 通常包含本次启动的全部 MPI 进程。
- `MPI_Comm_rank` 返回当前 communicator 内的编号；`MPI_Comm_size` 返回成员数量。
- 同一进程在两个 communicator 中可以有不同 rank，Lab 5 的 `cart_rank` 就需要特别注意。
- `MPI_Finalize` 不能代替 `MPI_Waitall`；程序仍应完成已发布的非阻塞请求，再释放缓冲区并结束 MPI。

### 3. 消息为什么能匹配

~~~c
MPI_Send(&value, 1, MPI_INT, destination, tag, comm);

MPI_Recv(&value, 1, MPI_INT, source, tag, comm, &status);
~~~

发送描述的是“把什么发给谁”；接收描述的是“愿意从谁接收哪类消息”。

| 参数 | 含义 | 常见误解 |
|---|---|---|
| `buf` | 本进程内的数据地址 | 不是发送一个供远端解引用的地址 |
| `count` | datatype 的实例数量 | 通常不是字节数；使用 `MPI_BYTE` 时才按字节数 |
| `datatype` | 数据的类型和内存布局 | 接收缓冲区要容纳实际匹配的数据 |
| `source/destination` | 对方在该 communicator 内的 rank | 不是物理节点编号 |
| `tag` | 程序约定的消息类别 | 不等于 source，除非程序刻意这样设置 |
| `comm` | 通信范围和独立的消息上下文 | 同样 source/tag、不同 communicator 不会相互匹配 |

匹配依赖来源、目标、tag 和 communicator。类型与接收容量也要兼容，但不是靠 datatype 名字为消息选择接收者。`MPI_Recv` 的 count 是接收容量，收到较短消息并不意味着未写满部分自动补零。

`MPI_ANY_SOURCE`、`MPI_ANY_TAG` 是接收侧通配符；完成后从 `MPI_Status` 获取实际来源和 tag。

### 4. “Blocking”到底保证什么

**`MPI_Send` 返回：发送缓冲区可以复用；不保证接收方应用已经处理了消息。**

小消息可能先复制到 MPI 内部缓冲区，发送很快返回；大消息可能采用 rendezvous，等待接收方准备好。正确程序不能依赖“小消息有缓冲，所以双方先 Send 没事”。

**`MPI_Recv` 返回：匹配消息已经写入本地接收缓冲区，可以读取。**

下面的对称顺序有死锁风险：

~~~c
/* 两个进程都先执行这一条 */
MPI_Send(sendbuf, n, MPI_DOUBLE, partner, 0, comm);
MPI_Recv(recvbuf, n, MPI_DOUBLE, partner, 0, comm, &status);
~~~

双方都可能等对方发布接收，结果谁也走不到下一行。解决方法包括让两边采用相反顺序、使用 `MPI_Sendrecv`，或者先发布非阻塞请求再等待。`Sendrecv` 消除了这个交换顺序导致的循环等待，但 source、tag 和调用次数仍需正确匹配。

### 5. 非阻塞的缓冲区规则

~~~c
MPI_Request reqs[2];
MPI_Irecv(recvbuf, n, MPI_DOUBLE, partner, 0, comm, &reqs[0]);
MPI_Isend(sendbuf, n, MPI_DOUBLE, partner, 0, comm, &reqs[1]);

/* 这里可做不依赖 recvbuf、也不修改 sendbuf 的工作 */

MPI_Waitall(2, reqs, MPI_STATUSES_IGNORE);
~~~

- `Irecv/Isend` 返回表示请求已发布，不表示数据已经传完。
- 发送完成前，不修改、释放或复用发送区域。
- 接收完成前，不读取或改写接收区域。
- 只读发送区域通常可以，但要确保不与正在接收的区域重叠；Lab 5 内部更新读取旧数据、写另一张网格就是这种用法。
- `MPI_Waitall` 等这批请求全部完成。它不是全 communicator 的 barrier。
- `MPI_Test` 可检查一个请求是否完成；只有完成标志为真，才能按完成状态使用缓冲区。
- `MPI_Barrier` 不能替代请求完成操作，也不会自动修复错误的 tag。

## 二、Lab 4：Ping-Pong 与通信成本模型

对应 `pingpong_skeleton.c`；主要联系 Week 8 PDF 7–9、25–34。

### 1. 一次 round trip 做了什么

~~~c
if (rank == 0) {
    MPI_Send(buf, n, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
    MPI_Recv(buf, n, MPI_BYTE, 1, 1, MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);
} else {
    MPI_Recv(buf, n, MPI_BYTE, 0, 0, MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);
    MPI_Send(buf, n, MPI_BYTE, 0, 1, MPI_COMM_WORLD);
}
~~~

rank 0 发出数据，rank 1 收到后原样返回，rank 0 再收回。同一 rank 的两次本地计时可以得到完整 RTT，不需要把两台机器的时钟相减。

tag 0 表示去程，tag 1 表示回程。rank 0 的 send 与 rank 1 的 recv 匹配；rank 1 的 send 与 rank 0 的 recv 匹配。两边不是都先阻塞发送，因此没有前面那个对称等待问题。

`MPI_BYTE` 的一个元素就是一个字节，所以这里的 n 才直接对应成本模型的消息字节数。

### 2. 为什么预热、重复、除以 2

本地模板使用 `WARMUP=5`、`TRIALS=50` 和七种消息大小。预热不计入正式结果，减少首次连接、协议初始化等一次性成本；重复测量后取平均，减少单次调度波动的影响。

计时的核心关系是：

~~~c
double total_rtt_seconds = 0.0;

for (int t = 0; t < TRIALS; ++t) {
    if (rank == 0) {
        double start = MPI_Wtime();
        MPI_Send(buf, n, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
        MPI_Recv(buf, n, MPI_BYTE, 1, 1, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);
        total_rtt_seconds += MPI_Wtime() - start;
    } else {
        MPI_Recv(buf, n, MPI_BYTE, 0, 0, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);
        MPI_Send(buf, n, MPI_BYTE, 0, 1, MPI_COMM_WORLD);
    }
    MPI_Barrier(MPI_COMM_WORLD);
}

double one_way_us = total_rtt_seconds * 1e6 / (2.0 * TRIALS);
~~~

这个片段中的最后一个数只在 rank 0 上使用。`MPI_Wtime` 返回秒，乘 `1e6` 转为微秒，除 `TRIALS` 得到每轮平均，再除以 2 估计单向成本。这是往返对称条件下的估计，不是独立测量单程传播时间。

这里 barrier 在每轮被测 send/recv 区间之外，不直接计入 `total_rtt_seconds`。barrier 只保证所有进程进入了同步点，不保证 stdout 按 rank 顺序显示。

### 3. 当前源码里要识别的单位问题

当前 `pingpong_skeleton.c` 把 `total_rtt / TRIALS` 存进 `avg_rtt_us`，但没有乘 `1e6`，随后打印 `us`。变量名不会改变单位：那个值实际仍是秒。

因此，使用上面“先把单向时间换成微秒”的写法后，下面的微秒公式才匹配。若始终保留秒，也能计算，但 α、β、带宽换算必须全部按秒处理。不能拿“秒的数值”当“微秒的数值”。

数组实际为 8、64、512、4096、32768、262144、1048576 bytes；源码说“doubling”的注释不准确，以实际数组为准。

### 4. α、β、带宽与 crossover

成本模型：

$$
T(n)=\alpha+n\beta.
$$

- `α`：每条消息的固定启动成本，单位 µs。
- `β`：每字节的传输成本，单位 µs/byte；斜率是 β，带宽是其倒数。
- n：消息字节数，不是 double 的个数。

用最小和最大消息测得的两个点拟合：

~~~c
double beta = (t_large_us - t_small_us) / (n_large - n_small);
double alpha = t_small_us - n_small * beta;
double bandwidth_GBps = 1e-3 / beta;
double crossover_bytes = alpha / beta;
~~~

`bandwidth_GBps` 按十进制 GB/s 计算：`1/beta` 先是 byte/µs，乘 `1e6` 得 byte/s，再除 `1e9`。所以最终系数为 `1e-3`，不是 `1e6`。β 应为正；若拟合为负，先检查单位、噪声和模型适用性，不把负带宽当成真实链路性质。

crossover 满足 `α=nβ`，即 `n*=α/β`。它表示启动成本与传输成本相等的消息大小，不等于 MPI 实现内部的 eager/rendezvous 阈值。

例：1000 bytes 用 12 µs，11000 bytes 用 32 µs：

- `β=(32−12)/(11000−1000)=0.002 µs/byte`。
- `α=12−1000×0.002=10 µs`。
- 带宽 `0.001/0.002=0.5 GB/s`。
- crossover 为 `10/0.002=5000 bytes`。

同一总数据量拆成 M 条消息，模型成本从 `α+Bβ` 变成 `Mα+Bβ`，多付了 `(M−1)α`。这解释为什么小消息应减少次数，大消息应关心总字节数。模型是近似值，实际还受拥塞、复制和协议切换影响。

## 三、Lab 4：Collector、通配符与消息验证

对应 `collector_skeleton.c`；主要联系 Week 8 PDF 22–24。

### 1. Worker 的协议

~~~c
int payload = rank * rank;
MPI_Send(&payload, 1, MPI_INT, 0, rank, MPI_COMM_WORLD);
~~~

每个非零 rank 发送一条消息给 rank 0。payload 为自己的 rank 平方，tag 等于自己的 rank。随机 sleep 用来打乱到达顺序，不负责保证 MPI 正确性。

不同发送者之间没有统一到达顺序。收集器不能把第 i 次接收就当成“来自 rank i”。

### 2. 接收之前不知道来源，接收之后知道实际来源

~~~c
int payload;
MPI_Status status;

MPI_Recv(&payload, 1, MPI_INT,
         MPI_ANY_SOURCE, MPI_ANY_TAG,
         MPI_COMM_WORLD, &status);

int true_source = status.MPI_SOURCE;
int true_tag = status.MPI_TAG;
~~~

通配符描述的是接收条件，`status` 描述的是本次实际匹配到的消息。`MPI_ANY_SOURCE` 不是实际 sender，不能存进数组作为下标；接收完成后才读取 `status.MPI_SOURCE`。

如果使用 `MPI_STATUS_IGNORE`，就放弃了这里需要的实际 source/tag 信息。tag 与 payload 是不同的东西：tag 用于消息分类匹配，payload 是消息正文。

### 3. 验证的三个维度

~~~c
if (true_source < 1 || true_source >= size) {
    all_ok = 0;
} else if (heard_from[true_source] ||
           payload != true_source * true_source ||
           true_tag != true_source) {
    all_ok = 0;
} else {
    heard_from[true_source] = 1;
}
~~~

`heard_from` 初始全零，用于记录已收到哪些 worker 的消息。只检查 payload 不够：重复接收同一 worker 的正确 payload，仍可能漏掉另一个 worker。

收满 `size−1` 条后，再检查 `heard_from[1..size−1]` 全部为 1，才能说明没有遗漏。这个协议中 rank 0 不发送给自己，所以不能把它计为需要收集的 worker。

## 四、Lab 4：通信计算重叠

对应 `overlap_skeleton.c`；主要联系 Week 8 PDF 31–32、Week 10 PDF 20–23。

### 1. 环邻居与缓冲区

~~~c
int right = (rank + 1) % size;
int left = (rank - 1 + size) % size;
~~~

最后一个 rank 的 right 回到 0，rank 0 的 left 回到 `size−1`，形成环。加 `size` 避免 C 的负数取余得到负 rank。

`send_buf` 是发给两个邻居的本地数据；`recv_from_left` 与 `recv_from_right` 是两块不同的接收区域，防止两个尚未完成的接收同时写同一缓冲区。

模板的 `BLOCK_SIZE=2000000` 是 double 数量，不是字节数。按 8-byte double 计算，每个邻居方向约 16 MB。

### 2. 顺序方案：通信完成后才计算

~~~c
MPI_Sendrecv(send_buf, BLOCK_SIZE, MPI_DOUBLE, right, 0,
             recv_from_left, BLOCK_SIZE, MPI_DOUBLE, left, 0,
             MPI_COMM_WORLD, MPI_STATUS_IGNORE);

MPI_Sendrecv(send_buf, BLOCK_SIZE, MPI_DOUBLE, left, 1,
             recv_from_right, BLOCK_SIZE, MPI_DOUBLE, right, 1,
             MPI_COMM_WORLD, MPI_STATUS_IGNORE);

sink += burn_cpu(COMPUTE_ITERS);
~~~

第一条沿 right 方向发送，同时从 left 接收；第二条反向交换。每次 `Sendrecv` 的发送对象和接收来源可以不同，这正适合环。

两次交换都完成后才进入 `burn_cpu`，所以通信时间与计算时间顺序相加。该函数只做本地数学运算，不依赖刚收到的数组。

### 3. 重叠方案：发布、计算、等待

~~~c
MPI_Request reqs[4];
MPI_Irecv(recv_from_left, BLOCK_SIZE, MPI_DOUBLE,
          left, 0, MPI_COMM_WORLD, &reqs[0]);
MPI_Irecv(recv_from_right, BLOCK_SIZE, MPI_DOUBLE,
          right, 1, MPI_COMM_WORLD, &reqs[1]);
MPI_Isend(send_buf, BLOCK_SIZE, MPI_DOUBLE,
          right, 0, MPI_COMM_WORLD, &reqs[2]);
MPI_Isend(send_buf, BLOCK_SIZE, MPI_DOUBLE,
          left, 1, MPI_COMM_WORLD, &reqs[3]);

sink += burn_cpu(COMPUTE_ITERS);

MPI_Waitall(4, reqs, MPI_STATUSES_IGNORE);
~~~

四个请求描述的是与顺序版相同的传输，只调整等待位置。`burn_cpu` 放在发布和等待之间，因此 MPI 有机会在 CPU 做独立工作时推进通信。

不要把 `Waitall` 放在 `burn_cpu` 前，否则又成了“等通信、再计算”。也不能为了省时间删掉 `Waitall`：后续仍要知道接收是否完成，以及发送缓冲区何时可复用。

`sink` 把计算结果带到后续可观察路径，减少计算被当作无用代码删除的可能。比较两版时要保持工作量一致，而不是给重叠版少算一些。

### 4. 性能模型与 savings

$$
T_{\mathrm{seq}}\approx T_{\mathrm{comm}}+T_{\mathrm{comp}},
\qquad
T_{\mathrm{overlap,ideal}}\approx\max(T_{\mathrm{comm}},T_{\mathrm{comp}}).
$$

$$
\mathrm{savings}=\frac{T_{\mathrm{seq}}-T_{\mathrm{overlap}}}
{T_{\mathrm{seq}}}\times100\%.
$$

例如通信 50 µs、独立计算 200 µs，顺序约 250 µs，理想重叠约 200 µs，节省 20%，加速比是 1.25×，不是节省 25%。

实际耗时还包括请求发布、等待、内存复制与同步成本。非阻塞不保证后台通信一定持续推进；实现可能需要进入 MPI 才有进展。计算太短时隐藏不了通信，计算太长时收益比例也可能小。同机通信与网络通信成本不同，不能把两者混作一个性能结论。

## 五、Lab 4：手写 Ring All-Reduce

对应 `allreduce_skeleton.c`；联系 Week 9 PDF 18–24 的 Reduce/Allreduce。

### 1. 三个变量不能混淆

~~~c
double local_value = rank + 1.0;
double running_total = local_value;
double relay = local_value;
~~~

- `local_value`：本进程最初贡献的数据，保持不变。
- `running_total`：已经收集到的数据之和。
- `relay`：本轮要向右转发的数据，不一定来自自己。

每个 rank 已经知道自己的值，还缺其他 `P−1` 个值，所以执行 `P−1` 轮。

### 2. 正确的环传递

~~~c
for (int step = 0; step < size - 1; ++step) {
    double incoming;
    MPI_Sendrecv(&relay, 1, MPI_DOUBLE, right, 0,
                 &incoming, 1, MPI_DOUBLE, left, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    running_total += incoming;
    relay = incoming;
}
~~~

**每个 rank 都参与每一轮。** 每轮从左边收到一个原始贡献值、加到总和，再在下一轮把它继续向右转发。

必须用 `relay=incoming`。若每轮都重新发送自己的 `local_value`，邻居只会反复收到同一个值；若把 `running_total` 当 relay，则会把已经加过的数据反复传播并重复计数。

P=4 的手算过程：

| 阶段 | 各 rank 本轮收到的值，按 0、1、2、3 排列 | 各 rank 的 running_total |
|---|---|---|
| 初始 | 尚未接收 | 1、2、3、4 |
| 第 1 轮 | 4、1、2、3 | 5、3、5、7 |
| 第 2 轮 | 3、4、1、2 | 8、7、6、9 |
| 第 3 轮 | 2、3、4、1 | 10、10、10、10 |

期望总和为：

$$
\sum_{r=0}^{P-1}(r+1)=\frac{P(P+1)}2.
$$

### 3. 当前源码的循环为什么不对

当前文件使用 `for(i=0;i<size;i++)`，内部只有 `if(i==rank)` 时才调用通信。循环变量是各进程的本地变量，不是全局轮次调度器；每个进程最终只参与了一次交换。

因此它通常只得到“自己的值 + 左邻居的值”，不是全局总和。应理解并采用上一节的“所有 rank 每轮都交换，执行 P−1 轮”的结构。这里指出的是代码含义，本文没有修改 C 文件。

### 4. 与库里的 Allreduce 有什么关系

本实验用点对点通信实现“每个 rank 都得到全局和”的语义。这个标量转发算法每轮传一个 double，总共 `P−1` 轮，简单模型约为 `(P−1)(α+8β)`。

它不等于生产库中用于大向量的两阶段 ring allreduce。后者可能分成 reduce-scatter 与 allgather，使用不同的数据块和通信轮数。不能把本题直接套成“所有 ring allreduce 都只需 P−1 轮”。

MPI 库会根据数据量、rank 数和硬件选择树、环或其他算法。小标量归约可能更适合减少延迟轮数的树结构；手写环主要用于理解通信和累加过程。

## 六、Lab 5：二维 Jacobi Stencil 与 Halo

把二维网格想成一块平板：外侧温度固定为 100，内部最初为 0。每轮让一个格子的新温度等于上下左右四个旧温度的平均值。这个过程叫 Jacobi 迭代，重复后温度逐渐趋近 100。

每个 MPI 进程负责一块数据，进程之间没有共享地址空间。计算自己这块边缘格子时，需要先从邻居进程取得相邻温度，这就是 halo exchange（边界数据交换）。

骨架里的变量对应关系：

| 变量 | 含义 |
|---|---|
| `rank`、`size` | `MPI_COMM_WORLD` 中的进程编号和总进程数 |
| `dims[0]`、`dims[1]` | 进程网格的行数和列数，不是温度网格大小 |
| `coords[0]`、`coords[1]` | 当前进程在二维进程网格里的位置 |
| `global_n` | 骨架中待更新的全局自有网格边长，物理边界放在外侧 halo |
| `local_rows`、`local_cols` | 当前进程负责的行数和列数 |
| `W`、`H` | 包含 halo 后，本地数组的实际宽度和高度 |
| `old_grid`、`new_grid` | 本轮读取的旧温度、计算得到的新温度 |
| `IDX(i,j)` | 把二维坐标换成线性下标 `i*W+j` |
| `epsilon` | 相邻两轮温度最大变化的停止阈值，不是距最终 100 的误差 |

例如 4 ranks 分成 2×2，`global_n=32`，每个进程负责 16×16 个格子，但实际分配 18×18 个位置。第 0 行/列和最后一行/列是 halo，自有格子的下标范围为 `1..16`。

老师提供的单格更新函数是：

~~~c
static inline double jacobi(double* g, int i, int j, int W) {
    return 0.25 * (g[(i - 1) * W + j] + g[(i + 1) * W + j] +
                   g[i * W + (j - 1)] + g[i * W + (j + 1)]);
}
~~~

其中四项依次是上、下、左、右邻居，乘 0.25 就是取平均；`i*W+j` 将二维坐标转换为连续数组中的下标。“五点 stencil”指中心位置与四邻居构成的依赖形状，本题公式并不把中心旧值再加进平均。

整个迭代的顺序是：发布邻居通信 → 计算内部 → 等通信结束 → 计算外圈 → 求全局最大变化 → 交换新旧指针 → 判断是否停止。

### 1. Cartesian 拓扑：把进程组织成网格

这段代码在计算局部网格大小之前执行。先让 MPI 确定进程网格，再查询当前进程的 Cartesian 身份和邻居。

~~~c
int dims[2] = {0, 0};
int periods[2] = {0, 0};
int coords[2];
int up, down, left, right;
int cart_rank;
MPI_Comm cart_comm;

MPI_Dims_create(size, 2, dims);
MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 1, &cart_comm);
MPI_Comm_rank(cart_comm, &cart_rank);
MPI_Cart_coords(cart_comm, cart_rank, 2, coords);
MPI_Cart_shift(cart_comm, 0, 1, &up, &down);
MPI_Cart_shift(cart_comm, 1, 1, &left, &right);

~~~

逐项理解：

- `MPI_Dims_create(size,2,dims)` 把进程数分成两个维度。两个初值为 0，表示交给 MPI 选择；4 ranks 通常得到 2×2，8 ranks 通常得到 4×2。
- `MPI_Cart_create` 创建带坐标关系的新 communicator。`periods={0,0}` 表示边缘不环绕，右边界不会连接到左边界。
- 参数 `reorder=1` 允许重排 rank。因此 `cart_rank` 可能不同于原来的 `rank`，不能把 world rank 直接拿去查 Cartesian 坐标。
- `MPI_Cart_shift(...,0,1,&up,&down)` 沿进程网格的第 0 维移动一步，返回负方向和正方向邻居；第 1 维同理得到左右邻居。这些编号属于 `cart_comm`。
- 没有邻居时返回 `MPI_PROC_NULL`。发送/接收这个特殊进程不会真的交换数据；接收缓冲区保持不变，因此骨架预设的物理边界温度能保留。

骨架接着检查 `global_n` 能否被两个维度整除，再计算每个 rank 的局部大小。先建立拓扑再执行整除，避免拿初始的 0 做除数。

### 2. 列 datatype：用布局描述代替手动打包

先声明类型句柄，再构造和提交类型：

~~~c
MPI_Datatype col_type;
MPI_Type_vector(local_rows, 1, W, MPI_DOUBLE, &col_type);
MPI_Type_commit(&col_type);
~~~

`MPI_Type_vector` 的参数分别表示：

| 参数 | 本例取值 | 含义 |
|---|---|---|
| count | `local_rows` | 这一列有多少个数据块 |
| blocklength | `1` | 每块只有一个温度值 |
| stride | `W` | 相邻块起点相隔多少个 `MPI_DOUBLE`，不是字节数 |
| oldtype | `MPI_DOUBLE` | 温度的基本类型 |

一行在 C 数组里连续，所以行直接发送 `local_cols` 个 double。一列的位置是 `i*W+j`，下一行同列要跨过 `W` 个 double；派生类型让 MPI 按这个布局读取/写入，不用手动打包。`MPI_Type_commit` 让类型可用于通信，末尾的 `MPI_Type_free` 释放类型资源，不会释放网格数据。

### 3. Halo exchange：四个接收、四个发送

每轮先准备 `MPI_Request reqs[8]`，然后发布下面八个请求。

~~~c
enum { TAG_UP = 10, TAG_DOWN = 11, TAG_LEFT = 12, TAG_RIGHT = 13 };

MPI_Irecv(&old_grid[IDX(0, 1)], local_cols, MPI_DOUBLE,
          up, TAG_DOWN, cart_comm, &reqs[0]);
MPI_Irecv(&old_grid[IDX(local_rows + 1, 1)], local_cols, MPI_DOUBLE,
          down, TAG_UP, cart_comm, &reqs[1]);
MPI_Irecv(&old_grid[IDX(1, 0)], 1, col_type,
          left, TAG_RIGHT, cart_comm, &reqs[2]);
MPI_Irecv(&old_grid[IDX(1, local_cols + 1)], 1, col_type,
          right, TAG_LEFT, cart_comm, &reqs[3]);

MPI_Isend(&old_grid[IDX(1, 1)], local_cols, MPI_DOUBLE,
          up, TAG_UP, cart_comm, &reqs[4]);
MPI_Isend(&old_grid[IDX(local_rows, 1)], local_cols, MPI_DOUBLE,
          down, TAG_DOWN, cart_comm, &reqs[5]);
MPI_Isend(&old_grid[IDX(1, 1)], 1, col_type,
          left, TAG_LEFT, cart_comm, &reqs[6]);
MPI_Isend(&old_grid[IDX(1, local_cols)], 1, col_type,
          right, TAG_RIGHT, cart_comm, &reqs[7]);
~~~

每个调用都包含：缓冲区起点、数量、类型、对方 rank、tag、communicator 和请求句柄。`Irecv` 发布接收，`Isend` 发布发送；函数返回不代表传输已完成，请求句柄交给后面的 `MPI_Waitall`。

这里的 tag 以**发送者发送方向**命名，因此从上方收到的是上方进程向下发送的 `TAG_DOWN`。四组匹配关系是：

| 本进程接收位置 | 对方 | 对方发送方向 | 接收 tag |
|---|---|---|---|
| 顶部 halo `(0,1)` | `up` | 向下 | `TAG_DOWN` |
| 底部 halo `(local_rows+1,1)` | `down` | 向上 | `TAG_UP` |
| 左侧 halo `(1,0)` | `left` | 向右 | `TAG_RIGHT` |
| 右侧 halo `(1,local_cols+1)` | `right` | 向左 | `TAG_LEFT` |

发送数据来自自有边缘，接收数据进入 halo。它们都属于 `old_grid`：本轮更新必须基于同一代的温度。`new_grid` 用来存结果，因此不会改写尚未完成发送的数据。

四角 halo 不参与五点平均，不需要对角线邻居通信。即使某个邻居是 `MPI_PROC_NULL`，也照样调用并保存请求，所以后面总能等待完整的八个请求。

### 4. Overlap：只提前计算不依赖 halo 的格子

发布八个请求之后执行下面的更新顺序，再进入误差计算：

~~~c
for (int i = 2; i < local_rows; ++i) {
    for (int j = 2; j < local_cols; ++j) {
        new_grid[IDX(i, j)] = jacobi(old_grid, i, j, W);
    }
}

MPI_Waitall(8, reqs, MPI_STATUSES_IGNORE);

for (int i = 1; i <= local_rows; ++i) {
    for (int j = 1; j <= local_cols; ++j) {
        if (i == 1 || i == local_rows || j == 1 || j == local_cols) {
            new_grid[IDX(i, j)] = jacobi(old_grid, i, j, W);
        }
    }
}
~~~

内部格子的上下左右都属于本进程，计算时不读取正在接收的 halo，所以可以在等待通信期间处理。它可能读取发送中的自有边缘，但只读不会改变发送内容。

`MPI_Waitall` 保证八个请求完成：halo 已经可读，发送缓冲区也不再被这轮发送占用。之后才能计算依赖 halo 的外圈。

外圈条件用逻辑“或”，一次遍历中每个格子只更新一次，包括四个角点。`local_rows` 或 `local_cols` 为 1、2 时，内部循环会自然为空，外圈仍覆盖全部自有格子。

`jacobi()` 已由老师写好：它根据 `old_grid` 的四邻居返回平均值，不会修改输入，也不包含中心格子本身的值。

### 5. Allreduce：所有 rank 得到同一个停止依据

全部自有格子更新后、交换指针之前，先计算变化并做全局归约：

~~~c
double local_max_diff = 0.0;
for (int i = 1; i <= local_rows; ++i) {
    for (int j = 1; j <= local_cols; ++j) {
        double diff = fabs(new_grid[IDX(i, j)] - old_grid[IDX(i, j)]);
        if (diff > local_max_diff) {
            local_max_diff = diff;
        }
    }
}

MPI_Allreduce(&local_max_diff, &global_max_diff, 1,
              MPI_DOUBLE, MPI_MAX, cart_comm);
~~~

`local_max_diff` 是当前 rank 自有格子的最大变化；不包含 halo。`fabs()` 取 double 的绝对值，不能用整数的 `abs()` 代替。

`MPI_Allreduce` 收集所有 rank 的这个值，使用 `MPI_MAX` 取全局最大值，并让所有 rank 都收到结果。这不是求平均，也不是求和。每个进程都有相同的停止依据，避免部分进程退出、其他进程继续等待 halo 通信。

随后交换两张网格的角色，并判断是否停止：

~~~c
double* tmp = old_grid;
old_grid = new_grid;
new_grid = tmp;

if (global_max_diff < epsilon) break;
~~~

这里只交换两个指针，不复制整张网格。交换后 `old_grid` 指向刚算好的新温度，最终平均值也从它计算。误差在交换前计算，停止判断在交换后执行，因此即使这一轮收敛，最终输出仍对应最后一次更新。

骨架在两份网格上都设置了物理边界为 100，所以指针交换不会丢失固定边界。非物理边界的 halo 每轮重新通信，上一轮留下的旧值会被覆盖。

### 6. 释放类型、数组与 communicator

当通信与结果处理结束后，释放派生类型、两份网格与先前创建的 communicator：

~~~c
MPI_Type_free(&col_type);
free(old_grid);
free(new_grid);
MPI_Comm_free(&cart_comm);
~~~

类型句柄、数组内存和 communicator 是三种不同资源，分别释放。Cartesian communicator 在计算和结果输出完成后才不再使用；这里不释放 `MPI_COMM_WORLD`。随后由 `MPI_Finalize` 结束 MPI。


### 7. 不重叠版本：同样的数据，改变等待位置

`stencil_blocking.c` 保留同样的八个非阻塞请求，但立刻等待，再一次更新所有自有格子。拓扑、数据量、边界、停止条件保持一致：

~~~c
MPI_Waitall(8, reqs, MPI_STATUSES_IGNORE);

for (int i = 1; i <= local_rows; ++i) {
    for (int j = 1; j <= local_cols; ++j) {
        new_grid[IDX(i, j)] = jacobi(old_grid, i, j, W);
    }
}
~~~

这个版本先等全部通信结束，再算所有自有格子，因此没有通信计算重叠。依然使用非阻塞调用来发布请求，符合手册的 blocking 对照定义。不要在旧的内部循环之前只加一次等待、却漏删后面原有的等待和更新段。


### 8. 为什么单节点和跨节点的收益不同

不重叠版本大致付出 `T_comm + T_interior + T_outer`；理想的重叠版本希望接近 `max(T_comm,T_interior) + T_outer`。全局误差归约和其他开销两版都有。

实际是否变快取决于内部计算能否覆盖通信时间，以及 MPI 是否在计算期间推进数据传输。`MPI_Isend/Irecv` 提供重叠机会，但不会保证一定更快。单节点主要使用同机通信，跨节点增加网络延迟和传输成本，两种 placement 都要测，不能只在 Mac 上启动多进程就当成跨节点结果。

原骨架只打印 world rank 0 的本地循环耗时。比较时两版先保持同一计时定义；若统一完善计时，可以在循环前 `MPI_Barrier(cart_comm)` 对齐起点，循环后用 `MPI_MAX` 汇总各 rank 的本地耗时，并由 cart rank 0 打印，归约本身放在被测循环之后。

骨架的 `t0` 在迭代循环之前，`t1` 在循环之后。因此 MPI 初始化、拓扑创建、网格初始化和 datatype 创建不计入 solve 时间，循环内的 halo 通信、计算和全局收敛归约计入 solve 时间。

### 9. 结合这次调试理解“假收敛”

你之前的 `32 100 1e-30` 在 27 轮后停止、平均值约 0.4557，不能据此认为精度特别高。当时外圈条件写错，只更新了四个角，其他本该更新的边缘留在初值。程序实际上在求解另一个错误的问题，错误结果也可能不再变化。

收敛条件只检查“新旧两轮差多少”，不检查“更新公式和覆盖范围是否写对”。即使两个版本输出相同，也需要确认它们没有共享同一个错误。

修正后，你的 `256` 网格在相同设置下，blocking 和 overlap 都在 64201 轮停止，局部平均值约 99.4534。这与边界值 100 不矛盾：`epsilon` 限制的是**一次迭代的最大变化**，不是当前解到最终稳态 100 的最大距离。大网格可能每轮只变化一点，但离最终稳态仍有距离。

如果输出使用 `%.6f`，实际略小于 `0.0001` 的值也可能显示为 `0.000100`。判断是否收敛应看程序比较的原始 double，而不是四舍五入后的字符串。

### 10. 二维分解改变了什么

同一个全局问题使用相同的 Jacobi 更新、边界和停止条件，改变进程划分后，各格子仍应读取相同上一轮邻居值。正确实现的迭代次数和全局误差应一致；改变的是每个 rank 的数据量、邻居通信和耗时。

设全局内部网格为 `G × G`，进程网格为 `P_x × P_y`，且能够整除：

- 每个 rank 自有行数 `r = G / P_x`、列数 `c = G / P_y`。
- 自有格子数为 `rc`；包含一圈 halo 的分配量为 `(r+2)(c+2)`。
- 可提前算的内部格子数为 `max(r-2,0) × max(c-2,0)`。
- 等待后更新的自有外圈格子数为 `rc - max(r-2,0) × max(c-2,0)`。
- 有四个真实邻居时，每轮发送 `2r + 2c` 个 double，接收同样数量。物理边界的 `MPI_PROC_NULL` 不产生真实消息，实际通信量会减少。

例如 `G=64`、`P=16`，采用 `4×4` 进程网格，每个 rank 自有 `16×16`，分配 `18×18`；可提前更新 `14×14=196` 格，等待后更新 `256-196=60` 格。一个四面都有邻居的 rank 每轮发送 `64` 个 double，按每个 double 8 bytes 算是 `512 bytes`，不包含接收量。

方形分解下，每个 rank 的计算量约为 `O(G²/P)`，边界数据量约为 `O(G/√P)`。固定全局规模而增加进程时，通信量与计算量之比约按 `√P/G` 增长。每条消息的启动延迟、每轮 `Allreduce` 也不会随着格子数等比例缩小，所以进程越多不保证越快。

## 七、Week 9 补齐：Collectives 与数据分解

### 1. Gather 和 Reduce 到底差在哪里

Collective 是 communicator 内所有成员共同参与的一类操作。不同函数决定数据如何移动、是否做运算，以及谁得到结果。

| 调用 | 数据从哪里来 | 谁得到什么 |
|---|---|---|
| `MPI_Bcast` | root 的一份数据 | 所有 rank 得到这份数据 |
| `MPI_Scatter` | root 的一份分块数据 | 每个 rank 得到自己的一块 |
| `MPI_Gather` | 每个 rank 的一块数据 | root 按 rank 顺序拼起来 |
| `MPI_Allgather` | 每个 rank 的一块数据 | 所有 rank 得到完整拼接结果 |
| `MPI_Reduce` | 每个 rank 的同形状数据 | root 得到逐元素归约结果 |
| `MPI_Allreduce` | 每个 rank 的同形状数据 | 所有 rank 得到逐元素归约结果 |
| `MPI_Barrier` | 不携带用户数据 | 所有成员进入后，调用才可返回 |

假设四个 rank 各有一个整数 `3,5,2,7`：

- Gather 给 root 一个长度为 4 的数组 `[3,5,2,7]`，保留各 rank 的贡献。
- Reduce 配合 `MPI_SUM` 给 root 一个整数 `17`。
- Allreduce 配合 `MPI_SUM` 给每个 rank 一个整数 `17`。

~~~c
MPI_Gather(&local_value, 1, MPI_INT,
           gathered, 1, MPI_INT, root, comm);
~~~

这里的接收 count `1` 是**每个 rank 贡献多少元素**，不是 root 最终数组长度。root 的 `gathered` 至少容纳 `P` 个 int，root 自己也贡献一个；按 communicator 的 rank 排列，与实际到达顺序无关。非 root 的接收缓冲区参数不使用，可传 `NULL`。

~~~c
MPI_Reduce(&local_value, &sum_at_root, 1,
           MPI_INT, MPI_SUM, root, comm);

MPI_Allreduce(&local_value, &sum_everywhere, 1,
              MPI_INT, MPI_SUM, comm);
~~~

`Reduce` 的结果只在 root 有效；`Allreduce` 没有 root 参数。这里的 root 编号属于传入的 `comm`，不一定等于 world rank。

对于数组，归约是**逐元素**进行：每个 rank 提供长度为 n 的数组，结果仍为长度 n，而不是自动把所有元素压成一个标量。Lab 5 先在本地压成一个最大值，才用 count=1 做全局最大值归约。

### 2. 为什么所有 rank 必须以一致顺序调用

不能只把 `MPI_Bcast` 放进 `if (rank == root)`。root 提供数据，其他成员也必须调用，以接收这次广播。

下面展示错误顺序：

~~~c
if (rank == 0) {
    MPI_Bcast(&x, 1, MPI_INT, 0, comm);
    MPI_Allreduce(&a, &b, 1, MPI_INT, MPI_SUM, comm);
} else {
    MPI_Allreduce(&a, &b, 1, MPI_INT, MPI_SUM, comm);
    MPI_Bcast(&x, 1, MPI_INT, 0, comm);
}
~~~

同一 communicator 上的 collective 必须按一致顺序匹配。这段代码把第一、第二次操作交叉了，可能挂住或出现其他错误。Collective 没有供应用自行匹配的 tag，不能靠“最后每人都调用过一次”补救。

各 rank 的本地缓冲区地址可以不同，但类型、数量及 root 等参数必须符合对应操作的匹配规则。普通阻塞 collective 返回，表示本进程可以按该调用的完成语义使用缓冲区；**不能把所有 collective 都当作 barrier**。

Week 9 PDF 38 的非阻塞 collective，例如 `MPI_Iallreduce`，同样要求各成员按一致顺序参与，并通过 request 判断完成。发布后可以做独立工作，但完成前不能使用结果缓冲区。

### 3. 为什么树式通信可能更快

在简化的“每轮每个进程发送一次完整消息”模型下，设消息为 n bytes：

`T_linear ≈ (P-1)(α+nβ)`

`T_tree ≈ ⌈log₂P⌉(α+nβ)`

P=64 时，root 逐个发送需要 63 次串行发送；理想倍增传播需要 6 轮。这里比较的是模型中的**串行通信轮数**，不能只数任意一张二叉树图的层数，就认定它等于实际传播时间。

实际 MPI 库会按消息大小、拓扑和实现选择算法，也可能分块或流水传输。因此不能说所有 broadcast、reduce、allreduce 都严格遵循同一个公式；Lab 4 的简单标量 ring 需要 P−1 轮，也不等于树算法。

浮点数加法还存在结合顺序问题：

~~~text
(a + b) + c 不一定等于 a + (b + c)
~~~

例如 double 中的极大数与很小的数相加，可能丢失小数值的贡献。改变归约树可能改变求和结果的末位。Lab 5 使用的是有限、非 NaN 变化量的 `MPI_MAX`，不涉及浮点求和的结合顺序；但本地计算写错仍然会得到错误的最大值。

### 4. Block、Cyclic、Block-Cyclic

| 分解 | 分配方法 | 适用直觉 |
|---|---|---|
| Block | 每个 rank 分到一段连续数据 | 局部性好；连续区间工作量不均时可能失衡 |
| Cyclic | 相邻元素轮流分给各 rank | 可分散随索引变化的工作量；数据较零散 |
| Block-Cyclic | 每次分配一个连续小块，再轮流分配块 | 在局部性和负载平衡之间折中 |

Block-Cyclic 的块长为 b、进程数为 P 时，元素 i 的拥有者为：

`owner(i) = floor(i / b) % P`

例如 P=4、b=2，rank 0 拥有 `0,1,8,9,16,17,…`。b=1 就退化为 cyclic；block 则是按完整连续区间划分，不能简单把任意大 b 都认作均匀 block。

### 5. 从矩阵向量乘法看 collective 的选择

对 `y=A×x`，采用按行分块：

1. Scatter：将 A 的不同行分给各 rank。
2. Bcast：将完整 x 发送给所有 rank。
3. 每个 rank 用本地行乘完整 x，得到 y 的一段。
4. Gather：将各段 y 按 rank 顺序拼回 root。

每个输出行需要完整 x，所以仅给每个 rank 一小段 x 不够。最后每个 rank 拥有不同输出行，应使用 Gather；如果各 rank 计算的是同一个输出元素的部分和，才需要 Reduce。

Week 9b 的一维矩阵乘法同理：分发 A 的行块，广播完整 B，本地计算 C 的行块，再 Gather。计算可以分摊，但每个 rank 都保存完整 B，这部分内存不会随着 P 增长而按 1/P 缩小。

### 6. Week 9b 的 Cannon 算法需要知道到哪里

Slides 的 Cannon 假设 `P=q²`，进程排成 `q×q`，矩阵按二维块分解。进程坐标为 `(i,j)`：

- 初始把 A 的块向左移 i 次，把 B 的块向上移 j 次，采用周期环绕。
- 初始配对的乘法指标为 `k=(i+j) mod q`，所以得到匹配的 `A[i,k]×B[k,j]`。
- 累加当前块乘积后，A 每轮左移一格、B 每轮上移一格，总共计算 q 组块乘积。

二维分解避免所有 rank 都保存完整 B。Cannon 的通信沿行、列周期环绕；Lab 5 是非周期拓扑，越出物理边界会得到 `MPI_PROC_NULL`，二者不能混用。

Week 9b PDF 16 明确把 **SUMMA 标为 Not Examinable**。Cannon 出现在前面的教学内容中，可作为矩阵分解的补充复习；现有资料没有说明 Quiz 必定出 Cannon。

## 八、Week 10 补齐：Datatype 与混合线程

### 1. Datatype 的“元素数量”和“跨度”不同

~~~c
MPI_Type_vector(4, 1, 10, MPI_DOUBLE, &column);
MPI_Type_commit(&column);
~~~

它选择相对起点偏移 `0,10,20,30` 的四个 double。若 double 占 8 bytes：

- 有效数据量为 `4×8=32 bytes`。
- 默认 extent 从第一个元素起点覆盖到最后一个元素末尾，为 `31×8=248 bytes`。
- stride=10 以旧类型的 extent 为单位；对 `MPI_DOUBLE`，就是相隔 10 个 double，而不是 10 bytes。

~~~c
MPI_Send(base, 1, column, dest, tag, comm);
~~~

count=1 已经发送四个选中的元素。count=2 则按新类型的 extent 再重复一份布局，不是自动选择“紧挨着的下一列”。

创建和 commit 描述布局；传输仍由 Send/Recv 等调用执行。类型通常只需创建一次并重复使用，通信结束后再释放。派生类型不保证底层零拷贝，MPI 可以在内部打包。

### 2. Subarray 描述二维子块

~~~c
int sizes[2]    = {8, 8};
int subsizes[2] = {2, 3};
int starts[2]   = {4, 3};

MPI_Type_create_subarray(2, sizes, subsizes, starts,
                        MPI_ORDER_C, MPI_DOUBLE, &block);
MPI_Type_commit(&block);
~~~

这里从一个 `8×8` 的 C 行优先数组中，选择从 `[4][3]` 开始的 `2×3` 子块。starts 使用从零开始的索引，`MPI_ORDER_C` 表示最后一维连续。

调用传输时，若 datatype 已经记录 starts，缓冲区应指向整张数组的对应基址。不能又把缓冲区移到 `&a[4][3]`，否则会重复偏移。Datatype 只描述本进程内存布局，不会自动创建跨进程共享数组。

### 3. MPI 与 OpenMP 混合时，谁能调用 MPI

~~~c
int provided;
MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
if (provided < MPI_THREAD_FUNNELED) {
    MPI_Abort(MPI_COMM_WORLD, 1);
}
~~~

`required` 表示程序希望获得的线程支持，`provided` 表示 MPI 实际提供的等级。请求后要检查结果，不能假设一定得到。

| 等级 | 允许的行为 |
|---|---|
| `MPI_THREAD_SINGLE` | 进程只执行一个线程 |
| `MPI_THREAD_FUNNELED` | 可以有多个线程，但只有调用初始化的主线程调用 MPI |
| `MPI_THREAD_SERIALIZED` | 多个线程可以调用 MPI，但任意时刻最多一个线程进入 MPI |
| `MPI_THREAD_MULTIPLE` | 多个线程可以同时调用 MPI |

`#pragma omp single` 只保证某一个线程执行，不保证它是初始化 MPI 的线程。因此不能仅靠 single 就满足 FUNNELED。将 MPI 调用放在 OpenMP 并行区域之外，是更容易解释的结构。

混合程序可以让一个 MPI rank 管一块数据，再用多个 OpenMP 线程更新这块数据。减少 MPI rank 数可能减少 halo 消息，但也会引入线程同步、负载和内存带宽因素，不保证一定优于纯 MPI。

## 九、Quiz 应该重点准备什么

**明确范围：Lab 5 手册写 Weeks 8–10、30 分钟、闭卷纸笔。下面的优先级是复习建议；资料没有提供实际考题。**

### 1. 复习优先级与 slides 对照

| 优先级 | 知识点 | 需要会做什么 | 依据 |
|---|---|---|---|
| 高 | 消息匹配、status、wildcard | 判断消息由谁接收，解释 source 与 tag | Week 8 PDF 22–24 |
| 高 | Blocking、缓冲与死锁 | 判断 Send 是否能返回，修复对称先 Send | Week 8 PDF 25–32 |
| 高 | RTT、单向延迟与单位 | 算 α、β、带宽、crossover | Week 8 PDF 7–9、33–34；Lab 4 |
| 高 | Ring All-Reduce | 手推 P−1 轮，说明 relay 与 total 的区别 | Lab 4 PDF 3；Week 9 PDF 18–24 |
| 高 | Gather / Reduce / Allreduce | 按输出需求选择调用，确定结果在哪些 rank 有效 | Week 9 PDF 14–20 |
| 高 | Collective 调用顺序 | 找出某 rank 缺调用、顺序不一致等错误 | Week 9 PDF 7、12 |
| 高 | Vector datatype | 算 count、blocklength、stride 和内存偏移 | Week 10 PDF 5–13 |
| 高 | Cartesian 拓扑 | 解释 dims、coords、shift、reorder、PROC_NULL | Week 10 PDF 15–18 |
| 高 | Jacobi 与重叠 | 找出内部/外圈，安排 Waitall，解释双缓冲 | Week 10 PDF 25–28；Lab 5 |
| 高 | 全局收敛 | 解释为什么用 Allreduce + MAX，为什么不能只看本地 | Week 9 PDF 18–20；Lab 5 PDF 2 |
| 高 | Overlap 成本模型 | 算收益，解释为什么非阻塞也可能更慢 | Week 10 PDF 20–23；Lab 4–5 |
| 中 | Block/Cyclic、强扩展 | 计算拥有者，比较通信与计算的增长比例 | Week 9 PDF 29–36；Week 10 PDF 22–28 |
| 中 | Subarray、线程支持 | 找出偏移错误，区分四个 thread level | Week 10 PDF 10–13、30–33 |
| 补充 | 矩阵乘法、Cannon | 解释为何广播 B、为什么初始错位和周期移位 | Week 9b PDF 4–15 |

### 2. 24 个闭卷自测问题

**1. `MPI_Send` 返回，接收方是否已经处理完？**

不保证。只保证发送缓冲区可以复用；接收方应用后续计算可能还没有开始。

**2. 为什么两个 rank 都先 Send，小消息正常、大消息却卡住？**

小消息可能由内部缓冲暂存；大消息可能等待对方发布接收。程序依赖了不保证存在的缓冲。可改变两边顺序，使用 Sendrecv，或先发布非阻塞收发再等待。

**3. Barrier 能代替 Waitall 吗？**

不能。Barrier 同步 communicator 成员进入某个位置；Waitall 完成本地列出的请求。二者保证不同。

**4. Collector 第二个收到的消息一定来自 rank 2 吗？**

不是。wildcard 接收由实际匹配决定，应读取 `status.MPI_SOURCE`，再验证 payload、tag 和重复来源。

**5. 四个 rank 分别有 1、2、3、4，Gather 和 Allreduce(SUM) 各得到什么？**

Gather 在 root 得到 `[1,2,3,4]`；Allreduce 在所有 rank 得到标量 `10`。

**6. 简单标量 ring 为什么需要 P−1 轮？**

初始只知道自己的值，每轮收到一个先前未收到的原始贡献；剩下 P−1 个贡献需要 P−1 轮。每轮转发刚收到的值，不能反复转发自己，也不能直接转发不断增长的累计和。

**7. `if (step == rank)` 能让整个集群按 rank 轮流执行环归约吗？**

不能。各进程独立执行自己的循环，并没有全局同步排序。这种写法让每个进程只交换一次，不能累计全部贡献。

**8. 单向时间：1000 bytes 为 12 μs，11000 bytes 为 32 μs，α、β、带宽和 crossover 是多少？**

`β=(32-12)/(11000-1000)=0.002 μs/byte`；`α=12-1000β=10 μs`。带宽为 `1/β=500 bytes/μs=0.5 GB/s`；启动成本等于传输成本时 `n*=α/β=5000 bytes`。如果题目给的是 RTT，先除以 2，再代入单向模型。

**9. 通信 80 ms、内部计算 120 ms、外圈 20 ms，理想重叠有什么收益？**

不重叠 `80+120+20=220 ms`；理想重叠 `max(80,120)+20=140 ms`。耗时减少约 `36.36%`，speedup 约 `1.57×`。二者不是同一个百分比。

**10. 为什么 Isend/Irecv 版本可能更慢？**

独立计算不足、通信本来就很快、MPI 异步推进有限、额外分段循环或条件判断，以及打包和内存访问开销，都可能抵消重叠收益。正确性与性能要分别解释。

**11. 自有网格 16×16，需要分配多少？能提前算多少？等待后算多少？**

分配 `18×18`；提前算 `14×14=196`；等待后算 `256-196=60`。halo 不属于自有更新区域。

**12. 这一网格的列类型 stride 是多少？为什么传输 count=1？**

stride=18，因为分配宽度包含左右 halo。vector 本身已包含一整列自有元素，count=1 表示一份列布局。

**13. Cart_create 的 reorder=1 后，能直接用 world rank 计算坐标吗？**

不能假设两种 rank 相同。先从 Cartesian communicator 获取 cart rank，再用它获取坐标；邻居通信也使用这个 communicator。

**14. 从 MPI_PROC_NULL 接收，会把 halo 设成零吗？**

不会。调用可正常完成，但接收缓冲区内容保持原样。物理边界应预先初始化为题目规定的值。

**15. 为什么顶部 halo 接收 TAG_DOWN？**

tag 以发送方向命名；上方邻居把它的底行向下发送，接收方用对应的 TAG_DOWN 匹配。

**16. 为什么第一行自有格子不能放到 Waitall 前计算？**

它需要顶部 halo 的上一轮温度；接收未完成时该区域不可读取。内部格子没有这一依赖。

**17. 为什么不能原地更新 old_grid？**

后续格子会混用本轮与上一轮数据，改变 Jacobi 的同步更新语义，还可能改写尚未完成的发送区域。用 old/new 两份数组保持本轮输入一致。

**18. 本地误差已经小于 epsilon，为什么不能直接退出？**

其他 rank 可能仍未收敛，继续通信时会等待退出的 rank。Allreduce(MAX) 提供统一的全局误差和一致停止决定。

**19. 27 轮收敛能证明程序正确吗？**

不能。漏更新格子也可能造成“新旧不再变化”。要检查更新公式、范围、边界和全局误差，而不能只看 converged=yes。

**20. 为什么收敛后平均值还没到 100？**

停止条件限制单次迭代变化，不直接限制当前解与稳态的距离；小变化不等于已经精确达到最终解。

**21. 固定全局网格，进程数越来越多，为什么可能变慢？**

每个 rank 的计算区缩小，可覆盖通信的内部计算减少；边界/面积比增大，消息延迟和全局归约开始占主要部分。

**22. P=64，简化线性传播和理想倍增传播分别多少轮？**

线性为 63 次串行发送，理想倍增为 6 轮。该模型不能直接套给所有消息大小下的所有 MPI 实现。

**23. FUNNELED 下，把 MPI 调用放进 omp single 就一定合法吗？**

不一定。single 可由任意团队线程执行；FUNNELED 要求由初始化 MPI 的主线程调用。

**24. Cannon 为什么要先错位？**

让 `(i,j)` 处 A 和 B 的块共享乘法指标 `k=(i+j) mod q`；之后同步周期移位，q 轮覆盖全部 k。

### 3. 闭卷前应能独立说明的十件事

1. 从一段 Send/Recv 代码写出每条消息的 source、destination、tag 和 communicator。
2. 画出先 Send 后 Recv 的等待关系，并给出一种不依赖消息缓冲的修复。
3. 从 RTT 推出单向时间，再算 α、β、带宽，写清单位。
4. 手推四进程 ring 三轮，解释 relay 和 total 为何不能混用。
5. 区分 Gather、Reduce、Allreduce，以及 collective 顺序要求。
6. 从二维数组宽度写出列 vector 的 count、blocklength、stride。
7. 解释 Cart_shift 得到谁，为什么 reorder 后要使用 cart rank。
8. 准确指出五点 stencil 的内部、外圈、halo 和物理边界。
9. 说明八个请求、内部计算、Waitall、外圈计算和 Allreduce 的先后依赖。
10. 用成本模型解释 overlap 的收益和限制，区分“耗时减少”和“加速比”。

回答代码题时，按这条顺序组织：**当前 rank 拥有什么 → 从谁拿什么 → 相关内存何时能读写 → 如何得到一致的继续或退出条件。**
