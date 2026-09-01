# NuttX LED 跑马灯实操（M144Z-M3 / PB5 · PE5）

> 本文是《NuttX 源码学习文档》的第 16 篇。在第 13 篇「点亮 LED」的基础上，
> 写一个自己的应用 `marquee`，让 PB5、PE5 两个 LED 循环跑马灯。
> **所有文件都由你手动创建 / 修改**，本文逐个给出完整内容和确切位置。
>
> 前置阅读：[13_点亮LED实操教程.md](13_点亮LED实操教程.md)（分层原理、低有效、`/dev/userleds`）

---

## 0. 现在的状态（本文基于此）

`nuttx/.config` 当前：

| 配置 | 值 | 含义 |
|------|-----|------|
| `CONFIG_ARCH_LEDS` | **y** | 现在 LED 被内核当**状态灯**用（`stm32_autoleds.c`），你控制不了 |
| `CONFIG_USERLED` | 未设置 | 用户态 LED 框架**没开** |
| `CONFIG_BOARD_LATE_INITIALIZE` | y | 开机会调 `stm32_bringup()` ✅ |
| `CONFIG_DEBUG_FEATURES` | y | （之前为看启动字母打开的，与本文无关）|

目标：**关掉 `ARCH_LEDS`，打开 `USERLED`**，让 `/dev/userleds` 出现，再写 app 操作它。

板级代码（`boards/arm/stm32f1/m144z-m3/src/stm32_userleds.c`）已经写好，不用动：

- `BOARD_NLEDS = 2`
- 位图约定：**bit0 = LED0 = PB5**，**bit1 = LED1 = PE5**
- 低有效取反已在 `board_userled_all()` 里做掉 → **app 层永远 `1 = 亮`**

---

## 1. 改内核配置（3 个开关）

### 方式 A：menuconfig（推荐）

```bash
cd /home/nuttx/nuttxspace/nuttx
make menuconfig
```

> 按 `/` 输入符号名可搜索并跳转。

1. **关掉** `CONFIG_ARCH_LEDS`
   路径：`Board Selection` → `Board LED Status support`
   光标移到它，按 `N`。

2. **打开** `CONFIG_USERLED`
   路径：`Device Drivers` → `LED Support` → `LED driver`
   按 `Y`。

3. **打开** `CONFIG_USERLED_LOWER`
   打开 `LED driver` 后，同一子菜单里出现 `Generic Lower Half LED Driver`，按 `Y`。

`Esc Esc` 一路退出，保存选 `Yes`。

### 方式 B：直接改 `.config`（你要"手动改所有文件"就用这个）

编辑 `nuttx/.config`：

```diff
- CONFIG_ARCH_LEDS=y
+ # CONFIG_ARCH_LEDS is not set
```

在文件里加两行（放在 `CONFIG_USART1_...` 附近，字母序无所谓）：

```
CONFIG_USERLED=y
CONFIG_USERLED_LOWER=y
```

然后让 NuttX 补全依赖：

```bash
cd /home/nuttx/nuttxspace/nuttx
make olddefconfig
```

### 验证

```bash
grep -E "ARCH_LEDS|USERLED" .config
# 期望:
#   # CONFIG_ARCH_LEDS is not set
#   CONFIG_USERLED=y
#   CONFIG_USERLED_LOWER=y
```

> 关掉 `ARCH_LEDS` 后，构建系统会自动改编 `stm32_userleds.c`（而不是 `stm32_autoleds.c`），
> 见 `boards/arm/stm32f1/m144z-m3/src/Make.defs` 里的 `ifeq ($(CONFIG_ARCH_LEDS),y)`。

---

## 2. 新建应用 `apps/examples/marquee/`（4 个文件）

```bash
mkdir -p /home/nuttx/nuttxspace/apps/examples/marquee
cd /home/nuttx/nuttxspace/apps/examples/marquee
```

### 文件 1：`Kconfig`

```kconfig
#
# LED 跑马灯示例
#

config EXAMPLES_MARQUEE
	tristate "LED marquee (跑马灯) example"
	default n
	depends on USERLED
	---help---
		在 /dev/userleds 上循环点亮 LED，形成跑马灯效果。
		本板: bit0 = PB5(LED0), bit1 = PE5(LED1)。

if EXAMPLES_MARQUEE

config EXAMPLES_MARQUEE_PROGNAME
	string "Program name"
	default "marquee"

config EXAMPLES_MARQUEE_PRIORITY
	int "marquee task priority"
	default 100

config EXAMPLES_MARQUEE_STACKSIZE
	int "marquee stack size"
	default DEFAULT_TASK_STACKSIZE

config EXAMPLES_MARQUEE_DEVPATH
	string "LED device path"
	default "/dev/userleds"

config EXAMPLES_MARQUEE_DELAY_MS
	int "每帧间隔 (毫秒)"
	default 200

endif # EXAMPLES_MARQUEE
```

### 文件 2：`Make.defs`

