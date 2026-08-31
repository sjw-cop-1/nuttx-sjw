#!/usr/bin/env python3
"""从 make V=1 构建日志解析生成 compile_commands.json（Sourcetrail 使用）
策略：
  - 匹配编译命令行（cc/gcc/... -c ... src.c -o dst.o）
  - 源文件为裸文件名时，用仓库内 basename 索引定位绝对路径
  - directory = 源文件所在目录（NuttX 在源文件目录内编译）
  - 命令原文保留在 command 字段（Sourcetrail 支持 shell 语义解析）
"""
import json
import os
import re
import sys

LOG = sys.argv[1] if len(sys.argv) > 1 else '/tmp/build.log'
ROOTS = sys.argv[2].split(',') if len(sys.argv) > 2 else ['/home/nuttx/nuttxspace/nuttx']
OUT = sys.argv[3] if len(sys.argv) > 3 else os.path.join(ROOTS[0], 'compile_commands.json')

SRC_EXT = ('.c', '.cc', '.cpp', '.cxx', '.S', '.s')
COMPILER_RE = re.compile(r'^(cc|gcc|g\+\+|clang|clang\+\+|arm-none-eabi-gcc|aarch64-none-elf-gcc|riscv64-unknown-elf-gcc)\b')

# 构建 basename -> [绝对路径] 索引（覆盖所有根目录）
name_index = {}
skip_dirs = {'.git', 'staging', 'pass1', 'dummy', 'node_modules'}
for ROOT in ROOTS:
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in skip_dirs]
        for fn in filenames:
            if fn.endswith(SRC_EXT):
                name_index.setdefault(fn, []).append(os.path.join(dirpath, fn))

# 当前配置的板卡名（NuttX 多个板子常含同名源文件, 如 stm32_boot.c,
# 编译经 VPATH 后命令行只留 basename, 需按 .config 的 ARCH_BOARD 消歧）
CONFIG_BOARD = None
_config_path = os.path.join(ROOTS[0], '.config')
if os.path.exists(_config_path):
    _m = re.search(r'^CONFIG_ARCH_BOARD="([^"]+)"',
                   open(_config_path, encoding='utf-8', errors='replace').read(),
                   re.M)
    CONFIG_BOARD = _m.group(1) if _m else None

def match_board(cand):
    """候选路径是否属于当前配置的板卡目录(boards/<arch>/<chip>/<board>/...)"""
    if not CONFIG_BOARD:
        return False
    return ('/boards/' in cand and
            re.search(r'/boards/[^/]+/[^/]+/%s/(src|include)/' %
                      re.escape(CONFIG_BOARD), cand) is not None)

def locate_source(basename, include_dirs):
    """定位源文件绝对路径；多个候选时用命令行中的 -I/-isystem 绝对路径消歧"""
    cands = name_index.get(basename, [])
    if not cands:
        return None
    if len(cands) == 1:
        return cands[0]
    # 消歧 1: 候选文件目录 == -I 目录，或是 -I 目录的子目录
    # （NuttX 在父目录编译子目录文件，-I 常指向编译根目录）
    for c in cands:
        d = os.path.dirname(c)
        for inc in include_dirs:
            inc = inc.rstrip('/')
            if d == inc or d.startswith(inc + '/'):
                return c
    return cands[0]

def locate_source_checked(basename, rel_src, include_dirs):
    """带存在性验证的定位：对每个候选，用 infer_cwd 反推 cwd，
    验证 cwd + rel_src 是否真实存在（模拟 clang 的解析路径）。"""
    cands = name_index.get(basename, [])
    if not cands:
        return None
    if len(cands) == 1:
        return cands[0]
    # 优先 -I 消歧：候选文件目录 == -I 目录，或是 -I 目录的子目录
    inc_norm = [i.rstrip('/') for i in include_dirs]
    inc_hits = [c for c in cands
                if any(os.path.dirname(c) == inc or os.path.dirname(c).startswith(inc + '/')
                       for inc in inc_norm)]
    rel = rel_src.lstrip('./')
    if len(inc_hits) == 1:
        return inc_hits[0]
    if len(inc_hits) > 1:
        # 多个 -I 命中：用存在性验证进一步区分
        for c in inc_hits:
            if os.path.exists(os.path.join(infer_cwd(c, rel), rel)):
                return c
        return inc_hits[0]
    # -I 无命中：存在性验证（cwd + rel_src 真实存在者优先）
    for c in cands:
        if os.path.exists(os.path.join(infer_cwd(c, rel), rel)):
            return c
    # 仍无命中：优先当前配置板卡的源文件（NuttX 同名源文件跨板卡常见）
    for c in cands:
        if match_board(c):
            return c
    return cands[0]

