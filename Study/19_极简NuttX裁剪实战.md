# 极简 NuttX 裁剪实战 —— 把 123KB 砍到 40KB 级

> 本文是《NuttX 源码学习文档》第 19 篇。**这是一份让你自己动手的实验指导书**，
> 不是"照抄命令"的清单 —— 每一步都请先**猜**、再**做**、最后**对答案**。
>
> **目标**：搞清楚你固件里那 123KB 到底是谁在吃，并亲手把它砍到 40KB 级。
>
> **收获**：这个练习真正的价值不是"省了多少 Flash"（你有 512KB，根本不缺），
> 而是**建立"每个 CONFIG 开关对应多少代码"的直觉**。这个直觉在选型、
> 排查、面试时都比背概念有用得多。

---

## 0. 安全网：先能随时回来

**动手之前必做**，否则改坏了要重配一遍。

```bash
cd /home/nuttx/nuttxspace/nuttx

# 1) 把当前"完整版"配置存成板子的第二套 defconfig
mkdir -p boards/arm/stm32f1/m144z-m3/configs/full
make savedefconfig
cp defconfig boards/arm/stm32f1/m144z-m3/configs/full/defconfig

# 2) 确认存进去了
grep -c . boards/arm/stm32f1/m144z-m3/configs/full/defconfig
```

以后任何时候想回到完整版：

```bash
cd /home/nuttx/nuttxspace/nuttx
make distclean
./tools/configure.sh m144z-m3:full
make -j$(nproc)
```

> **想清楚再往下**：`make savedefconfig` 生成的 `defconfig` 只记录**和默认值不同**
> 的项，所以它很短。这是 NuttX 保存配置的标准做法，比备份整个 `.config` 更好 ——
> `.config` 换个 NuttX 版本就可能失效，`defconfig` 不会。

---

## 1. 先学会"测量" —— 没有数据就是瞎砍

裁剪的第一原则：**每改一步就量一次**。给你三把尺子。

### 尺子① `size` —— 总量

```bash
arm-none-eabi-size nuttx
```
```
   text    data     bss     dec     hex  filename
 122970     504    2664  126138   1ecba  nuttx
```

| 列 | 含义 | 占哪儿 |
|---|---|---|
| `text` | 代码 + 只读数据 | **Flash** |
| `data` | 有初值的全局变量 | Flash（存初值）+ RAM（运行时） |
| `bss` | 无初值的全局变量 | 只占 RAM |

**烧进 Flash 的 = text + data**。`bss` 不占 Flash。

### 尺子② 按库拆分 —— 知道是谁在吃

新建 `~/bin/nuttx-size.sh`，内容如下（一次粘贴即可）：

```bash
cat > ~/bin/nuttx-size.sh <<'EOF'
#!/usr/bin/env bash
# 按静态库统计最终固件里各部分的大小
# 原理: 用 nm 拿到 ELF 里每个符号的大小, 再查它是哪个 .a 定义的
cd "${1:-/home/nuttx/nuttxspace/nuttx}" || exit 1
python3 - <<'PY'
import subprocess, glob, collections, os
NM = 'arm-none-eabi-nm'
sym = {}
for l in subprocess.run([NM,'--defined-only','--print-size','--format=posix','nuttx'],
                        capture_output=True, text=True).stdout.splitlines():
    p = l.split()
    if len(p) >= 4 and p[1] in 'TtRrDdBbWwVv':
        try: sym[p[0]] = int(p[3], 16)
        except ValueError: pass
owner = {}
for lib in sorted(glob.glob('staging/*.a')):
    for l in subprocess.run([NM,'--defined-only','--format=posix',lib],
                            capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) >= 2 and p[1] in 'TtRrDdBbWwVv':
            owner.setdefault(p[0], os.path.basename(lib))
tot = collections.Counter(); unk = 0
for s, sz in sym.items():
    lib = owner.get(s)
    if lib: tot[lib] += sz
    else:   unk += sz
s = sum(tot.values()) + unk
print(f"  {'库':<20}{'大小':>10}   占比")
for k, v in tot.most_common():
    print(f"  {k:<20}{v/1024:8.1f} KB   {100*v/s:5.1f}%")
print(f"  {'(未归类)':<20}{unk/1024:8.1f} KB")
print(f"  {'合计':<20}{s/1024:8.1f} KB")
PY
EOF
chmod +x ~/bin/nuttx-size.sh
```

跑一下：

```bash
bash ~/bin/nuttx-size.sh
```

> **注意**：`nm` 只能统计"有大小的符号"，字符串常量、向量表、对齐填充统计不到，
> 所以合计会比 `size` 的 text+data 小十几 KB。**看比例，别抠绝对值。**

