# Keil 裸机 IAP 串口升级实战（STM32F103ZET6）

> 本文是《NuttX 源码学习文档》第 20 篇。虽然编号在 NuttX 系列里，但**内容是纯裸机 + Keil MDK**，
> 和 NuttX 无关。放在这里是因为它和第 17 篇《代码固化烧录教程》是同一条线：
> 从"靠调试器烧"走到"产品自己升级"。
>
> **这是一份让你自己动手的指导书**。最关键的方法论是：
> **绝对不要一次写完整个 bootloader**。分 5 个阶段，每阶段都能独立验证，
> 每阶段跑通了再往下。跳过这个纪律，你会陷入"烧进去没反应，不知道死在哪"的泥潭。

---

## 0. 先建立心智模型

### 0.1 IAP 到底在干什么

正常单片机：Flash 里就一个程序，上电从 `0x08000000` 跑。

IAP：把 Flash **切成两块**，放两个**互相独立**的程序。

```
0x08000000 ┌───────────────────────┐
           │  Bootloader   32 KB   │  ← 上电先跑它。永不被擦除
           │  · 判断要不要升级      │
           │  · 收固件、擦写 App 区 │
           │  · 跳到 App           │
0x08008000 ├───────────────────────┤
           │  Application  480 KB  │  ← 你的业务代码。升级时被整体替换
           └───────────────────────┘  0x08080000 (512KB 结束)
```

**两个是完全独立的 Keil 工程**，各自编译、各自有 `main()`、各自有中断向量表。

### 0.2 三个必须理解的机制

动手前请先想明白这三件事，想不明白后面必卡：

**① Cortex-M 上电时到底发生了什么**

CPU 复位后是**硬件行为**，不执行任何指令：
```
MSP ← 内存[0x08000000]      (向量表第 0 项 = 初始栈顶)
PC  ← 内存[0x08000004]      (向量表第 1 项 = 复位处理函数地址)
```
所以"跳到 App"本质就是**软件模仿这个过程**：手动把 App 向量表的这两个值
装进 MSP 和 PC。

**② 中断向量表在哪，CPU 怎么找**

Cortex-M3 有个寄存器 `SCB->VTOR`（地址 `0xE000ED08`），存放"当前向量表的基地址"。
复位默认是 0。
- Bootloader 跑的时候，VTOR = 0（用 `0x08000000` 的表）✅
- 跳到 App 后如果**不改 VTOR**，来中断时 CPU 还是去 `0x08000000` 找处理函数
  → 跳进 Bootloader 的中断向量 → **跑飞**

**这是 IAP 最经典的坑**，90% 的"App 单独烧能跑、经 Bootloader 跳过去就死"都是它。

**③ F103 擦写 Flash 时 CPU 在干什么**

STM32F103 只有**一个 Flash bank**。擦除/编程期间，Flash 总线被占用，
**CPU 从 Flash 取指会被 stall**。这意味着：
- 擦写期间如果来中断，中断向量和 ISR 代码都在 Flash 里 → 取不到 → 出事
- **结论：擦写全程必须关中断**（或把擦写函数搬到 RAM 执行）

---

## 1. 阶段一：先做一个"能在 0x08008000 跑起来"的 App

**这一阶段完全不碰 Bootloader。** 目标只有一个：证明你的 App 挪了地址还能跑。

### 1.1 新建 App 工程

随便一个能闪灯的裸机工程即可（用你熟的模板）。先确认它在**默认地址** `0x08000000`
能正常跑，再往下改。

### 1.2 改 Keil 的 ROM 地址

`Options for Target` → **Target** 标签页：

| 字段 | 原值 | 改成 |
|---|---|---|
| IROM1 Start | `0x8000000` | **`0x8008000`** |
| IROM1 Size | `0x80000` | **`0x78000`** （480KB） |
| IRAM1 | `0x20000000` / `0x10000` | 不动 |

> **自己想一下**：为什么 Size 要从 0x80000 改成 0x78000？
> （提示：0x78000 = 0x80000 − 0x8000）

### 1.3 改中断向量表偏移

**两种做法，任选其一：**

