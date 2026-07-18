# import tkinter as tk
# from tkinter import messagebox
# import json
# import struct
# import shlex
# import os

# # ===============================================================
# # 状态管理
# # ===============================================================
# current_isa = None
# current_peri = None

# # ===============================================================
# # 自动初始化逻辑
# # ===============================================================
# def auto_init_configs():
#     """程序启动时自动查找同级目录下的配置文件"""
#     global current_isa, current_peri
#     base_dir = os.path.dirname(os.path.abspath(__file__))
#     isa_path = os.path.join(base_dir, "isa.json")
#     peri_path = os.path.join(base_dir, "peri.json")
    
#     # 尝试加载 ISA
#     try:
#         with open(isa_path, 'r', encoding='utf-8') as f:
#             current_isa = json.load(f)
#             lbl_isa.config(text="ISA: 已就绪", fg="green")
#     except:
#         lbl_isa.config(text="ISA: 未找到 (isa.json)", fg="red")
        
#     # 尝试加载 Peri
#     try:
#         with open(peri_path, 'r', encoding='utf-8') as f:
#             current_peri = json.load(f)
#             lbl_peri.config(text="Peri: 已就绪", fg="green")
#     except:
#         lbl_peri.config(text="Peri: 未找到 (peri.json)", fg="red")

# # ===============================================================
# # 编译核心逻辑
# # ===============================================================
# def do_compile():
#     if current_isa is None or current_peri is None:
#         messagebox.showerror("配置缺失", "编译失败：请确保 isa.json 和 peri.json 存在于脚本同级目录下！")
#         return

#     try:
#         OPCODES = current_isa
#         SYSCALLS = current_peri.get("SYSCALLS", {})
#         CLASSES = current_peri.get("CLASSES", {})
#         IOCTL_CMDS = current_peri.get("IOCTL_CMDS", {})
        
#         raw_text = text_code.get("1.0", tk.END)
#         source_code = []
#         for line in raw_text.splitlines():
#             line = line.strip()
#             if not line or line.startswith('#'): continue
#             tokens = shlex.split(line)
#             parsed = []
#             for t in tokens:
#                 if t.isdigit() or (t.startswith('-') and t[1:].isdigit()): parsed.append(int(t))
#                 elif t.startswith('0x'): parsed.append(int(t, 16))
#                 else: parsed.append(t)
#             source_code.append(parsed)
            
#         machine_code = []
#         labels = {}
#         string_pool = b""
#         symbol_table = {}
#         fd_allocator = 1 
        
#         # 预扫描
#         pc = 0
#         for inst in source_code:
#             if inst[0] == "LABEL": labels[inst[1]] = pc
#             else: pc += 16
        
#         # 编译
#         pc = 0
#         for inst in source_code:
#             if inst[0] == "LABEL": continue
#             op, args = inst[0], inst[1:]
            
#             opcode_val, reg0, reg1, reg2 = 0, 0, 0, 0
            
#             if op in symbol_table: 
#                 obj = symbol_table[op]
#                 dev_class = obj["class"]
                
#                 # --- 新增：拦截 WRITE 动作 ---
#                 if args[0] == "WRITE":
#                     opcode_val = OPCODES["SYSCALL"]
#                     reg0 = SYSCALLS.get("WRITE", 4) # 对应 C 里的 SYS_VFS_WRITE (4)
#                     reg1 = obj["fd"]
#                     # 计算偏移量：指令区总长度 + 字符串池当前长度
#                     reg2 = (sum(1 for i in source_code if i[0] != "LABEL") * 16) + len(string_pool)
#                     # 追加字符串并带上 \0 结尾
#                     # string_pool += args[1].encode('utf-8') + b'\0'
#                     string_pool += args[1].replace("\\r", "\r").replace("\\n", "\n").encode('utf-8') + b'\0'
#                 # --- 原有的 IOCTL 逻辑 ---
#                 else:
#                     if dev_class not in CLASSES or args[0] not in CLASSES[dev_class]:
#                         raise ValueError(f"驱动类 '{dev_class}' 不支持动作 '{args[0]}'")
#                     opcode_val = OPCODES["SYSCALL"]
#                     reg0 = SYSCALLS.get("IOCTL", 3)
#                     reg1 = obj["fd"]
#                     reg2 = CLASSES[dev_class][args[0]]
                    
