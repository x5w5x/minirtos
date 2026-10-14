#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Keil MDK .map 转 Handheld DAP 二进制符号表 (.sym)
- 提取 Flash Thumb-2 函数 (用于反汇编定位与函数名显示)
- 提取 SRAM 全局/静态变量 (用于变量监控面板与实时热读写)
- 生成紧凑二进制 .sym 文件，直接复制进 TF 卡即可加载
"""

import os
import re
import sys
import struct

MAP_FILE = "Listings/Project.map"  # Keil map 默认路径，可按需修改
OUT_SYM  = "app.sym"               # 导出的二进制符号文件名

# 1. 函数匹配正则: Symbol_Name   0x08000244   Thumb Code   46   delay.o(...)
FUNC_PATTERN = re.compile(
    r"^\s*([a-zA-Z0-9_$]+)\s+(0x[0-9a-fA-F]+)\s+Thumb Code\s+(\d+)\s+"
)

# 2. 全局变量匹配正则: Symbol_Name   0x20000004   Data   4   main.o(.data)
VAR_PATTERN = re.compile(
    r"^\s*([a-zA-Z0-9_$]+)\s+(0x20[0-9a-fA-F]{6})\s+Data\s+(\d+)\s+"
)

def parse_map(map_path):
    funcs = []
    vars_list = []

    if not os.path.exists(map_path):
        print(f"[错误] 找不到 Map 文件: {map_path}")
        return funcs, vars_list

    with open(map_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            # --- A. 提取函数符号 ---
            mf = FUNC_PATTERN.match(line)
            if mf:
                name, addr_str, size_str = mf.groups()
                addr = int(addr_str, 16) & ~1  # 剥除 Thumb LSB (Bit0)
                size = int(size_str)
                # 过滤汇编临时标号与零长度项
                if not name.startswith(("$", ".")) and size > 0:
                    funcs.append({"name": name, "addr": addr, "size": size})
                continue

            # --- B. 提取全局变量 (SRAM 0x20xxxxxx 区域) ---
            mv = VAR_PATTERN.match(line)
            if mv:
                name, addr_str, size_str = mv.groups()
                addr = int(addr_str, 16)
                size = int(size_str)
                # 过滤堆栈内部保留段 (Stack_Mem, Heap_Mem 等超大块)，限制监控普通变量 (1~256 字节)
                if not name.startswith(("$", ".")) and 0 < size <= 256:
                    if name not in ("Stack_Mem", "Heap_Mem", "__initial_sp"):
                        vars_list.append({"name": name, "addr": addr, "size": size})

    # 地址去重：优先保留名称更具体的符号
    unique_funcs = {}
    for f in funcs:
        if f["addr"] not in unique_funcs or len(f["name"]) > len(unique_funcs[f["addr"]]["name"]):
            unique_funcs[f["addr"]] = f

    unique_vars = {}
    for v in vars_list:
        if v["addr"] not in unique_vars or len(v["name"]) > len(unique_vars[v["addr"]]["name"]):
            unique_vars[v["addr"]] = v

    # 必须严格按地址升序排序 (供 Pico 内部进行极速二分查找)
    sorted_funcs = sorted(unique_funcs.values(), key=lambda x: x["addr"])
    sorted_vars  = sorted(unique_vars.values(), key=lambda x: x["addr"])

    return sorted_funcs, sorted_vars

def build_sym_binary(funcs, vars_list, out_path):
    str_pool = bytearray()
    name_to_offset = {}

    def get_str_offset(name: str) -> int:
        if name not in name_to_offset:
            offset = len(str_pool)
            if offset > 65535:
                print(f"[警告] 字符串池超过 64KB，偏移溢出: {name}")
            name_to_offset[name] = offset
            str_pool.extend(name.encode("ascii", errors="replace") + b"\x00")
            return offset
        return name_to_offset[name]

    # 1. 打包函数表 (每个表项 8 字节: [Addr: 4B] [Size: 2B] [StrOffset: 2B])
    func_table_bytes = bytearray()
    for f in funcs:
        offset = get_str_offset(f["name"])
        func_table_bytes.extend(struct.pack("<IHH", f["addr"], f["size"] & 0xFFFF, offset & 0xFFFF))

    # 2. 打包变量表 (每个表项 8 字节: [Addr: 4B] [Size: 1B] [TypeHint: 1B] [StrOffset: 2B])
    var_table_bytes = bytearray()
    for v in vars_list:
        offset = get_str_offset(v["name"])
        size_byte = min(v["size"], 255)
        type_hint = 0  # 预留类型: 0=uint, 1=int, 2=hex, 3=float
        var_table_bytes.extend(struct.pack("<IBBH", v["addr"], size_byte, type_hint, offset & 0xFFFF))

    # 3. 打包文件头 (10 字节: Magic(4B) + Ver(2B) + FuncCount(2B) + VarCount(2B))
    header_bytes = struct.pack("<4sHHH", b"SYMB", 2, len(funcs), len(vars_list))

    # 4. 写入输出文件
    with open(out_path, "wb") as f:
        f.write(header_bytes)
        f.write(func_table_bytes)
        f.write(var_table_bytes)
        f.write(str_pool)

    total_size = len(header_bytes) + len(func_table_bytes) + len(var_table_bytes) + len(str_pool)
    print("=" * 50)
    print(f"  成功导出: {out_path}")
    print(f"  - 函数数量: {len(funcs):4d} 个 ({len(func_table_bytes)} 字节)")
    print(f"  - 变量数量: {len(vars_list):4d} 个 ({len(var_table_bytes)} 字节)")
    print(f"  - 字符池大: {len(str_pool):4d} 字节")
    print(f"  - 文件总计: {total_size:4d} 字节 ({total_size / 1024:.2f} KB)")
    print("=" * 50)

if __name__ == "__main__":
    map_file = sys.argv[1] if len(sys.argv) > 1 else MAP_FILE
    out_file = sys.argv[2] if len(sys.argv) > 2 else OUT_SYM
    funcs, vars_list = parse_map(map_file)
    if funcs or vars_list:
        build_sym_binary(funcs, vars_list, out_file)