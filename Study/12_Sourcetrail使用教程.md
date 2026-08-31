# 12 Sourcetrail 可视化阅读 NuttX 教程（初学者版）

> 本文手把手教你在 Sourcetrail 里阅读 NuttX 源码。**不需要任何经验**，
> 从"双击图标"讲到"看懂调用图"。建议打开 Sourcetrail 跟着做一遍。
>
> 配套阅读：10_源码阅读路线图.md（读哪些代码）、11_编译教程.md（怎么编译）。
> 本文只讲工具本身怎么用。

---

## 0. 你会学到什么

学完本文你将能：

1. 启动 Sourcetrail 并认识它的 4 个区域
2. 搜索任意函数/变量/宏（如 `nx_start`）
3. 看懂"谁调用了它、它调用了谁"的关系图
4. 从代码跳到定义、从定义跳到调用者，来回穿梭
5. 结合 Study 文档按图索骥地读 NuttX

全程约 30~60 分钟。

---

## 1. Sourcetrail 是什么（30 秒理解）

Sourcetrail 是一个**源码可视化工具**：它先把整个项目分析一遍
（我们已经做好了，索引了 1201 个文件），然后让你：

- **点一个函数**，立刻看到"谁调用了它"（左侧）和"它调用了谁"（右侧）
- **点任意符号**，立刻跳到它的定义处看代码
- 像看地图一样在几十万行代码里导航

一句话：**它是给 C/C++ 代码用的"高德地图"**。
传统上我们靠 `grep` 找函数、靠记忆拼调用链，Sourcetrail 把这件事变成了"点击"。

---

## 2. 环境准备（已就绪，确认即可）

本机的 Sourcetrail 学习环境已经全部配好：

| 项目 | 状态 | 说明 |
|------|------|------|
| 索引数据库 | ✅ 已建好 | /home/nuttx/nuttxspace/nuttx.srctrldb（1201 文件，10293 个符号节点） |
| 编译数据库 | ✅ 已生成 | /home/nuttx/nuttxspace/nuttx/compile_commands.json（1201 条编译命令） |
| 工程文件 | ✅ 已配置 | /home/nuttx/nuttxspace/nuttx.srctrlprj（源码根：nuttx + apps） |
| GUI | ✅ 可启动 | 通过 `sourcetrail` 命令（已处理 Windows 高 DPI 缩放） |

> 索引数据源是 `compile_commands.json`，它由编译日志解析而来（见
> Study/tools/parse_compile_commands.py）。**代码改了、配置换了，就要重新
> 编译并重建索引**，否则 Sourcetrail 里看到的是旧代码（第 10 节有命令）。

---

## 3. 启动 Sourcetrail

打开终端（WSL），输入：

```bash
sourcetrail /home/nuttx/nuttxspace/nuttx.srctrlprj
```

或者不带参数启动后手动打开工程：

```bash
sourcetrail
# 菜单 File → Open Project → 选择 nuttx.srctrlprj
```

启动后会在 Windows 桌面弹出一个窗口（WSLg 自动显示）。
看到左下角状态栏显示 **"Indexing finished"** 之类，说明已就绪。

> 窗口字体如果偏小/偏大，是因为 Windows 缩放与 WSLg 的 DPI 换算，
> 本机已通过 ~/apps/sourcetrail.sh 适配（QT_SCALE_FACTOR=1），一般无需调整。

---

## 4. 界面布局（先认识这 4 个区域）

Sourcetrail 主窗口分为 4 块（大致示意）：

```
+------------------------------------------------------------------+
| 菜单栏 / 工具栏   [搜索框..............]   [索引状态]   [激活]    |
+----------------------------------------+-------------------------+
|  [Search] [Code]  ← 左侧两个标签页      |   Graph View（图形视图）|
|  搜索结果列表（符号）                    |    关系图主战场，最常用  |
|                                        |                         |
|  代码视图（当前符号的定义/引用处）        |  右侧面板：             |
|                                        |  [Legend] 颜色图例      |
|                                        |  [Overview] 概览        |
+----------------------------------------+-------------------------+
```

各区域作用：

| 区域 | 名字 | 干什么用 |
|------|------|----------|
| 左侧上半 | **Search（搜索）** | 输入符号名，列出所有匹配的符号 |
| 左侧下半 | **Code（代码）** | 显示当前符号的代码（定义处高亮） |
| 中间 | **Graph View（图形视图）** | 符号关系图：点一个符号，周围展开它的调用者/被调用者 |
| 右侧 | **Legend / Overview** | Legend 是图例（节点颜色含义）；Overview 是当前符号的统计信息 |

> 新手最容易忽略右侧的 **Legend（图例）**：它告诉你图形里
> **每种颜色代表什么**（函数、类型、宏、变量……），看不懂图先看它。

---

## 5. 第一次搜索：`nx_start` 全程演示