#             elif op == "OPEN":
#                 opcode_val = OPCODES["SYSCALL"]
#                 reg0 = SYSCALLS.get("OPEN", 1)
#                 dev_class = current_peri.get("BINDINGS", {}).get(args[0], "unknown")
#                 if len(args) >= 3 and args[1] == "AS":
#                     symbol_table[args[2]] = {"fd": fd_allocator, "class": dev_class}
#                     reg1, fd_allocator = fd_allocator, fd_allocator + 1
#                 else: reg1 = args[1]
#                 reg2 = (sum(1 for i in source_code if i[0] != "LABEL") * 16) + len(string_pool)
#                 string_pool += args[0].encode('utf-8') + b'\0'
#             elif op in SYSCALLS:
#                 opcode_val = OPCODES["SYSCALL"]
#                 reg0 = SYSCALLS[op]
#                 if op == "IOCTL": reg1, reg2 = int(args[0]), int(args[1])
#             elif op in OPCODES:
#                 opcode_val = OPCODES[op]
#                 if op == "JUMP": reg0 = labels[args[0]] - pc
#                 else: reg0, reg1, reg2 = (int(args[0]), int(args[1]), int(args[2])) if len(args) >= 3 else (int(args[0]), 0, 0)
#             else: raise ValueError(f"未知指令: {op}")
                
#             machine_code.append(struct.pack('<iiii', opcode_val, reg0, reg1, reg2))
#             pc += 16
            
#         final_binary = b"".join(machine_code) + string_pool
#         hex_list = [f"0x{b:02X}" for b in final_binary]
#         out_str = "__attribute__((aligned(4)))\nstatic const uint8_t raw_app_bytecode[] = {\n"
#         for i in range(0, len(hex_list), 16): out_str += "    " + ", ".join(hex_list[i:i+16]) + ",\n"
#         out_str += "};\n"
#         text_out.delete("1.0", tk.END); text_out.insert(tk.END, out_str)
#         messagebox.showinfo("成功", "机器码已生成")
#     except Exception as e:
#         messagebox.showerror("编译错误", str(e))

# # ===============================================================
# # GUI 界面
# # ===============================================================
# root = tk.Tk()
# root.title("VM 编译器 (Auto-Load)")
# root.geometry("800x650")

# status_frame = tk.Frame(root); status_frame.pack(fill=tk.X, padx=10, pady=5)
# lbl_isa = tk.Label(status_frame, text="ISA: 检查中...", fg="gray"); lbl_isa.pack(side=tk.LEFT)
# lbl_peri = tk.Label(status_frame, text="Peri: 检查中...", fg="gray"); lbl_peri.pack(side=tk.LEFT, padx=20)

# text_code = tk.Text(root, font=("Consolas", 12)); text_code.pack(fill=tk.BOTH, expand=True, padx=5)

# # 放入测试串口 WRITE 的默认代码
# text_code.insert("1.0", 'OPEN "sys_uart" AS uart1\nLABEL LOOP\nuart1 WRITE "hello\\r\\n"\nDELAY 1000\nJUMP LOOP')

# tk.Button(root, text="🔥 编译并生成 HEX 🔥", bg="#ff4757", fg="white", font=("微软雅黑", 12), command=do_compile).pack(fill=tk.X, padx=5, pady=5)

# text_out = tk.Text(root, height=10, bg="#2f3542", fg="#7bed9f", font=("Consolas", 10)); text_out.pack(fill=tk.BOTH, padx=5, pady=5)

# auto_init_configs()
# root.mainloop()


# import tkinter as tk
# from tkinter import messagebox
# import json
# import struct
# import shlex
# import os

# # ===============================================================
# # 状态管理
# # ===============================================================
# current_isa = None
# current_peri = None

# # ===============================================================
# # 自动初始化逻辑
# # ===============================================================
# def auto_init_configs():
#     global current_isa, current_peri
#     base_dir = os.path.dirname(os.path.abspath(__file__))
#     isa_path = os.path.join(base_dir, "isa.json")
#     peri_path = os.path.join(base_dir, "peri.json")
    
#     if os.path.exists(isa_path):
#         try:
#             with open(isa_path, 'r', encoding='utf-8') as f:
#                 current_isa = json.load(f)
#             lbl_isa.config(text="ISA: 已就绪", fg="green")
#         except:
#             lbl_isa.config(text="ISA: 读取失败", fg="red")
#     else:
#         lbl_isa.config(text="ISA: 未找到 (isa.json)", fg="red")
        