**做法 A（推荐，改一行）** —— 标准库工程里 `system_stm32f10x.c`：
```c
#define VECT_TAB_OFFSET  0x8000      /* 原来是 0x0 */
```
这行会在 `SystemInit()` 里被用来设置 `SCB->VTOR`。**先自己去文件里找到那段代码，
确认它确实做了 `SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;`** —— 别盲信，不同版本库不一样。

**做法 B（HAL 库或想显式一点）** —— 在 `main()` 第一行：
```c
SCB->VTOR = 0x08008000;
```

### 1.4 让 Keil 生成 .bin 文件

Keil 默认只出 `.axf` / `.hex`，IAP 传输一般用 `.bin`（纯二进制，无地址信息，最省字节）。

`Options for Target` → **User** 标签页 → 勾选 `After Build/Rebuild` 的
`Run #1`，填：

```
fromelf --bin --output ".\Objects\app.bin" ".\Objects\app.axf"
```

> 路径按你工程实际的 Output 目录改。编译后去看一眼 `app.bin` 有没有生成、多大。

### 1.5 验证（关键！这一步跑通才能往下）

用 J-Link/ST-Link 把 App **直接烧到 0x08008000**：

- Keil：`Options` → `Debug` → `Settings` → `Flash Download`，
  确认 Programming Algorithm 的地址范围覆盖了 0x08008000
- 或者用 J-Link Commander：
  ```
  loadbin app.bin 0x08008000
  ```

烧完之后 **直接复位是不会跑的**（因为 `0x08000000` 那里现在是空的 0xFF）。
所以要**手动设 PC 验证**：

```
J-Link>connect
J-Link>halt
J-Link>w4 0xE000ED08, 0x08008000        # 手动设 VTOR
J-Link>setPC 0x08008000                  # 先别急, 见下
```

更简单可靠的做法 —— 用 GDB：
```
(gdb) target remote localhost:2331
(gdb) monitor reset
(gdb) set $sp = {unsigned int}0x08008000
(gdb) set $pc = {unsigned int}0x08008004
(gdb) continue
```
（这套你在第 17 篇 BOOT0 兜底里已经用过，原理一模一样。）

**判定标准**：LED 开始闪 = 阶段一通过。

> ❓ **如果不闪，先自查**：
> 1. `app.bin` 前 8 个字节是什么？`hexdump -C app.bin | head -1`
>    第 1 个 word 应该是 `0x2000xxxx`（栈顶，在 SRAM 范围），
>    第 2 个 word 应该是 `0x0800Axxx`（在 App 区且末位为 1，Thumb 位）。
>    **不对说明地址没改对。**
> 2. VTOR 设了吗？把 SysTick 中断关掉试试 —— 如果关了就能跑，就是 VTOR 问题。

---

## 2. 阶段二：最小 Bootloader —— 只做跳转，不做任何升级

**这一阶段的 Bootloader 只有几十行**，目的是把"上电 → Bootloader → App"这条链打通。

### 2.1 新建 Bootloader 工程

`Options for Target` → Target：

| 字段 | 值 |
|---|---|
| IROM1 Start | `0x8000000` |
| IROM1 Size | **`0x8000`**（32KB，别给太大） |

向量表偏移**保持 0**（Bootloader 就在 0 地址）。

### 2.2 跳转函数

```c
#define APP_ADDR   0x08008000U

typedef void (*app_entry_t)(void);

static void jump_to_app(void)
{
    uint32_t sp = *(volatile uint32_t *)(APP_ADDR);
    uint32_t pc = *(volatile uint32_t *)(APP_ADDR + 4);

    /* ---- 清场：把 Bootloader 用过的东西全还原 ---- */
    __disable_irq();

    SysTick->CTRL = 0;                 /* 关 SysTick，否则 App 会收到莫名其妙的中断 */
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    for (int i = 0; i < 8; i++) {      /* 关掉并清空所有外部中断 */
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    /* 你在 Bootloader 里开过的外设时钟，也建议在这里复位
     * (RCC_APB2PeriphResetCmd / RCC_APB1PeriphResetCmd)，
     * 否则 App 重新初始化时可能撞上"已经被配过"的状态 */

    SCB->VTOR = APP_ADDR;              /* ★ 向量表交给 App */

    __set_MSP(sp);                     /* 装载 App 的栈顶 */
    __enable_irq();

    ((app_entry_t)pc)();               /* 走你 */
    /* 不会返回 */
}
```