```make
############################################################################
# apps/examples/marquee/Make.defs
############################################################################

ifneq ($(CONFIG_EXAMPLES_MARQUEE),)
CONFIGURED_APPS += $(APPDIR)/examples/marquee
endif
```

### 文件 3：`Makefile`

```make
############################################################################
# apps/examples/marquee/Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = $(CONFIG_EXAMPLES_MARQUEE_PROGNAME)
PRIORITY  = $(CONFIG_EXAMPLES_MARQUEE_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_MARQUEE_STACKSIZE)
MODULE    = $(CONFIG_EXAMPLES_MARQUEE)

MAINSRC = marquee_main.c

include $(APPDIR)/Application.mk
```

> ⚠ `include $(APPDIR)/Application.mk` 必须是**最后一行**。

### 文件 4：`marquee_main.c`

```c
/****************************************************************************
 * apps/examples/marquee/marquee_main.c
 *
 * LED 跑马灯: 在 /dev/userleds 上循环切换图案。
 *   bit0 = LED0 = PB5,  bit1 = LED1 = PE5,  1 = 亮
 *   (低有效取反已由板级 stm32_userleds.c 处理, app 层只管 1=亮)
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <nuttx/leds/userled.h>

#ifndef CONFIG_EXAMPLES_MARQUEE_DEVPATH
#  define CONFIG_EXAMPLES_MARQUEE_DEVPATH "/dev/userleds"
#endif
#ifndef CONFIG_EXAMPLES_MARQUEE_DELAY_MS
#  define CONFIG_EXAMPLES_MARQUEE_DELAY_MS 200
#endif

/* 跑马灯图案表: 每个元素是一帧。
 * 只有 2 个 LED, 就让它俩交替 —— 这就是最简单的跑马灯。
 * 想要更多花样/更多 LED, 直接改这张表即可。
 */

static const userled_set_t g_frames[] =
{
  0x1,   /* 0b01  只亮 PB5 */
  0x2,   /* 0b10  只亮 PE5 */
};

#define NFRAMES (sizeof(g_frames) / sizeof(g_frames[0]))

/****************************************************************************
 * marquee_main
 ****************************************************************************/

int main(int argc, char *argv[])
{
  userled_set_t supported = 0;
  int fd;
  int i;

  /* 1. 打开 LED 设备 */

  fd = open(CONFIG_EXAMPLES_MARQUEE_DEVPATH, O_WRONLY);
  if (fd < 0)
    {
      fprintf(stderr, "marquee: 打开 %s 失败: %d\n",
              CONFIG_EXAMPLES_MARQUEE_DEVPATH, errno);
      return EXIT_FAILURE;
    }

  /* 2. 问硬件支持哪些 LED (本板应返回 0x03) */

  if (ioctl(fd, ULEDIOC_SUPPORTED,
            (unsigned long)(uintptr_t)&supported) == 0)
    {
      printf("marquee: 启动, 硬件 LED 位图 = 0x%02x, 帧间隔 %d ms\n",
             (unsigned int)supported, CONFIG_EXAMPLES_MARQUEE_DELAY_MS);
    }

  /* 3. 无限循环跑马灯。前台运行, Ctrl-C 结束;
   *    或用  nsh> marquee &  放后台, 再  nsh> kill <pid>
   */

  for (i = 0; ; i = (i + 1) % NFRAMES)
    {
      if (ioctl(fd, ULEDIOC_SETALL,
                (unsigned long)g_frames[i]) < 0)
        {
          fprintf(stderr, "marquee: ULEDIOC_SETALL 失败: %d\n", errno);
          break;
        }

      usleep(CONFIG_EXAMPLES_MARQUEE_DELAY_MS * 1000);
    }

  /* 4. 收尾: 关灯 */

  ioctl(fd, ULEDIOC_SETALL, 0);
  close(fd);
  return EXIT_SUCCESS;
}
```

---

## 3. 注册这个 app

新目录要让 apps 的 Kconfig 收录进去。回到内核目录跑一次 menuconfig：

```bash
cd /home/nuttx/nuttxspace/nuttx
make menuconfig
```

`apps/examples/Kconfig` 会在打开时**自动重新生成**（把 `examples/*/Kconfig` 全 source 进来）。
进入 `Application Configuration` → `Examples` → 找到 **`LED marquee (跑马灯) example`**，按 `Y`。

保存退出。

### 手动方式（不开 menuconfig）

```bash
cd /home/nuttx/nuttxspace/nuttx
make apps_preconfig          # 重新生成 apps/examples/Kconfig(收录新目录)
echo 'CONFIG_EXAMPLES_MARQUEE=y' >> .config
echo 'CONFIG_EXAMPLES_MARQUEE_PROGNAME="marquee"' >> .config
echo 'CONFIG_EXAMPLES_MARQUEE_PRIORITY=100' >> .config
echo 'CONFIG_EXAMPLES_MARQUEE_STACKSIZE=2048' >> .config
echo 'CONFIG_EXAMPLES_MARQUEE_DEVPATH="/dev/userleds"' >> .config
echo 'CONFIG_EXAMPLES_MARQUEE_DELAY_MS=200' >> .config
make olddefconfig
```