#     if os.path.exists(peri_path):
#         try:
#             with open(peri_path, 'r', encoding='utf-8') as f:
#                 current_peri = json.load(f)
#             lbl_peri.config(text="Peri: 已就绪", fg="green")
#         except:
#             lbl_peri.config(text="Peri: 读取失败", fg="red")
#     else:
#         lbl_peri.config(text="Peri: 未找到 (peri.json)", fg="red")

# # ===============================================================
# # 编译核心逻辑
# # ===============================================================
# def do_compile():
#     if current_isa is None or current_peri is None:
#         messagebox.showerror("配置缺失", "编译失败：请确保 isa.json 和 peri.json 存在！")
#         return

#     try:
#         OPCODES = current_isa
#         SYSCALLS = current_peri.get("SYSCALLS", {})
#         CLASSES = current_peri.get("CLASSES", {})
        
#         raw_text = text_code.get("1.0", tk.END)
        
#         apps_source = {}
#         current_app = "app0"
#         apps_source[current_app] = []

#         for line in raw_text.splitlines():
#             # 💡 暴力防御：将 '->' 或者 ',' 直接替换为空格，彻底消除标点符号干扰
#             line = line.strip().replace("->", " ").replace(",", " ")
#             if not line or line.startswith('#') or line.startswith(';'): continue
            
#             tokens = shlex.split(line)
#             if not tokens: continue
            
#             if tokens[0].upper().startswith('APP') and tokens[0].endswith(':'):
#                 current_app = tokens[0][:-1].lower() 
#                 apps_source[current_app] = []
#                 continue

#             parsed = []
#             for t in tokens:
#                 if t.startswith('0x'): parsed.append(int(t, 16))
#                 elif t.isdigit() or (t.startswith('-') and t[1:].isdigit()): parsed.append(int(t))
#                 else: parsed.append(t)
            
#             if parsed:
#                 apps_source[current_app].append(parsed)
            
#         out_str = ""
#         app_names = []

#         # --- 编译每个 App ---
#         for app_name, source_code in apps_source.items():
#             if not source_code: continue
#             app_names.append(app_name)
            
#             # 1. 预计算标签
#             labels = {}
#             pc = 0
#             instr_count = sum(1 for i in source_code if i[0] != "LABEL")
#             instr_zone_size = instr_count * 16 
#             for inst in source_code:
#                 if inst[0] == "LABEL": labels[inst[1]] = pc
#                 else: pc += 16
            
#             # 2. 生成机器码
#             machine_code = []
#             string_data = []      
#             current_str_offset = 0 
#             pc = 0
#             symbol_table = {}
#             fd_allocator = 1
            
#             for inst in source_code:
#                 if inst[0] == "LABEL": continue
#                 op, args = inst[0], inst[1:]
#                 opcode_val, reg0, reg1, reg2 = 0, 0, 0, 0
                
#                 def to_int(val, param_name):
#                     try: return int(val)
#                     except: raise ValueError(f"'{op}' 指令的 {param_name} ('{val}') 必须是数字！")

#                 # A: 对象操作 (如: mq1 WRITE "...")
#                 if op in symbol_table:
#                     obj = symbol_table[op]
#                     dev_class = obj["class"]
#                     action = args[0]
                    
#                     if action == "WRITE":
#                         opcode_val = OPCODES["SYSCALL"]
#                         reg0 = SYSCALLS.get("WRITE", 4)
#                         reg1 = obj["fd"]
#                         reg2 = instr_zone_size + current_str_offset
#                         s = args[1].replace("\\r", "\r").replace("\\n", "\n").encode('utf-8') + b'\0'
#                         string_data.append(s)
#                         current_str_offset += len(s)
#                     elif action == "READ":
#                         opcode_val = OPCODES["SYSCALL"]
#                         reg0 = SYSCALLS.get("READ", 2)
#                         reg1 = obj["fd"]
#                         reg2 = to_int(args[1], "目标寄存器")
#                     else:
#                         opcode_val = OPCODES["SYSCALL"]
#                         reg0 = SYSCALLS.get("IOCTL", 3)
#                         reg1 = obj["fd"]
#                         reg2 = CLASSES.get(dev_class, {}).get(action, 0)
                
#                 # B: OPEN
#                 elif op == "OPEN":
#                     opcode_val = OPCODES["SYSCALL"]
#                     reg0 = SYSCALLS.get("OPEN", 1)
#                     dev_class = current_peri.get("BINDINGS", {}).get(args[0], "unknown")
#                     if len(args) >= 3 and args[1] == "AS":
#                         symbol_table[args[2]] = {"fd": fd_allocator, "class": dev_class}
#                         reg1 = fd_allocator
#                         fd_allocator += 1
#                     reg2 = instr_zone_size + current_str_offset
#                     s = args[0].encode('utf-8') + b'\0'
#                     string_data.append(s)
#                     current_str_offset += len(s)