`nx_start` 是 NuttX 的启动入口函数（在 sched/init/nx_start.c）。
我们用它走一遍完整操作，**请跟着做**：

### 第 1 步：打开全局搜索

按 **Ctrl+Shift+F**（或点工具栏的搜索按钮）。
顶部会出现一个搜索框。

### 第 2 步：输入符号名

输入 `nx_start`。下方立刻列出匹配结果，类似：

```
nx_start                    (void)   nx_start.c
nx_start_internal           (void)   nx_start.c
nx_start_thread             ...
```

> 注意：结果里可能有多个同名符号（不同文件/不同重载），
> 看后面的**文件名**区分，选 `nx_start.c` 里那个。

### 第 3 步：双击结果

双击 `nx_start`。会发生三件事：

1. **中间图形视图**出现一个大节点 `nx_start`，周围连着很多小节点
2. **左侧 Code** 显示 `nx_start.c` 中 `nx_start` 函数的代码
3. **右侧 Overview** 显示它的信息（定义文件、行号等）

### 第 4 步：看懂图形视图

现在看中间。`nx_start` 是中心，连出去的每条线都是一个**关系**：

- **线指向左侧的节点 = 谁调用了 nx_start**（它的调用者，通常是 `main` 或启动汇编）
- **线指向右侧的节点 = nx_start 调用了谁**（如 `nx_start_internal`、`nxsem_init`…）

> 这就是 Sourcetrail 的核心读法：
> **选中一个符号 → 左边是"输入"（谁引用它），右边是"输出"（它引用谁）**。

### 第 5 步：顺着调用链走

- **想看 nx_start 调用的某个函数**：双击右边那个节点（如 `nx_start_internal`），
  它就变成新的中心，右侧继续展开它调用了谁——像翻书一样一页页往下走。
- **想看谁调用了 nx_start**：双击左边的节点（如 `main`）。
- **想看某段代码**：单击图形里的节点，左侧 Code 区就会跳到对应代码，
  你可以滚动阅读，再双击代码里的其他符号继续跳。

### 第 6 步：回退

工具栏有 **返回/前进** 按钮（浏览器风格），随时回到上一步看的符号。

> 到此你已经会用 Sourcetrail 了。剩下的就是"多点、多走"。

---

## 6. 图形视图速成（看图的 6 个技巧）

| 操作 | 做法 | 效果 |
|------|------|------|
| 选中符号 | 单击节点 | 左侧 Code 跳到对应代码，右侧显示信息 |
| 展开符号 | 双击节点 | 该节点成为中心，展开它的调用关系 |
| 收起分支 | 再次双击已展开的节点 | 收起周围节点，图变清爽 |
| 缩放 | 鼠标滚轮 | 放大/缩小，看细节或看全局 |
| 平移 | 按住鼠标左键拖拽空白处 | 移动画布 |
| 查看颜色含义 | 看右侧 **Legend** 面板 | 每种颜色 = 一种符号类型 |

读图心法：

1. **先找中心，再看左右**——永远问"这个符号被谁用？它用了谁？"
2. **节点上带箭头/方向的边**才有意义，注意线的方向
3. 图太乱时：把无关的节点**收起**（双击），只留当前关心的链
4. 想知道"这个符号在哪个文件哪一行"：看左侧 Code 顶部，或右侧 Overview

---

## 7. 实战演练：跟踪 `hello` 程序的一生

结合 Study 文档第 1~3 周的内容，做一次完整的"追踪之旅"。
每个步骤都在 Sourcetrail 里完成：

### 演练 1：hello 应用入口 → 系统调用（对应第 3 周"系统调用"）

1. 搜索 `hello_main`（apps/examples/hello/hello_main.c，NSH 里敲 hello 就是运行它）
2. 双击它，右侧看它调用了谁（应该能看到 `printf` 等）
3. 点 `printf`，继续跟到 libc → 系统调用 stub → 内核 `write`/`sys_write`
   ——这就是 08_系统调用.md 讲的 "libc → 内核" 全链路，用图走一遍比读十遍都清楚

### 演练 2：启动流程（对应第 1 周"看懂启动"）

1. 搜索 `nx_start`，双击
2. 顺着右侧链走：`nx_start` → `nx_start_internal` → `nxsched_add_readytorun` → …
3. 对照 02_启动流程.md，把文档里讲的每个函数都点一遍，看它们真实的调用关系
4. 尝试不看文档，仅凭调用图说出启动顺序（这就是路线图 8 的终极练习）

### 演练 3：任务调度（对应第 2 周"调度算法"）

1. 搜索 `nxsched_add_readytorun`（注意：是 `add_` 下划线，见 sched/sched/sched_addreadytorun.c）
2. 看左侧——谁调用了它（`nxtask_start`、`sem_post` 等都会经过它）
3. 看右侧——它调用了什么（`nxsched_add_prioritized` 等）
4. 把 03_任务调度.md 里的"就绪队列"概念和这张图对应起来