验证：

```bash
grep MARQUEE .config
```

---

## 4. 编译 + 烧录 + 运行

```bash
cd /home/nuttx/nuttxspace/nuttx
make -j$(nproc)
```

> 报 `Clock skew detected` 就先 `sudo ntpdate -u -b ntp.aliyun.com` 再重编。

烧录：VS Code 任务 `NuttX: 编译并烧录 (J-Link)`，或

```bash
bash /mnt/d/AI/STM32F103ZET6-MINI-SYS/Nuttx_DEMO/debug/run_flash.sh
```

> BOOT0=1 时单独上电不跑固件；用 F5 起调试会话，或先把 BOOT0 跳线拨到 GND。

串口（Windows 端 COM18 @ 115200）：

```
NuttShell (NSH) NuttX-13.0.1-RC0

nsh> ls /dev
 /dev:
  console
  userleds        ← 有它 = USERLED 生效

nsh> marquee
marquee: 启动, 硬件 LED 位图 = 0x03, 帧间隔 200 ms
```

此时 PB5、PE5 交替闪烁 = 跑马灯成功。**Ctrl-C** 停止（停下时最后一帧的灯可能还亮着，代码里 `SETALL(0)` 会在退出前关掉）。

后台运行：

```
nsh> marquee &
nsh> ps                 # 找到 marquee 的 PID
nsh> kill <PID>
```

---

## 5. 改花样

### 改速度

`nsh` 里没法传参，改 `CONFIG_EXAMPLES_MARQUEE_DELAY_MS`（menuconfig 或 `.config`），重编重烧。
或直接改 `marquee_main.c` 里的宏。

### 改图案

改 `g_frames[]` 这张表。例如"呼吸式"来回扫：

```c
static const userled_set_t g_frames[] =
{
  0x1,   /* 01 */
  0x2,   /* 10 */
  0x3,   /* 11  两个都亮 */
  0x0,   /* 00  全灭 */
};
```

或走马灯"拖尾"（2 灯太少看不出，等你接了更多 LED 再玩）：

```c
static const userled_set_t g_frames[] =
{
  0x1, 0x3, 0x2, 0x0,
};
```

### 单独控制某个灯（不用整帧位图）

```c
struct userled_s s;
s.ul_led = 0;      /* 0 = PB5, 1 = PE5 */
s.ul_on  = true;   /* 亮 */
ioctl(fd, ULEDIOC_SETLED, (unsigned long)(uintptr_t)&s);
```

---

## 6. 常见坑

| 现象 | 原因 / 解决 |
|------|-------------|
| `ls /dev` 没有 `userleds` | `CONFIG_USERLED_LOWER` 没开，或 `CONFIG_BOARD_LATE_INITIALIZE` 没开（本板已开）|
| `marquee: 打开 /dev/userleds 失败: 2` | 同上（errno 2 = ENOENT）|
| menuconfig 里找不到 `LED marquee` | 在 nuttx 目录 `make apps_preconfig` 后重开 menuconfig；或 4 个文件名拼错（是 `Make.defs` 不是 `Makefile.defs`）|
| `make` 报 `Application.mk: No such file` | `Makefile` 里 `include $(APPDIR)/Application.mk` 不是最后一行 |
| 编译报 `undefined reference to board_userled_all` | `CONFIG_ARCH_LEDS` 还是 `y`（编的是 autoleds.c）。确认 `.config` 是 `# CONFIG_ARCH_LEDS is not set` |
| 灯一直亮 / 不闪 | 别在 app 里对电平取反；`board_userled_all()` 已处理低有效，`1 = 亮` |
| 跑起来 `nsh>` 卡住不能输入 | `marquee` 前台无限循环，正常。Ctrl-C，或下次用 `marquee &` |
| LED0(PB5) 闪、LED1(PE5) 不闪 | 量 PE5 对地电压（亮时应 ≈0V），查焊点/跳线；`supported` 若为 `0x01` 说明 `BOARD_NLEDS` 或编译没对 |

---

## 7. 一页流程

```
1. 改 .config:  -CONFIG_ARCH_LEDS  +CONFIG_USERLED  +CONFIG_USERLED_LOWER
              → make olddefconfig
2. 建 apps/examples/marquee/{Kconfig, Make.defs, Makefile, marquee_main.c}
3. make menuconfig → Examples → 勾 LED marquee   (或手动 +CONFIG_EXAMPLES_MARQUEE=y)
4. make -j$(nproc)  →  烧录  →  nsh> marquee
5. 改 g_frames[] / DELAY_MS 换花样
```

核心：`open("/dev/userleds")` → 循环 `ioctl(fd, ULEDIOC_SETALL, 位图)`，
`bit0=PB5  bit1=PE5  1=亮`。