### 尺子③ `savedefconfig` diff —— 知道自己到底改了什么

```bash
make savedefconfig
diff boards/arm/stm32f1/m144z-m3/configs/full/defconfig defconfig
```

每做完一个实验都 diff 一次，你会发现**关一个开关常常连带关掉一串** ——
这正是 Kconfig 依赖关系在起作用，值得留意。

---

## 2. 建立基线

先把当前状态记下来。**请你自己跑一遍并填表**：

```bash
cd /home/nuttx/nuttxspace/nuttx
arm-none-eabi-size nuttx
bash ~/bin/nuttx-size.sh
```

| 项 | 你的数值 |
|---|---|
| text + data | ______ KB |
| libsched.a（内核） | ______ KB |
| libapps.a（NSH + 你的 example） | ______ KB |
| libfs.a（VFS + procfs） | ______ KB |
| libc.a | ______ KB |
| libarch.a（芯片层） | ______ KB |
| libdrivers.a（驱动框架） | ______ KB |

**看完这张表先回答一个问题**（这决定了你该砍哪里）：

> 你觉得"NuttX 内核"占了这 123KB 的多大比例？先猜一个数，再看表。
> 猜完往下翻 —— 如果你的直觉和数据差很远，说明这个练习对你很值。

---

## 3. 裁剪实验

每个实验的固定流程：

```
① 先猜能省多少   →  ② 改配置  →  ③ make clean && make -j  →  ④ 量  →  ⑤ 对答案
```

> ⚠️ **改配置后必须 `make clean`**（第 18 篇 §6.7 讲过：增量编译不会重编受影响的文件）。
> 每次都是：
> ```bash
> make olddefconfig && make clean && make -j$(nproc) && arm-none-eabi-size nuttx
> ```

---

### 实验 1：关掉 procfs

**先猜**：`ps` 命令背后那套 `/proc` 文件系统，占多少？____ KB

**动手**：
```bash
kconfig-tweak --disable CONFIG_FS_PROCFS
make olddefconfig && make clean && make -j$(nproc)
arm-none-eabi-size nuttx
```

**失去什么**：`nsh> ps` 没了（`ps: command not found`）。

**思考**：`FS_PROCFS` 关掉后，`.config` 里那一堆 `FS_PROCFS_EXCLUDE_*`
去哪了？用 `diff` 看看，理解 `if XXX ... endif` 在 Kconfig 里的作用。

---

### 实验 2：关掉你自己写的三个 example

**先猜**：marquee + mqtune + ledsw 三个加起来多少？____ KB

**动手**：
```bash
kconfig-tweak --disable CONFIG_EXAMPLES_MQTUNE
kconfig-tweak --disable CONFIG_EXAMPLES_LEDSW
# marquee 先留着 —— 实验 3 要拿它当唯一的应用
make olddefconfig && make clean && make -j$(nproc)
arm-none-eabi-size nuttx
```

**这一步的意义**：让你对"自己写的代码到底多大"有个量级概念。
三百多行 C 编出来才几 KB —— **绝大部分固件不是你写的**。

---

### 实验 3：关掉 NSH ★ 这是最大的一刀，也是转折点

**先猜**：整个命令行 shell 占多少？____ KB

**动手前先想清楚一件事**：NSH 是通过 `CONFIG_INIT_ENTRYPOINT="nsh_main"`
被当作"开机第一个任务"启动的。关掉 NSH，**必须给系统换一个入口**，
否则内核起来后没有任何用户任务，等于白跑。

```bash
# 1) 先换入口，再关 NSH（顺序反了会 olddefconfig 报错）
bash ~/bin/nuttx-initapp.sh marquee_main     # 第 18 篇那个切换脚本

# 2) 关掉 NSH 本体
kconfig-tweak --disable CONFIG_SYSTEM_NSH
kconfig-tweak --disable CONFIG_NSH_LIBRARY

make olddefconfig && make clean && make -j$(nproc)
arm-none-eabi-size nuttx
```

**失去什么**：**没有命令行了**。开机直接跑 marquee，串口只有 printf 输出。
想控制它只能改代码重烧。

**验证**：烧录后串口应该看到 `marquee: 启动 (固定 200 ms)`，LED 开始交替闪，
但敲什么都没反应（因为没有 shell 在读串口）。

**对答案**：这一刀通常能砍掉 **20KB 以上**。到这里你应该已经明白：
> **"NuttX 重"很大程度上重在那个像 Linux 的 shell，不是内核。**

---