### 2.3 App 有效性校验（防砖的第一道保险）

**跳之前必须检查 App 区是不是真有东西**，否则跳进 0xFFFFFFFF 直接 HardFault：

```c
static int app_valid(void)
{
    uint32_t sp = *(volatile uint32_t *)(APP_ADDR);

    /* 栈顶必须落在 SRAM 范围内 (F103ZET6: 0x20000000 ~ 0x20010000) */
    if ((sp & 0x2FFE0000U) != 0x20000000U) {
        return 0;
    }
    return 1;
}
```

> **自己想**：这个 `0x2FFE0000` 掩码是怎么来的？为什么不用 `>= 0x20000000 && <= 0x20010000`？
> （两种都行，前者是 ST 官方例程的写法，只用一条与运算）

### 2.4 Bootloader 的 main

```c
int main(void)
{
    SystemInit();
    uart_init(115200);                 /* 用 USART1: PA9/PA10, 板载 CH340 */

    printf("\r\nBootloader v0.1\r\n");

    if (app_valid()) {
        printf("App found, jumping...\r\n");
        delay_ms(50);                  /* 等 printf 发完，否则跳过去后串口被 App 重配，最后几个字符丢失 */
        jump_to_app();
    }

    printf("No valid app!\r\n");
    while (1) { /* 停在这里等升级 */ }
}
```

### 2.5 验证

1. 用 J-Link 把 **Bootloader 烧到 0x08000000**（正常烧就行）
2. App 已经在 0x08008000（阶段一烧的）
3. **按复位键**

**判定标准**：串口先打印 `Bootloader v0.1` + `App found, jumping...`，然后 LED 开始闪。

> ❓ **打印出来了但 LED 不闪** → 跳转有问题。按顺序查：
> 1. `app_valid()` 返回 1 了吗？（把 sp 的值打印出来看）
> 2. App 的 `VECT_TAB_OFFSET` 改了吗？
> 3. `SysTick->CTRL = 0` 加了吗？
> 4. 用 GDB 在 `jump_to_app` 最后一行打断点，单步进去，看 PC 落到哪
>
> ❓ **连 Bootloader 的打印都没有** → 是 Bootloader 本身的问题，和 IAP 无关，
> 先单独把它当普通程序调通。

**阶段二通过 = 你已经有一个可用的双分区系统了。** 剩下的只是"怎么把新固件弄进 App 区"。

---

## 3. 阶段三：Flash 擦写 —— 先不接串口，写死数据测试

**别急着上串口协议**。先证明"你能正确擦写 App 区"。

### 3.1 F103 Flash 编程的三个硬性约束

先自己去《STM32F10xxx Flash 编程手册》(PM0075) 确认，别背我的：

| 约束 | 值 | 影响 |
|---|---|---|
| 擦除粒度 | **页**（ZET6 高密度 = 2KB/页） | 想改 1 个字节也要擦整页 |
| 编程粒度 | **半字（16 bit）** | 不能按字节写；数据长度要凑成偶数 |
| 擦写期间 | 总线 stall | **必须关中断** |

### 3.2 写一个最小的擦写测试

```c
#define APP_ADDR      0x08008000U
#define PAGE_SIZE     2048U

/* 擦除从 addr 开始的 len 字节所覆盖的所有页 */
static int flash_erase(uint32_t addr, uint32_t len)
{
    uint32_t p;
    FLASH_Unlock();
    for (p = addr; p < addr + len; p += PAGE_SIZE) {
        if (FLASH_ErasePage(p) != FLASH_COMPLETE) {
            FLASH_Lock();
            return -1;
        }
    }
    FLASH_Lock();
    return 0;
}

/* 写入。data 长度必须是偶数(半字对齐) */
static int flash_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    FLASH_Unlock();
    for (i = 0; i < len; i += 2) {
        uint16_t hw = data[i] | ((uint16_t)data[i + 1] << 8);
        if (FLASH_ProgramHalfWord(addr + i, hw) != FLASH_COMPLETE) {
            FLASH_Lock();
            return -1;
        }
    }
    FLASH_Lock();
    return 0;
}
```

**关中断包在外面**：
```c
    __disable_irq();
    flash_erase(APP_ADDR, 4096);
    flash_write(APP_ADDR, test_data, sizeof(test_data));
    __enable_irq();
```

