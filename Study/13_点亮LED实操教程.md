# NuttX 点亮 LED 实操教程（M144Z-M3 / STM32F103ZET6）

> 本文是《NuttX 源码学习文档》的第 13 篇，手把手带初学者在 NuttX 框架内
> 点亮正点原子 M144Z-M3 Mini 开发板上的两个 LED（PB5、PE5，低有效）。
> 所有步骤基于 `boards/arm/stm32f1/m144z-m3` 板级代码核实，照着做即可。
>
> 目标读者：**第一次在真实开发板上跑 NuttX 的人。**
>
> 文中所有 `文件:行号` 均为 Markdown 链接，在 VS Code 里 `Ctrl+单击` 直接跳到源码。

---

## 1. 本文适用范围

- **开发板**：正点原子 M144Z-M3 Mini（ALIENTEK）
- **MCU**：STM32F103ZET6（Cortex-M3，512K Flash / 64K SRAM）
- **配置**：`m144z-m3:nsh`
- **硬件连接**：

  | LED | 引脚 | 电路 | 点亮条件 |
  |-----|------|------|----------|
  | LED0（红） | PB5 | 阳极经限流电阻接 3.3V | 引脚输出**低电平** |
  | LED1（绿） | PE5 | 同上 | 引脚输出**低电平** |

  这种接法叫 **低有效 / active-low**：写 0（低）灯亮，写 1（高）灯灭。

- **串口**：板载 CH340C USB 转串口挂在 **USART1（PA9/PA10）**，波特率 115200
- **烧录/调试器**：J-Link（本文只讲点灯，调试环境配置见 [.vscode/launch.json](../.vscode/launch.json)）

---

## 2. 代码索引（点开先扫一眼）

板级 LED 代码 **已经写好了**，下表每个链接都能直接跳转。你只需"看懂"，不用改：