### 实验 4：回收 NSH 的"附庸"

NSH 没了，一堆当初为它开的东西也就没用了。**这一步最能体现 Kconfig 依赖的连锁效应**。

**先猜**：下面这些加起来还能再省多少？____ KB

```bash
kconfig-tweak --disable CONFIG_BUILTIN            # builtin 命令注册表(没 shell 就没人查表)
kconfig-tweak --disable CONFIG_SYSTEM_READLINE    # 行编辑
kconfig-tweak --disable CONFIG_SCHED_WAITPID      # NSH 前台等子进程用的
kconfig-tweak --disable CONFIG_SERIAL_TERMIOS     # 终端属性(波特率/回显等 ioctl)
kconfig-tweak --disable CONFIG_TTY_SIGINT         # Ctrl-C
kconfig-tweak --enable  CONFIG_DISABLE_PTHREAD    # 之前为 NSH 的 & 才开的(见第18篇§6.6)

make olddefconfig && make clean && make -j$(nproc)
arm-none-eabi-size nuttx
```

> ⚠️ **注意**：关掉 pthread 后，如果你的 marquee 用到了 `pthread_*` 会编译失败。
> 它没用，所以没事。但这提醒你：**裁剪会连带影响应用代码，不是纯配置游戏。**

**做完后跑一下 `diff`**，看看你只写了 6 条命令，`defconfig` 实际变了多少行：

```bash
make savedefconfig
diff boards/arm/stm32f1/m144z-m3/configs/full/defconfig defconfig | head -40
```

---

### 实验 5：信号系统降级（进阶）

**背景**：第 18 篇里为了让 `kill` 能真的杀掉任务，我们开了
`ENABLE_ALL_SIGNALS` + `SIG_DEFAULT` + `SIG_SIGKILL_ACTION`。
没有 shell 就没人 `kill` 了。

```bash
kconfig-tweak --enable  CONFIG_ENABLE_PARTIAL_SIGNALS
kconfig-tweak --disable CONFIG_ENABLE_ALL_SIGNALS
make olddefconfig && make clean && make -j$(nproc)
```

> ⚠️ **这一步大概率编译失败**！因为 `marquee_main.c` 里的 `worker_alive()`
> 用了 `kill(g_pid, 0)`。
>
> **这是故意留给你的坑** —— 请你自己判断：
> - 没有 shell 了，还需要 `worker_alive()` 这套自愈机制吗？
> - 如果不需要，把它删掉再编；如果需要，就别关这个开关。
>
> **裁剪的本质是"根据实际需求做减法"，不是"能关就关"。**

---

### 实验 6：驱动层（进阶，需要改代码）

现在 `libdrivers.a` 里还有 userled 上下半部 + 串口驱动。

- **userled**：应用通过 `open("/dev/userleds") + ioctl` 控灯。极简系统里可以
  绕过整个 VFS，直接调 `stm32_gpiowrite(GPIO_LED0, false)`。
  代价：应用代码和芯片强绑定，失去可移植性。
- **串口**：如果只要 `printf` 输出、不需要输入，可以只保留 `syslog` + `arm_lowputc`，
  砍掉整个 `drivers/serial` 的上下半部。

**这一步我不给命令** —— 请你先回答：

1. 你的应用真的不需要串口**输入**吗？
2. 放弃 `/dev` 抽象，换来的几 KB 值得吗？
3. 如果以后换板子，代价是什么？

想清楚了再动手。方向提示：
`CONFIG_USERLED` / `CONFIG_STANDARD_SERIAL` / `CONFIG_DEV_CONSOLE` / `CONFIG_DISABLE_MOUNTPOINT`。

---

## 4. 记录你的实验结果

**请边做边填**，做完这张表你就有了自己的"配置—体积"直觉：

| 阶段 | 关掉了什么 | 猜测省 | 实测 text+data | 实际省 | 猜得准吗 |
|---|---|---|---|---|---|
| 基线 | — | — | ______ | — | — |
| 实验1 | procfs | ____ | ______ | ____ | |
| 实验2 | 两个 example | ____ | ______ | ____ | |
| 实验3 | **NSH** | ____ | ______ | ____ | |
| 实验4 | NSH 附庸 | ____ | ______ | ____ | |
| 实验5 | 信号降级 | ____ | ______ | ____ | |
| 实验6 | 驱动/VFS | ____ | ______ | ____ | |

**最后再跑一次按库拆分**，和第 2 节的基线表对比：

```bash
bash ~/bin/nuttx-size.sh
```

哪个库的**占比**变化最大？哪个几乎没变？为什么？

---

## 5. 把成果存成第二套配置