### 3.3 验证

在 Bootloader 里加一段测试代码：擦掉 App 区头 4KB，写进去一小段已知数据，
再读回来比对，串口打印结果。

```c
    uint8_t pattern[16] = {0x11,0x22,0x33,0x44, /* ... */};
    __disable_irq();
    flash_erase(APP_ADDR, PAGE_SIZE);
    flash_write(APP_ADDR, pattern, sizeof(pattern));
    __enable_irq();

    if (memcmp((void *)APP_ADDR, pattern, sizeof(pattern)) == 0)
        printf("flash test OK\r\n");
    else
        printf("flash test FAIL\r\n");
```

**判定标准**：打印 `flash test OK`。

> ⚠️ **跑完这个测试你的 App 就被破坏了**（头 4KB 被覆盖），这是预期的。
> 用 J-Link 重新烧一次 App 即可。**验证完记得把这段测试代码删掉。**

> ❓ **FAIL 怎么查**：
> - `FLASH_Unlock()` 调了吗？
> - 擦除后读回来应该全是 `0xFF`，先验证这一步
> - 写的地址是不是半字对齐（偶数）？
> - `FLASH_ErasePage` 的返回值是什么？打印出来

---

## 4. 阶段四：串口接收固件

到这里三块积木（跳转、擦写、串口）都验证过了，现在把它们串起来。

### 4.1 协议选型：先自己想

| 方案 | 优点 | 缺点 |
|---|---|---|
| 自定义简单协议 | 完全可控，好调试 | 要自己写上位机 |
| **XMODEM-1K / YMODEM** | **超级终端 / Tera Term / SecureCRT 原生支持，不用写上位机** | 协议要实现 CRC 和重传 |
| ST 原生 bootloader 协议(AN3155) | 现成工具链 | 那是方案 A，用不着自己写 |

**建议第一次用 YMODEM** —— ST 官方 IAP 例程 **AN2557** 就是 YMODEM，
代码可以直接拿来参考，而且 Tera Term 一个"发送文件"就搞定，省掉上位机开发。

### 4.2 YMODEM 要点（自己去查协议细节，这里只给骨架）

```
上位机                          Bootloader
                    <---- 'C' (每秒发一次，表示"我准备好了，用CRC模式")
SOH/STX 包0 (文件名+大小) ---->
                    <---- ACK, 'C'
STX 包1 (1024字节数据)  ---->    擦一页、写一页
                    <---- ACK
STX 包2 ...            ---->
                    ...
EOT                    ---->
                    <---- ACK
```

**实现时的三个关键点**（这些是 AN2557 里踩过的）：

1. **别收一包写一包地擦** —— 擦除是按 2KB 页的，YMODEM 一包 1KB。
   要么攒够 2KB 再擦写，要么**一开始就把整个 App 区擦干净**（简单但慢）。
   初学建议后者。
2. **超时处理** —— 每包要有超时，否则串口断了就死循环。
3. **写完立刻读回校验** —— 别信"写成功"的返回值。

### 4.3 什么时候进升级模式

```c
int main(void)
{
    SystemInit();
    uart_init(115200);

    printf("\r\nPress 'u' within 3s to update...\r\n");

    if (wait_key('u', 3000)) {          /* 3 秒窗口 */
        ymodem_receive();               /* 收固件 -> 擦写 App 区 */
    }

    if (app_valid()) jump_to_app();

    printf("No valid app, waiting for update...\r\n");
    ymodem_receive();                   /* 没有 App 就死等升级 */
}
```

**其它常见触发方式**（产品里更常用）：

| 触发源 | 做法 |
|---|---|
| App 收到升级命令 | App 往**备份寄存器** `BKP_DRx` 写魔数 → 软复位 → Bootloader 读到魔数就进升级 |
| 按键 | 上电时检测 KEY0 是否按下 |
| RAM 标志 | 定义一个 `.noinit` 段的变量，复位不清零 |

> **自己想**：为什么用 `BKP_DRx` 而不是普通全局变量？
> （提示：软复位后 SRAM 内容还在吗？靠得住吗？）

---

## 5. 阶段五：防砖 —— 让它"升级失败也能救回来"

前面四阶段跑通就能用了，但**升级到一半断电会变砖**。产品必须解决这个。

### 5.1 加固件校验