> 每次演练的产出：**画一张图**（截图或手绘），标注关键函数和调用方向。

---

## 8. 快捷键速查表（记 3 个就够）

| 快捷键 | 功能 | 优先级 |
|--------|------|--------|
| **Ctrl+Shift+F** | 全局搜索符号（最常用！） | ⭐⭐⭐ |
| **双击** 符号/节点 | 在图形视图中展开 | ⭐⭐⭐ |
| **单击** 节点 | 查看代码/信息 | ⭐⭐⭐ |
| Ctrl+B | 给当前符号加书签（下次快速找回） | ⭐⭐ |
| Ctrl+Z / Ctrl+Y | 撤销 / 重做 | ⭐ |
| Esc | 关闭弹出框/取消 | ⭐ |
| 滚轮 / 拖拽 | 缩放 / 平移图形 | ⭐⭐ |

> 记不住没关系：**所有操作都能用鼠标完成**，右键菜单和工具栏按钮
> 都有对应功能。快捷键只是提速。

---

## 9. 学习方法：文档 + Sourcetrail 三步法

这是把 Study 文档和 Sourcetrail 结合起来读源码的推荐流程：

```
第 1 步  读文档，圈出关键函数名
        （每篇文档里都有"材料"列，如 03_任务调度.md 的
        sched_addreadytorun.c、sem_wait.c）

第 2 步  在 Sourcetrail 里搜这个函数，双击展开
        看它的调用者/被调用者，形成"关系地图"

第 3 步  回到文档，把看到的调用链补充到文档上
        （文档 8 节说"在文档上做笔记"——记下真实函数名、
        文件路径、调用顺序，文档就变成了你自己的地图）
```

三个原则：

1. **跟着一个函数走到底**，不要跳来跳去——走完一条链再换
2. **先看数据结构**（`struct xxx_s` 的字段），再看函数逻辑
   ——在 Sourcetrail 里搜结构体名，右侧能看到所有使用它的函数
3. **每读完一个子系统，画一张图**——能画出来才算真懂了

---

## 10. 常见问题（FAQ）

### Q1：改了代码/换了配置，Sourcetrail 里还是旧代码？

索引不会自动更新。流程是：**重新编译 → 重新生成 compile_commands.json → 重新索引**：

```bash
cd /home/nuttx/nuttxspace/nuttx
make -j28 V=1 > /tmp/build.log 2>&1          # 重新编译
python3 Study/tools/parse_compile_commands.py \
    /tmp/build.log \
    "/home/nuttx/nuttxspace/nuttx,/home/nuttx/nuttxspace/apps" \
    /home/nuttx/nuttxspace/nuttx/compile_commands.json
sourcetrail index --full --project-file=/home/nuttx/nuttxspace/nuttx.srctrlprj
```

（本机全量索引约 45 秒；GUI 里点菜单栏 "Index" 按钮也可以做增量索引）

### Q2：搜不到某个函数？

- 检查拼写（如 `nxsched_add_readytorun` 中间有下划线）
- 该符号可能**没有被编译**（索引只包含编译过的文件）——换个配置编译后再索引
- 用 `grep -rn "函数名" sched/ fs/` 确认函数确实存在

### Q3：窗口字体太大/太小？

本机已通过 ~/apps/sourcetrail.sh 适配（QT_SCALE_FACTOR=1，WSLg 自动放大）。
若换显示器后异常，调整该脚本里的 `QT_SCALE_FACTOR`（1 或 2 试一下）。

### Q4：图形太乱，看不清楚？

- 双击已展开的节点**收起**分支
- 放大（滚轮）聚焦小范围
- 用书签（Ctrl+B）记住重要符号，避免反复搜索

### Q5：Sourcetrail 打不开/闪退？

```bash
# 确认命令存在
which sourcetrail
# 前台运行看报错（不要加 &）
sourcetrail /home/nuttx/nuttxspace/nuttx.srctrlprj
```

---

## 11. 附录：本机 Sourcetrail 环境速查

| 项 | 值 |
|----|----|
| 程序 | ~/apps/Sourcetrail.AppImage（2021.4.19），命令 `sourcetrail` |
| 工程文件 | /home/nuttx/nuttxspace/nuttx.srctrlprj |
| 索引数据库 | /home/nuttx/nuttxspace/nuttx.srctrldb |
| 编译数据库 | /home/nuttx/nuttxspace/nuttx/compile_commands.json（1201 条） |
| 索引范围 | nuttx 内核 + apps 应用（排除 Study/、Documentation/、staging/） |
| 索引规模 | 1201 文件、220135 行代码、10293 个符号、50303 条关系 |
| 索引耗时 | 全量约 45 秒（28 核） |

---

*本文所有示例符号（nx_start、nxsched_add_readytorun、sem_post、mm_malloc、
fs_open、hello_main）均已在本机索引库中验证存在。*