#                 # C: 普通系统调用或基本指令
#                 elif op in SYSCALLS:
#                     opcode_val = OPCODES["SYSCALL"]
#                     reg0 = SYSCALLS[op]
#                     reg1 = to_int(args[0], "参数1") if len(args) > 0 else 0
#                     reg2 = to_int(args[1], "参数2") if len(args) > 1 else 0
#                 elif op in OPCODES:
#                     opcode_val = OPCODES[op]
#                     if op == "JUMP": 
#                         reg0 = labels[args[0]] - pc
#                     else: 
#                         reg0 = to_int(args[0], "参数1") if len(args) > 0 else 0
#                         reg1 = to_int(args[1], "参数2") if len(args) > 1 else 0
#                         reg2 = to_int(args[2], "参数3") if len(args) > 2 else 0
#                 else: raise ValueError(f"未知指令或未定义的对象: {op}")
                
#                 machine_code.append(struct.pack('<iiii', opcode_val, reg0, reg1, reg2))
#                 pc += 16
            
#             # 生成 C 数组
#             final_binary = b"".join(machine_code) + b"".join(string_data)
#             hex_list = [f"0x{b:02X}" for b in final_binary]
            
#             out_str += f"__attribute__((aligned(4)))\nstatic const uint8_t {app_name}_code[] = {{\n"
#             for i in range(0, len(hex_list), 16): 
#                 out_str += "    " + ", ".join(hex_list[i:i+16]) + ",\n"
#             out_str += "};\n\n"

#         out_str += f"static const uint8_t* const raw_app_bytecodes[4] = {{\n"
#         for name in app_names: out_str += f"    {name}_code,\n"
#         for _ in range(max(0, 4 - len(app_names))): out_str += "    NULL,\n"
#         out_str += "};\n"

#         text_out.delete("1.0", tk.END)
#         text_out.insert(tk.END, out_str)
#         messagebox.showinfo("成功", "代码生成成功！")
#     except Exception as e:
#         messagebox.showerror("编译错误", str(e))

# # ===============================================================
# # GUI 界面
# # ===============================================================
# root = tk.Tk()
# root.title("VM 编译器")
# root.geometry("800x700")

# status_frame = tk.Frame(root); status_frame.pack(fill=tk.X, padx=10, pady=5)
# lbl_isa = tk.Label(status_frame, text="ISA: 检查中..."); lbl_isa.pack(side=tk.LEFT)
# lbl_peri = tk.Label(status_frame, text="Peri: 检查中..."); lbl_peri.pack(side=tk.LEFT, padx=20)

# text_code = tk.Text(root, font=("Consolas", 12), height=15); text_code.pack(fill=tk.BOTH, expand=True, padx=5)
# tk.Button(root, text="🔥 编译 🔥", bg="#ff4757", fg="white", font=("微软雅黑", 12), command=do_compile).pack(fill=tk.X, padx=5, pady=5)
# text_out = tk.Text(root, height=12, bg="#2f3542", fg="#7bed9f", font=("Consolas", 10)); text_out.pack(fill=tk.BOTH, padx=5, pady=5)

# auto_init_configs()
# root.mainloop()

import tkinter as tk
from tkinter import messagebox
import json
import struct
import shlex
import os

# ===============================================================
# 状态管理
# ===============================================================
current_isa = None
current_peri = None

# ===============================================================
# 自动初始化逻辑
# ===============================================================
def auto_init_configs():
    global current_isa, current_peri
    base_dir = os.path.dirname(os.path.abspath(__file__))
    isa_path = os.path.join(base_dir, "isa.json")
    peri_path = os.path.join(base_dir, "peri.json")
    
    if os.path.exists(isa_path):
        try:
            with open(isa_path, 'r', encoding='utf-8') as f:
                current_isa = json.load(f)
            lbl_isa.config(text="ISA: 已就绪", fg="green")
        except:
            lbl_isa.config(text="ISA: 读取失败", fg="red")
    else:
        lbl_isa.config(text="ISA: 未找到 (isa.json)", fg="red")
        
    if os.path.exists(peri_path):
        try:
            with open(peri_path, 'r', encoding='utf-8') as f:
                current_peri = json.load(f)
            lbl_peri.config(text="Peri: 已就绪", fg="green")
        except:
            lbl_peri.config(text="Peri: 读取失败", fg="red")
    else:
        lbl_peri.config(text="Peri: 未找到 (peri.json)", fg="red")