def resolve_from_obj(obj):
    """NuttX apps 构建的 -o 是点分绝对路径形式：
    base.c.home.nuttx.nuttxspace.apps.testing.ostest.o 对应
    /home/nuttx/nuttxspace/apps/testing/ostest/base.c
    返回绝对路径；非此格式返回 None。
    """
    m = re.match(r'^([^/]+\.c)\.([a-zA-Z0-9_.]+)\.o$', obj)
    if not m:
        return None
    base, dots = m.groups()
    path = '/' + dots.replace('.', '/')
    return os.path.join(path, base)

def parse_line(line):
    if '-c' not in line:
        return None
    if not COMPILER_RE.match(line):
        return None
    tokens = line.split()
    # 源文件参数：最后一个以源码扩展名结尾的 token（-o 的目标是 .o，不会误匹配）
    src = None
    for t in reversed(tokens):
        if t.endswith(SRC_EXT):
            src = t
            break
    if not src:
        return None
    mo = re.search(r'-o\s+(\S+)', line)
    obj = mo.group(1) if mo else ''
    # 命令行中的绝对 include 路径（用于消歧）
    inc_dirs = set(re.findall(r'-(?:isystem|I|iquote)\s+(/[^\s]+)', line))
    return src, obj, inc_dirs, line

def infer_cwd(abs_src, rel_src):
    """从命令中的源文件参数反推编译时的真实工作目录。
    NuttX 的编译模型：在父目录编译子目录文件（如 sched 目录内编译 wqueue/kwork_thread.c），
    命令里源文件参数带目录前缀，-o 目标可能是裸文件名或 bin/ 前缀。
    因此 cwd = abs_src 去掉尾部 rel_src 后的部分。
    """
    rel = rel_src.lstrip('./')
    if abs_src.endswith(rel):
        return abs_src[:-len(rel)]
    # 兜底：源文件所在目录
    return os.path.dirname(abs_src)

entries = []
seen = set()
with open(LOG, encoding='utf-8', errors='replace') as f:
    for raw in f:
        line = raw.rstrip('\n')
        if line.endswith('\\'):          # 简单续行合并（NuttX 一般不使用）
            line = line[:-1] + ' ' + next(f, '').strip()
        r = parse_line(line)
        if not r:
            continue
        src, obj, inc_dirs, cmd = r
        # 优先：-o 点分路径格式直接给出源文件绝对路径（NuttX apps 构建）
        abs_from_obj = resolve_from_obj(obj)
        if abs_from_obj and os.path.exists(abs_from_obj):
            abs_src = abs_from_obj
        else:
            base = os.path.basename(src)
            abs_src = locate_source_checked(base, src, inc_dirs)
        if not abs_src:
            print(f'[warn] 未找到源文件: {src}', file=sys.stderr)
            continue
        key = (abs_src, obj)
        if key in seen:
            continue
        seen.add(key)
        cwd = infer_cwd(abs_src, src).rstrip('/')
        entries.append({
            'directory': cwd,
            'file': abs_src,
            'command': cmd,
            'output': obj if os.path.isabs(obj) else os.path.join(cwd, obj),
        })

with open(OUT, 'w', encoding='utf-8') as f:
    json.dump(entries, f, indent=1)

print(f'共解析 {len(entries)} 条编译命令 -> {OUT}')