裁剪完别丢了，存成 `m144z-m3:tiny`：

```bash
cd /home/nuttx/nuttxspace/nuttx
mkdir -p boards/arm/stm32f1/m144z-m3/configs/tiny
make savedefconfig
cp defconfig boards/arm/stm32f1/m144z-m3/configs/tiny/defconfig
```

从此两套配置随时切换：

```bash
./tools/configure.sh -L m144z-m3          # 应该列出 full / nsh / tiny
make distclean && ./tools/configure.sh m144z-m3:tiny && make -j$(nproc)   # 极简版
make distclean && ./tools/configure.sh m144z-m3:full && make -j$(nproc)   # 完整版
```

> ⚠️ `make distclean` 会删掉 `.config`，所以**一定要先 savedefconfig 存好**。

---

## 6. 哪些不能关（关了必崩）

裁剪时踩到这些会直接不启动，且现象很迷惑：

| 开关 | 关掉后 | 为什么 |
|---|---|---|
| `CONFIG_INIT_ENTRYPOINT` 指向不存在的函数 | 链接报 undefined，或起来后无任何任务 | `nx_bringup()` 靠它 spawn 第一个任务 |
| `CONFIG_ARCH_BOARD_COMMON` | 板级代码不编，缺 `stm32_boardinitialize` | `__start()` 无条件调它 |
| `CONFIG_BOARD_LATE_INITIALIZE` | `stm32_bringup()` 不被调用，`/dev/userleds` 不存在 | 应用 `open()` 失败 |
| `CONFIG_STM32_USART1` / `USART1_SERIAL_CONSOLE` | 串口全哑 | 你连"哪里死的"都看不到 |
| `CONFIG_STM32_JTAG_SW_ENABLE` | **SWD 引脚被复用掉，J-Link 连不上** | 只能靠 BOOT0 进 ROM bootloader 救砖 |
| `CONFIG_DEBUG_SYMBOLS` | 能跑但没法单步 | 调试链失效 |

> **最后一条特别危险**：关掉 SWD 后烧进去，板子就"失联"了。
> 救法：BOOT0 拨高 → 复位 → ST 串口 bootloader 重新刷（见第 17 篇）。
> **动 `STM32_JTAG_*` 之前先确认你会救砖。**

---

## 7. 思考题（做完实验再回答）

1. 基线里 `libsched.a` 占 ~29KB。裁剪到最后它变了多少？**为什么内核几乎砍不动？**
2. 实验 3 关掉 NSH 省了 20 多 KB，但 `libapps.a` 减少的量可能不止这些 ——
   还有哪些库跟着瘦了？用按库拆分的数据说明。
3. 假设现在给你一块 **64KB Flash** 的 STM32F103C8，你会保留哪些功能？先列清单再验证。
4. 回到上一次的问题："比 NuttX 更轻的 RTOS"。做完这个实验，
   你觉得 **FreeRTOS 6-12KB 内核 vs NuttX 29KB 内核**，这个差距的代价是什么？
   （提示：对比一下你失去的 `open/ioctl/printf/task_create` 有多少是 FreeRTOS 自带的）
5. 极简版和完整版，你会在什么场景下分别使用？

---

## 8. 一页速查

```bash
# 存档(动手前必做)
make savedefconfig && cp defconfig boards/arm/stm32f1/m144z-m3/configs/full/defconfig

# 每步循环
kconfig-tweak --disable CONFIG_XXX
make olddefconfig && make clean && make -j$(nproc)      # ★ 必须 clean
arm-none-eabi-size nuttx
bash ~/bin/nuttx-size.sh
make savedefconfig && diff .../configs/full/defconfig defconfig

# 存成第二套配置
mkdir -p boards/arm/stm32f1/m144z-m3/configs/tiny
make savedefconfig && cp defconfig boards/arm/stm32f1/m144z-m3/configs/tiny/defconfig

# 两套之间切换
make distclean && ./tools/configure.sh m144z-m3:tiny && make -j$(nproc)
make distclean && ./tools/configure.sh m144z-m3:full && make -j$(nproc)

# 救砖(SWD 关掉了/固件跑飞)
# BOOT0 拨高 → 复位 → stm32flash 或 STM32CubeProgrammer 走串口重刷(见第17篇)
```

---

## 参考

- 第 11 篇《编译教程》：configure / make 基本流程
- 第 17 篇《代码固化烧录教程》：烧录、BOOT0、救砖
- 第 18 篇 §6.7：**改了 .config 必须 make clean**
- 第 18 篇 §6.9：怎么确认板上跑的是哪一版固件