# ===============================================================
# 编译核心逻辑
# ===============================================================
def do_compile():
    if current_isa is None or current_peri is None:
        messagebox.showerror("配置缺失", "编译失败：请确保 isa.json 和 peri.json 存在！")
        return

    try:
        OPCODES = current_isa
        SYSCALLS = current_peri.get("SYSCALLS", {})
        CLASSES = current_peri.get("CLASSES", {})
        
        raw_text = text_code.get("1.0", tk.END)
        
        apps_source = {}
        current_app = "app0"
        apps_source[current_app] = []

        for line in raw_text.splitlines():
            # 暴力防御：将 '->' 或者 ',' 直接替换为空格，彻底消除标点符号干扰
            line = line.strip().replace("->", " ").replace(",", " ")
            if not line or line.startswith('#') or line.startswith(';'): continue
            
            tokens = shlex.split(line)
            if not tokens: continue
            
            if tokens[0].upper().startswith('APP') and tokens[0].endswith(':'):
                current_app = tokens[0][:-1].lower() 
                apps_source[current_app] = []
                continue

            parsed = []
            for t in tokens:
                if t.startswith('0x'): parsed.append(int(t, 16))
                elif t.isdigit() or (t.startswith('-') and t[1:].isdigit()): parsed.append(int(t))
                else: parsed.append(t)
            
            if parsed:
                apps_source[current_app].append(parsed)
            
        out_str = ""
        app_names = []
        apps_bin_data_list = [] # 用于收集每个 App 纯净的二进制数据

        # --- 编译每个 App ---
        for app_name, source_code in apps_source.items():
            if not source_code: continue
            app_names.append(app_name)
            
            # 1. 预计算标签
            labels = {}
            pc = 0
            instr_count = sum(1 for i in source_code if i[0] != "LABEL")
            instr_zone_size = instr_count * 16 
            for inst in source_code:
                if inst[0] == "LABEL": labels[inst[1]] = pc
                else: pc += 16
            
            # 2. 生成机器码
            machine_code = []
            string_data = []      
            current_str_offset = 0 
            pc = 0
            symbol_table = {}
            fd_allocator = 1
            
            for inst in source_code:
                if inst[0] == "LABEL": continue
                op, args = inst[0], inst[1:]
                opcode_val, reg0, reg1, reg2 = 0, 0, 0, 0
                
                def to_int(val, param_name):
                    try: return int(val)
                    except: raise ValueError(f"'{op}' 指令的 {param_name} ('{val}') 必须是数字！")

                # A: 对象操作 (如: mq1 WRITE "...")
                if op in symbol_table:
                    obj = symbol_table[op]
                    dev_class = obj["class"]
                    action = args[0]
                    
                    if action == "WRITE":
                        opcode_val = OPCODES["SYSCALL"]
                        reg0 = SYSCALLS.get("WRITE", 4)
                        reg1 = obj["fd"]
                        reg2 = instr_zone_size + current_str_offset
                        s = args[1].replace("\\r", "\r").replace("\\n", "\n").encode('utf-8') + b'\0'
                        string_data.append(s)
                        current_str_offset += len(s)
                    elif action == "READ":
                        opcode_val = OPCODES["SYSCALL"]
                        reg0 = SYSCALLS.get("READ", 2)
                        reg1 = obj["fd"]
                        reg2 = to_int(args[1], "目标寄存器")
                    else:
                        opcode_val = OPCODES["SYSCALL"]
                        reg0 = SYSCALLS.get("IOCTL", 3)
                        reg1 = obj["fd"]
                        reg2 = CLASSES.get(dev_class, {}).get(action, 0)
                
                # B: OPEN
                elif op == "OPEN":
                    opcode_val = OPCODES["SYSCALL"]
                    reg0 = SYSCALLS.get("OPEN", 1)
                    dev_class = current_peri.get("BINDINGS", {}).get(args[0], "unknown")
                    if len(args) >= 3 and args[1] == "AS":
                        symbol_table[args[2]] = {"fd": fd_allocator, "class": dev_class}
                        reg1 = fd_allocator
                        fd_allocator += 1
                    reg2 = instr_zone_size + current_str_offset
                    s = args[0].encode('utf-8') + b'\0'
                    string_data.append(s)
                    current_str_offset += len(s)

                # C: 普通系统调用或基本指令
                elif op in SYSCALLS:
                    opcode_val = OPCODES["SYSCALL"]
                    reg0 = SYSCALLS[op]
                    reg1 = to_int(args[0], "参数1") if len(args) > 0 else 0
                    reg2 = to_int(args[1], "参数2") if len(args) > 1 else 0
                elif op in OPCODES:
                    opcode_val = OPCODES[op]
                    if op == "JUMP": 
                        reg0 = labels[args[0]] - pc
                    else: 
                        reg0 = to_int(args[0], "参数1") if len(args) > 0 else 0
                        reg1 = to_int(args[1], "参数2") if len(args) > 1 else 0
                        reg2 = to_int(args[2], "参数3") if len(args) > 2 else 0
                else: raise ValueError(f"未知指令或未定义的对象: {op}")
                
                machine_code.append(struct.pack('<iiii', opcode_val, reg0, reg1, reg2))
                pc += 16
            
            # 整合单个 App 的二进制数据
            final_binary = b"".join(machine_code) + b"".join(string_data)
            apps_bin_data_list.append(final_binary) # 收集起来供后面打包使用

            # -----------------------------------------------------------
            # 依旧保留生成 C 数组的逻辑 (方便在 GUI 查看和调试)
            # -----------------------------------------------------------
            hex_list = [f"0x{b:02X}" for b in final_binary]
            out_str += f"__attribute__((aligned(4)))\nstatic const uint8_t {app_name}_code[] = {{\n"
            for i in range(0, len(hex_list), 16): 
                out_str += "    " + ", ".join(hex_list[i:i+16]) + ",\n"
            out_str += "};\n\n"

        out_str += f"static const uint8_t* const raw_app_bytecodes[4] = {{\n"
        for name in app_names: out_str += f"    {name}_code,\n"
        for _ in range(max(0, 4 - len(app_names))): out_str += "    NULL,\n"
        out_str += "};\n"

        # ===============================================================
        # 🚀 核心新增：生成带头部目录的 BIN 文件
        # ===============================================================
        magic_word = b'MQVM'
        app_count = len(app_names)
        
        # 头部大小：Magic(4) + Count(4) + (Offset(4) + Size(4)) * Count
        header_size = 8 + app_count * 8
        
        toc_bytes = b""
        payload_bytes = b""
        current_offset = header_size
        
        for app_bin_data in apps_bin_data_list:
            app_size = len(app_bin_data)
            # 将 (当前偏移量, 大小) 写入 TOC，小端模式无符号整型
            toc_bytes += struct.pack('<II', current_offset, app_size)
            payload_bytes += app_bin_data
            current_offset += app_size
            
        final_bin_file = magic_word + struct.pack('<I', app_count) + toc_bytes + payload_bytes
        
        # 写入本地文件
        bin_filename = "apps_bundle.bin"
        with open(bin_filename, "wb") as f:
            f.write(final_bin_file)

        # 更新 GUI
        text_out.delete("1.0", tk.END)
        text_out.insert(tk.END, out_str)
        messagebox.showinfo("成功", f"代码编译成功！\n已在当前目录生成: {bin_filename}")

    except Exception as e:
        messagebox.showerror("编译错误", str(e))

# ===============================================================
# GUI 界面
# ===============================================================
root = tk.Tk()
root.title("VM 编译器 - 支持 BIN 动态打包")
root.geometry("800x700")

status_frame = tk.Frame(root); status_frame.pack(fill=tk.X, padx=10, pady=5)
lbl_isa = tk.Label(status_frame, text="ISA: 检查中..."); lbl_isa.pack(side=tk.LEFT)
lbl_peri = tk.Label(status_frame, text="Peri: 检查中..."); lbl_peri.pack(side=tk.LEFT, padx=20)

text_code = tk.Text(root, font=("Consolas", 12), height=15); text_code.pack(fill=tk.BOTH, expand=True, padx=5)
tk.Button(root, text="🔥 编译并生成 BIN 🔥", bg="#ff4757", fg="white", font=("微软雅黑", 12), command=do_compile).pack(fill=tk.X, padx=5, pady=5)
text_out = tk.Text(root, height=12, bg="#2f3542", fg="#7bed9f", font=("Consolas", 10)); text_out.pack(fill=tk.BOTH, padx=5, pady=5)

auto_init_configs()
root.mainloop()