| 层 | 文件:行号 | 作用 |
|----|-----------|------|
| 配置 | [boards/arm/stm32f1/m144z-m3/include/board.h:115](../boards/arm/stm32f1/m144z-m3/include/board.h#L115) | `#define BOARD_NLEDS 2`，告诉系统有 2 个灯 |
| 配置 | [boards/arm/stm32f1/m144z-m3/include/board.h:117](../boards/arm/stm32f1/m144z-m3/include/board.h#L117) | `LED_STARTED`…`LED_PANIC` 状态码（仅 autoled 用） |
| 引脚 | [boards/arm/stm32f1/m144z-m3/src/m144z-m3.h:49](../boards/arm/stm32f1/m144z-m3/src/m144z-m3.h#L49) | `GPIO_LED0` = PB5 推挽输出 + `GPIO_OUTPUT_SET`（上电默认高＝灭） |
| 引脚 | [boards/arm/stm32f1/m144z-m3/src/m144z-m3.h:52](../boards/arm/stm32f1/m144z-m3/src/m144z-m3.h#L52) | `GPIO_LED1` = PE5 |
| 板级下层 | [boards/arm/stm32f1/m144z-m3/src/stm32_userleds.c:61](../boards/arm/stm32f1/m144z-m3/src/stm32_userleds.c#L61) | `board_userled_initialize()` 配置两个 GPIO 为输出 |
| 板级下层 | [boards/arm/stm32f1/m144z-m3/src/stm32_userleds.c:83](../boards/arm/stm32f1/m144z-m3/src/stm32_userleds.c#L83) | `board_userled()`：`stm32_gpiowrite(pin, !ledon)` ← **`!` 就是低有效取反** |
| 板级下层 | [boards/arm/stm32f1/m144z-m3/src/stm32_userleds.c:91](../boards/arm/stm32f1/m144z-m3/src/stm32_userleds.c#L91) | `board_userled_all()`：按位图一次刷新所有灯 |
| 板级下层 | [boards/arm/stm32f1/m144z-m3/src/stm32_autoleds.c:80](../boards/arm/stm32f1/m144z-m3/src/stm32_autoleds.c#L80) | `board_autoled_on()`（仅 `CONFIG_ARCH_LEDS`，见第 6 节） |
| 开机注册 | [boards/arm/stm32f1/m144z-m3/src/stm32_bringup.c:76](../boards/arm/stm32f1/m144z-m3/src/stm32_bringup.c#L76) | `userled_lower_initialize("/dev/userleds")`（需 `CONFIG_USERLED_LOWER`） |
| 开机入口 | [boards/arm/stm32f1/m144z-m3/src/stm32_boot.c:74](../boards/arm/stm32f1/m144z-m3/src/stm32_boot.c#L74) | `board_late_initialize()` → `stm32_bringup()`（需 `CONFIG_BOARD_LATE_INITIALIZE`） |
| 构建 | [boards/arm/stm32f1/m144z-m3/src/Make.defs:31](../boards/arm/stm32f1/m144z-m3/src/Make.defs#L31) | `CONFIG_ARCH_LEDS` 决定编 `stm32_autoleds.c` 还是 `stm32_userleds.c` |
| 通用上层 | [drivers/leds/userled_lower.c:148](../drivers/leds/userled_lower.c#L148) | `userled_lower_initialize()`：创建 `/dev/userleds` 字符设备 |
| 通用上层 | [drivers/leds/userled_lower.c:116](../drivers/leds/userled_lower.c#L116) | ioctl `ULEDIOC_SETALL` 最终调 `board_userled_all()` |
| 接口定义 | [include/nuttx/leds/userled.h:49](../include/nuttx/leds/userled.h#L49) | `ULEDIOC_SUPPORTED` / `ULEDIOC_SETLED` / `ULEDIOC_SETALL` 命令号 |
| 接口定义 | [include/nuttx/leds/userled.h:116](../include/nuttx/leds/userled.h#L116) | `struct userled_s { ul_led; ul_on; }` |
| 官方示例 | [apps/examples/leds/leds_main.c:75](../../apps/examples/leds/leds_main.c#L75) | `led_daemon()` 跑马灯完整实现 |

> ⚠️ `apps/` 是**独立仓库**，在 `nuttx/` 的**上一级目录**，所以链接里是 `../../apps/...`。

---

## 3. NuttX 的 LED 分层（理解这张图＝理解框架）

NuttX 不鼓励应用直接操作寄存器。点一个灯要穿过 4 层：

```
  你的 App (myleds / leds 示例)
        │  open("/dev/userleds")  +  ioctl(ULEDIOC_SETALL, 位图)
        ▼
  /dev/userleds                                      ← 字符设备节点
        │
  drivers/leds/userled_lower.c  (通用上层, 架构无关)  ← userled_lower.c:116
        │  调用 board_userled_all() / board_userled()
        ▼
  boards/.../m144z-m3/src/stm32_userleds.c  (板级下层) ← stm32_userleds.c:83
        │  stm32_gpiowrite(GPIO_LEDx, !on)   ← 这里的 "!" 处理"低有效"
        ▼
  arch/arm/src/stm32/stm32_gpio.c  (直接读写 GPIOB / GPIOE 寄存器)
```

- 位图约定：`/dev/userleds` 里 **bit0 = LED0（PB5）**，**bit1 = LED1（PE5）**，位 = 1 表示"亮"。
- 低有效取反已由 [stm32_userleds.c:83](../boards/arm/stm32f1/m144z-m3/src/stm32_userleds.c#L83) 的 `!ledon` 做掉，**应用层永远 `1 = 亮`**。

你要做的只有两件事：

1. `make menuconfig` 打开几个开关，让 `/dev/userleds` 出现；
2. 用自带示例、或自己写一个 App 去操作 `/dev/userleds`。

---

## 4. 前置：确认工程已配置

```bash
cd /home/nuttx/nuttxspace/nuttx

# 若 .config 已存在可跳过；想从零开始：
make distclean
./tools/configure.sh m144z-m3:nsh
```

`configure.sh` 参数格式 `<板子名>:<配置名>`，对应
[boards/arm/stm32f1/m144z-m3/configs/nsh/defconfig](../boards/arm/stm32f1/m144z-m3/configs/nsh/defconfig)。

---

## 5. 路线 A：用 NuttX 自带的 `leds` 示例（不写代码，最快见效）

### 5.1 打开 menuconfig

```bash
make menuconfig
```

操作方法：方向键移动，`Enter` 进子菜单，`Y` 选中，`N` 取消，`Esc Esc` 返回。
**找不到某项时按 `/` 输入符号名搜索**，结果里按前面的数字键可直接跳转。

### 5.2 打开这 4 个开关

| # | 配置符号 | 菜单路径 | 作用 |
|---|----------|----------|------|
| 1 | `CONFIG_BOARD_LATE_INITIALIZE` | `RTOS Features` → `Board-Specific Initialization` → `Board late initialization` | 开机调用 [stm32_bringup()](../boards/arm/stm32f1/m144z-m3/src/stm32_bringup.c#L58) |
| 2 | `CONFIG_USERLED` | `Device Drivers` → `LED Support` | 打开 LED 驱动框架 |
| 3 | `CONFIG_USERLED_LOWER` | `Device Drivers` → `LED Support` → `Generic Lower Half LED Driver` | 生成 `/dev/userleds` 节点 |
| 4 | `CONFIG_EXAMPLES_LEDS` | `Application Configuration` → `Examples` → `LED driver example` | 自带跑马灯示例 |

> ⚠️ 同一个 `LED Support` 菜单里还有 `CONFIG_ARCH_LEDS`（"Architecture LED support"），
> **不要开**。它和 `CONFIG_USERLED` 二选一（见第 7 节），选错会导致 [Make.defs:31](../boards/arm/stm32f1/m144z-m3/src/Make.defs#L31) 编成 `stm32_autoleds.c`。

`Esc Esc` 一路退出，提示保存选 `Yes`。

### 5.3 编译

```bash
make -j$(nproc)
```

成功后仓库根目录生成 `nuttx`（ELF）、`nuttx.bin`、`nuttx.hex`。

### 5.4 烧录

```bash
bash /mnt/d/AI/STM32F103ZET6-MINI-SYS/Nuttx_DEMO/debug/run_flash.sh
```

或 VS Code：`Ctrl+Shift+P` → `Tasks: Run Task` → `NuttX: 编译并烧录 (J-Link)`。

### 5.5 打开串口，运行

串口工具连 115200（WSL 里 `picocom -b 115200 /dev/ttyUSB0`），复位板子：

```
NuttShell (NSH) NuttX-12.x
nsh> ls /dev
/dev:
 console
 ttyS0
 userleds          ← 出现它 = 驱动 OK

nsh> leds
led_daemon (100): Started
led_daemon: Supported LEDs 0x03    ← 0x03 = 两个灯都识别到
led_daemon: LED set 0x01
...
```

PB5、PE5 循环闪烁 = **点灯成功**。复位即可停止。
跑马灯逻辑见 [apps/examples/leds/leds_main.c:75](../../apps/examples/leds/leds_main.c#L75)。

---

## 6. 路线 B：写自己的应用 `myleds`（学会调用框架）

### 6.1 新建目录和 4 个文件

```bash
mkdir -p /home/nuttx/nuttxspace/apps/examples/myleds
cd /home/nuttx/nuttxspace/apps/examples/myleds
```

创建后这几个文件的链接（先建后可点）：
[Kconfig](../../apps/examples/myleds/Kconfig) ·
[Make.defs](../../apps/examples/myleds/Make.defs) ·
[Makefile](../../apps/examples/myleds/Makefile) ·
[myleds_main.c](../../apps/examples/myleds/myleds_main.c)

**`Kconfig`** —— 定义这个 app 的开关与参数：

```kconfig
config EXAMPLES_MYLEDS
	tristate "My LED blink example"
	default n
	depends on USERLED
	---help---
		Blink PB5 / PE5 through /dev/userleds

if EXAMPLES_MYLEDS

config EXAMPLES_MYLEDS_PROGNAME
	string "Program name"
	default "myleds"

config EXAMPLES_MYLEDS_PRIORITY
	int "myleds task priority"
	default 100

config EXAMPLES_MYLEDS_STACKSIZE
	int "myleds stack size"
	default DEFAULT_TASK_STACKSIZE

endif
```

**`Make.defs`** —— 把本目录登记进构建系统（仿照 [apps/examples/leds/Make.defs](../../apps/examples/leds/Make.defs)）：

```makefile
ifneq ($(CONFIG_EXAMPLES_MYLEDS),)
CONFIGURED_APPS += $(APPDIR)/examples/myleds
endif
```

**`Makefile`**：

```makefile
include $(APPDIR)/Make.defs

PROGNAME  = $(CONFIG_EXAMPLES_MYLEDS_PROGNAME)
PRIORITY  = $(CONFIG_EXAMPLES_MYLEDS_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_MYLEDS_STACKSIZE)
MODULE    = $(CONFIG_EXAMPLES_MYLEDS)

MAINSRC = myleds_main.c

include $(APPDIR)/Application.mk
```

**`myleds_main.c`**（ioctl 命令号见 [include/nuttx/leds/userled.h:49](../include/nuttx/leds/userled.h#L49)）：

```c
/****************************************************************************
 * apps/examples/myleds/myleds_main.c
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <nuttx/leds/userled.h>

/* /dev/userleds 位图:  bit0 = LED0(PB5),  bit1 = LED1(PE5)
 * 位 = 1 表示"亮"（低有效已由板级 stm32_userleds.c 取反处理）
 */

#define LED0_BIT  (1 << 0)   /* PB5 */
#define LED1_BIT  (1 << 1)   /* PE5 */

int main(int argc, char *argv[])
{
  userled_set_t supported = 0;
  userled_set_t ledset;
  int fd;
  int i;

  /* 1. 打开 LED 设备 */

  fd = open("/dev/userleds", O_WRONLY);
  if (fd < 0)
    {
      fprintf(stderr, "打开 /dev/userleds 失败: errno=%d\n", errno);
      return 1;
    }

  /* 2. 查询硬件支持哪些灯（应返回 0x03） */

  if (ioctl(fd, ULEDIOC_SUPPORTED, (unsigned long)(uintptr_t)&supported) == 0)
    {
      printf("支持的 LED 位图: 0x%02x\n", (unsigned int)supported);
    }

  /* 3. 主循环：两个灯交替闪 10 次 */

  for (i = 0; i < 10; i++)
    {
      ledset = (i & 1) ? LED0_BIT : LED1_BIT;

      if (ioctl(fd, ULEDIOC_SETALL, (unsigned long)ledset) < 0)
        {
          fprintf(stderr, "ULEDIOC_SETALL 失败: errno=%d\n", errno);
          break;
        }

      usleep(300 * 1000);   /* 300 ms */
    }

  /* 4. 收尾：两个灯都亮 2 秒，再全灭 */

  ioctl(fd, ULEDIOC_SETALL, (unsigned long)(LED0_BIT | LED1_BIT));
  sleep(2);
  ioctl(fd, ULEDIOC_SETALL, 0);

  close(fd);
  printf("myleds 结束\n");
  return 0;
}
```

> **单独控制一个灯**用 `ULEDIOC_SETLED` + [struct userled_s](../include/nuttx/leds/userled.h#L116)：
> ```c
> struct userled_s s;
> s.ul_led = 0;      /* 0 = LED0(PB5), 1 = LED1(PE5) */
> s.ul_on  = true;   /* true = 亮 */
> ioctl(fd, ULEDIOC_SETLED, (unsigned long)(uintptr_t)&s);
> ```

### 6.2 打开这个 app 的开关

```bash
cd /home/nuttx/nuttxspace/nuttx
make menuconfig
```

- 保留第 5.2 节的 `CONFIG_BOARD_LATE_INITIALIZE`、`CONFIG_USERLED`、`CONFIG_USERLED_LOWER`
- `CONFIG_EXAMPLES_LEDS` 可关掉
- `Application Configuration` → `Examples` → 勾选 `My LED blink example`（`CONFIG_EXAMPLES_MYLEDS`）

保存退出。

> 菜单里没有 `My LED blink example`？执行 `cd ../apps && make apps_distclean`，
> 再回 `nuttx` 目录重开 menuconfig。多半是文件名拼错（是 `Make.defs` 不是 `Makefile.defs`）。

### 6.3 编译、烧录、运行

```bash
make -j$(nproc)
bash /mnt/d/AI/STM32F103ZET6-MINI-SYS/Nuttx_DEMO/debug/run_flash.sh
```

串口：

```
nsh> myleds
支持的 LED 位图: 0x03
myleds 结束
```

改 `myleds_main.c` 的延时/次数 → 重新 `make` + 烧录即可。

---

## 7. 路线 C（了解即可）：让操作系统自动驱动 LED

只想**验证接线**、不想写代码：

```bash
make menuconfig
# 打开 Device Drivers → LED Support → Architecture LED support (CONFIG_ARCH_LEDS)
# 关掉 CONFIG_USERLED
```

此时 [Make.defs:31](../boards/arm/stm32f1/m144z-m3/src/Make.defs#L31) 会改编
[stm32_autoleds.c](../boards/arm/stm32f1/m144z-m3/src/stm32_autoleds.c)，
操作系统把两个灯当**状态指示灯**（[board.h:117](../boards/arm/stm32f1/m144z-m3/include/board.h#L117) 的状态码 → [board_autoled_on():80](../boards/arm/stm32f1/m144z-m3/src/stm32_autoleds.c#L80)）：

| 事件 | 现象 |
|------|------|
| 启动完成 | PB5 常亮 |
| 进入中断 | PB5 短暂灭 |
| panic / assert | PB5 + PE5 一起狂闪 |

看到 PB5 亮 = 接线与 GPIO 配置正确。缺点：**不能用程序控制灯**。
`CONFIG_ARCH_LEDS` 与 `CONFIG_USERLED` 二选一。

---

## 8. 用调试器观察（配合 F5 单步）

1. 在 [stm32_userleds.c:99](../boards/arm/stm32f1/m144z-m3/src/stm32_userleds.c#L99) 的 `stm32_gpiowrite` 那行打断点。
2. F5 启动调试 → 串口执行 `myleds` → 命中断点，单步进入 `stm32_gpiowrite`。
3. Debug Console 直接看 GPIO 输出寄存器：

   ```
   x/1xw 0x40010C0C     # GPIOB_ODR, bit5 = PB5 电平
   x/1xw 0x40011814     # GPIOE_ODR, bit5 = PE5 电平
   ```

   灯亮时对应 bit = 0（低电平）。

4. 装了 `peripheral-viewer` + SVD 后，侧栏 `XPERIPHERALS` → `GPIOB` → `ODR` → `ODR5` 直接看。

调试环境本身的配置见 [.vscode/launch.json](../.vscode/launch.json)。

---

## 9. 常见坑

| 现象 | 原因 / 解决 |
|------|-------------|
| `ls /dev` 里没有 `userleds` | 忘了开 `CONFIG_BOARD_LATE_INITIALIZE` → [stm32_boot.c:74](../boards/arm/stm32f1/m144z-m3/src/stm32_boot.c#L74) 的 `board_late_initialize()` 不会被调用，`stm32_bringup()` 白写；或忘了 `CONFIG_USERLED_LOWER` |
| `open("/dev/userleds")` 返回 `-ENOENT` | 同上 |
| 灯逻辑反了（该亮时灭） | **别在 App 里再取反电平**。[stm32_userleds.c:83](../boards/arm/stm32f1/m144z-m3/src/stm32_userleds.c#L83) 已用 `!ledon` 处理低有效，App 层永远 `1 = 亮` |
| `leds` 示例报 `Supported LEDs 0x00` | 编成了 `stm32_autoleds.c`。确认 `.config` 里是 `# CONFIG_ARCH_LEDS is not set` |
| PB5 亮、PE5 不亮 | [board_userled_initialize():61](../boards/arm/stm32f1/m144z-m3/src/stm32_userleds.c#L61) 里 `stm32_configgpio()` 会自动使能 GPIOE 时钟，通常不是时钟问题。先量 PE5 对地电压（亮时应 ≈0V），排查焊点/跳线/复用冲突 |
| menuconfig 找不到 `My LED blink example` | `cd ../apps && make apps_distclean` 后重开；检查 4 个文件名 |
| 改了 `.config` 后灯行为没变 | 每次改配置必须重新 `make` **并重新烧录**。Cortex-Debug 的 `loadFiles: []` 不烧录，调试前先手动烧 |
| `make` 报 `Application.mk: No such file` | `Makefile` 里 `include $(APPDIR)/Application.mk` 必须是最后一行 |

---

## 10. 一页流程总结

```
路线 A（自带示例）:
  make menuconfig 开 4 项 → make -j$(nproc) → 烧录 → nsh> leds

路线 B（自己写）:
  建 apps/examples/myleds/{Kconfig, Make.defs, Makefile, myleds_main.c}
  → menuconfig 开 BOARD_LATE_INITIALIZE + USERLED + USERLED_LOWER + EXAMPLES_MYLEDS
  → make -j$(nproc) → 烧录 → nsh> myleds

调框架的本质:
  open("/dev/userleds")  →  ioctl(ULEDIOC_SETALL, 位图)
  bit0 = PB5   bit1 = PE5   1 = 亮（低有效已由板级代码处理）
```

---

## 11. 延伸阅读

- [Study/06_设备驱动.md](06_设备驱动.md) —— 驱动注册机制、驱动与 VFS 的关系
- [Study/02_启动流程.md](02_启动流程.md) —— `board_late_initialize()` 在启动序列中的位置
- [drivers/leds/userled_lower.c](../drivers/leds/userled_lower.c) —— 上层驱动源码
- [drivers/leds/userled_upper.c](../drivers/leds/userled_upper.c) —— 字符设备 ops（open/read/ioctl）
- [apps/examples/leds/leds_main.c](../../apps/examples/leds/leds_main.c) —— 官方示例完整实现（含守护线程、信号处理）
- [include/nuttx/leds/userled.h](../include/nuttx/leds/userled.h) —— ioctl 接口与数据结构