在 App 编译产物末尾附加一个 CRC32，Bootloader 跳转前校验：

```
App 区布局:
  0x08008000  ┌──────────────┐
              │  App 代码     │
              │      ...      │
              ├──────────────┤
              │ 长度(4B)      │  ← 固定位置，比如 App 区末尾
              │ CRC32(4B)     │
  0x08080000  └──────────────┘
```

Bootloader：
```c
if (crc32((uint8_t *)APP_ADDR, app_len) != stored_crc) {
    printf("App CRC error, stay in bootloader\r\n");
    ymodem_receive();       /* 不跳转，等重新升级 */
}
```

CRC 怎么加进 bin？两个办法：
- 写个 PC 端小工具（Python 十几行）给 `app.bin` 追加长度和 CRC
- 或者用 Keil 的 `fromelf` + 后处理脚本

### 5.2 进阶：A/B 双分区

```
0x08000000  Bootloader   32 KB
0x08008000  App A       240 KB     ← 当前运行
0x08044000  App B       240 KB     ← 升级时写这里
```
写完 B 并校验通过，才把"下次启动用 B"的标志写进 Flash 的一个配置页。
**任何时候断电，都至少有一个完整可用的分区。**

代价：可用空间减半。你的 512KB 够用。

---

## 6. Keil 相关的坑

| 坑 | 现象 | 处理 |
|---|---|---|
| App 的 IROM Size 没跟着改小 | 编译能过，但链接器以为有 512KB，大工程会溢出到不存在的地址 | Size 改成 `0x78000` |
| 忘了改 `VECT_TAB_OFFSET` | App 单独烧能跑，经 Bootloader 跳过去一进中断就死 | 见 §1.3 |
| 没生成 .bin | 只有 .hex/.axf，没法通过串口传 | User 页加 `fromelf --bin` |
| 调 App 时 Keil 把 Bootloader 擦了 | Flash Download 默认 "Erase Full Chip" | 改成 **Erase Sectors**，且 Programming Algorithm 的地址范围只覆盖 App 区 |
| 用 Keil 调 App 却看不到源码 | 下载地址和 IROM 地址不一致 | Debug → Settings 里核对 |
| 串口打印丢最后几个字符 | 跳转太快，UART 发送 FIFO 还没空 | 跳转前 `delay_ms(50)` 或等 TC 标志 |

---

## 7. 阶段验收清单

**做完一阶段打一个勾再往下**，不要跳：

- [ ] **阶段一**：App 改到 0x08008000，J-Link 直接烧 + GDB 手动设 SP/PC，LED 能闪
- [ ] **阶段二**：Bootloader 烧到 0x08000000，按复位 → 串口打印 → 自动跳到 App，LED 闪
- [ ] **阶段三**：Bootloader 里擦写 App 区并读回校验，打印 `flash test OK`
- [ ] **阶段四**：Tera Term 发送 `app.bin`，Bootloader 收完自动跳转，新固件跑起来
- [ ] **阶段五**：故意在传输中途断开，复位后 Bootloader 报 CRC 错误并停在等待升级状态（不变砖）

---

## 8. 思考题

1. 为什么 Bootloader 要留 32KB 而不是 8KB？（提示：YMODEM + CRC + 串口驱动大概多大？先猜再实测）
2. 如果 App 崩溃了、进了 HardFault，怎么让它自动回到 Bootloader？（提示：看门狗？备份寄存器？）
3. Bootloader 自己要怎么升级？（想想为什么大多数产品选择"Bootloader 一旦出厂就不再改"）
4. 阶段三里，如果不关中断直接擦写会发生什么？**（建议真去试一次，看现象）**
5. 你现在跑 NuttX 的那块板子，能不能用这个 Bootloader 升级 NuttX？
   需要改 NuttX 的什么？（提示：第 17 篇里的 `ld.script` 和 `__start`）

---

## 参考

- **AN2557** — ST 官方 STM32F10x IAP 例程（YMODEM，可直接抄）
- **AN3155** — ST 出厂 ROM bootloader 的串口协议（方案 A 用）
- **PM0075** — STM32F10xxx Flash 编程手册（擦写时序、页大小的权威依据）
- 本系列第 17 篇《代码固化烧录教程》—— BOOT0、救砖、J-Link 烧录命令
