# import tkinter as tk
# from tkinter import messagebox, ttk, filedialog
# import json
# import struct
# import shlex
# import os
# import serial
# import time
# import socket
# import threading
# import sys

# # ===============================================================
# # 宏定义与状态管理
# # ===============================================================
# current_isa = None
# current_peri = None
# current_filepath = None  # 记录当前打开的文件路径

# # RTT 配置
# HOST = '127.0.0.1'
# CMD_PORT = 4444
# RTT_PORT = 8765
# RTT_SETUP_CMD = 'rtt setup 0x20000000 0x5000 "SEGGER RTT"\n'

# # IAP 配置
# IAP_HEADER      = 0xA5
# IAP_TAIL        = 0x5A
# CMD_START       = 0x01
# CMD_DATA        = 0x02
# CMD_FINISH      = 0x03
# IAP_ACK         = 0x06
# IAP_NACK        = 0x15
# IAP_DATA_MAX    = 256
# APP_START_ADDR  = 0x08000000 + 16 * 1024  # 0x08004000

# # ===============================================================
# # 示例代码库
# # ===============================================================
# EXAMPLES = {
#     "1. 基础闪灯 (Blink)": """APP0:
# # 初始化系统LED
# OPEN sys_led AS led1
# LABEL main_loop
# led1 TOGGLE
# DELAY 500
# JUMP main_loop
# """,
#     "2. 串口打印 (UART)": """APP0:
# # 初始化串口
# OPEN sys_uart AS uart1
# LABEL loop
# uart1 WRITE "Hello from X-Studio!\\r\\n"
# DELAY 1000
# JUMP loop
# """,
#     "3. 多任务并发 (LED + UART)": """APP0:
# # 任务0：高频闪灯
# OPEN sys_led AS led1
# LABEL l1
# led1 TOGGLE
# DELAY 200
# JUMP l1

# APP1:
# # 任务1：定时发送心跳
# OPEN sys_uart AS u1
# LABEL l2
# u1 WRITE "Ping...\\r\\n"
# DELAY 1000
# JUMP l2
# """
# }

# # ===============================================================
# # GUI 日志重定向 (线程安全)
# # ===============================================================
# def log_gui(msg, end="\n"):
#     def append():
#         text_log.config(state=tk.NORMAL)
#         text_log.insert(tk.END, msg + end)
#         text_log.see(tk.END)
#         text_log.config(state=tk.DISABLED)
#     root.after(0, append)

# # ===============================================================
# # 1. CRC 与 IAP 烧录底层逻辑
# # ===============================================================
# def stm32_crc32_word(crc, word):
#     crc ^= word
#     for _ in range(32):
#         if crc & 0x80000000:
#             crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF
#         else:
#             crc = (crc << 1) & 0xFFFFFFFF
#     return crc

# def calc_stm32_crc(data: bytes) -> int:
#     crc = 0xFFFFFFFF
#     length = len(data)
#     i = 0
#     while i + 4 <= length:
#         word = data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24)
#         crc = stm32_crc32_word(crc, word)
#         i += 4
#     if i < length:
#         last = 0
#         for j in range(length - i):
#             last |= (data[i+j] << (8 * j))
#         crc = stm32_crc32_word(crc, last)
#     return crc

# class IAP_Host:
#     def __init__(self, port, baudrate=115200):
#         self.ser = None
#         try:
#             self.ser = serial.Serial(port, baudrate, timeout=2.0)
#             log_gui(f"[+] 串口 {port} 打开成功")
#         except Exception as e:
#             log_gui(f"[-] 串口打开失败: {e}")

#     def wait_ack(self):
#         if not self.ser: return False
#         res = self.ser.read(1)
#         if not res:
#             log_gui("[-] 接收超时！")
#             return False
#         if res[0] == IAP_ACK: return True
#         elif res[0] == IAP_NACK:
#             log_gui(f"[-] 收到 NACK (0x15)！")
#             return False
#         else:
#             log_gui(f"[-] 收到未知响应: 0x{res[0]:02X}")
#             return False

#     def send_frame(self, cmd, size, addr, crc, data_bytes):
#         head = struct.pack('<BBHII', IAP_HEADER, cmd, size, addr, crc)
#         tail = struct.pack('<B', IAP_TAIL)
#         frame = head + data_bytes + tail
#         self.ser.write(frame)
#         self.ser.flush()
#         return self.wait_ack()

#     def upgrade(self, firmware: bytes):
#         if not self.ser: return
#         total_size = len(firmware)
#         total_crc = calc_stm32_crc(firmware)
#         log_gui(f"[*] 准备烧录... 总大小: {total_size} 字节, CRC: 0x{total_crc:08X}")

#         log_gui("[*] 发送 START 帧 (正在擦除 Flash)...")
#         start_data = struct.pack('<I', total_size) 
#         if not self.send_frame(CMD_START, 4, 0, total_crc, start_data):
#             log_gui("[-] START 帧失败！")
#             return

#         offset = 0
#         while offset < total_size:
#             chunk = firmware[offset : offset + IAP_DATA_MAX]
#             chunk_crc = calc_stm32_crc(chunk)
#             target_addr = APP_START_ADDR + offset
#             if not self.send_frame(CMD_DATA, len(chunk), target_addr, chunk_crc, chunk):
#                 log_gui(f"[-] DATA 帧失败 (偏移量 {offset})！")
#                 return
#             offset += len(chunk)

#         log_gui("[*] 发送 FINISH 帧 (整包校验)...")
#         if not self.send_frame(CMD_FINISH, 0, 0, 0, b''):
#             log_gui("[-] FINISH 帧失败！校验未通过。")
#             return

#         log_gui("[+] 升级圆满成功！🚀")

# # ===============================================================
# # 2. 线程任务：RTT 与 IAP 烧录
# # ===============================================================
# def thread_rtt_listen():
#     log_gui(f"[*] 连接 OpenOCD 控制台 ({CMD_PORT})...")
#     try:
#         with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
#             s.connect((HOST, CMD_PORT))
#             time.sleep(0.2)
#             s.recv(1024) 
#             for cmd in [RTT_SETUP_CMD, 'rtt start\n', f'rtt server start {RTT_PORT} 0\n']:
#                 s.sendall(cmd.encode('utf-8'))
#                 time.sleep(0.2)
#                 s.recv(1024)
#         log_gui("[+] OpenOCD RTT 配置完毕！")
#     except Exception as e:
#         log_gui(f"[!] 配置 RTT 失败: {e}\n(确认 OpenOCD 已启动)")
#         return

#     time.sleep(0.5) 
#     log_gui(f"[*] 连接 RTT 数据通道 ({RTT_PORT})...")
#     try:
#         with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
#             s.connect((HOST, RTT_PORT))
#             log_gui("[+] 日志通道连接成功！(等待单片机输出...)\n" + "="*40)
#             while True:
#                 data = s.recv(1024)
#                 if not data:
#                     log_gui("\n[-] RTT 连接已断开。")
#                     break
#                 text = data.decode('utf-8', errors='ignore')
#                 log_gui(text, end='')
#     except Exception as e:
#         log_gui(f"[!] RTT 数据接收失败: {e}")

# def thread_upload_task(port):
#     if not os.path.exists("apps_bundle.bin"):
#         log_gui("[-] 未找到 apps_bundle.bin，请先点击编译！")
#         return
#     btn_upload.config(state=tk.DISABLED)
#     try:
#         with open("apps_bundle.bin", "rb") as f:
#             firmware = f.read()
#         host = IAP_Host(port=port, baudrate=115200)
#         host.upgrade(firmware)
#         if host.ser: host.ser.close()
#     except Exception as e:
#         log_gui(f"[-] 烧录过程发生异常: {e}")
#     finally:
#         btn_upload.config(state=tk.NORMAL)

# def on_btn_upload():
#     port = entry_port.get().strip()
#     if not port:
#         messagebox.showwarning("提示", "请输入串口号！(如 COM17)")
#         return
#     threading.Thread(target=thread_upload_task, args=(port,), daemon=True).start()

# def on_btn_rtt():
#     btn_rtt.config(state=tk.DISABLED)
#     threading.Thread(target=thread_rtt_listen, daemon=True).start()

# # ===============================================================
# # 3. 编译器核心逻辑 (无缝集成)
# # ===============================================================
# def auto_init_configs():
#     global current_isa, current_peri
#     base_dir = os.path.dirname(os.path.abspath(__file__))
#     try:
#         with open(os.path.join(base_dir, "isa.json"), 'r', encoding='utf-8') as f:
#             current_isa = json.load(f)
#         lbl_isa.config(text="ISA: 就绪", fg="#2ed573")
#     except: lbl_isa.config(text="ISA: 失败", fg="#ff4757")
        
#     try:
#         with open(os.path.join(base_dir, "peri.json"), 'r', encoding='utf-8') as f:
#             current_peri = json.load(f)
#         lbl_peri.config(text="Peri: 就绪", fg="#2ed573")
#     except: lbl_peri.config(text="Peri: 失败", fg="#ff4757")

# def do_compile():
#     if current_isa is None or current_peri is None:
#         messagebox.showerror("配置缺失", "编译失败：请确保 isa.json 和 peri.json 存在！")
#         return
#     try:
#         OPCODES, SYSCALLS, CLASSES = current_isa, current_peri.get("SYSCALLS", {}), current_peri.get("CLASSES", {})
#         raw_text = text_code.get("1.0", tk.END)
#         apps_source, current_app = {}, "app0"
#         apps_source[current_app] = []

#         for line in raw_text.splitlines():
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
#             if parsed: apps_source[current_app].append(parsed)
            
#         app_names, apps_bin_data_list = [], []
#         for app_name, source_code in apps_source.items():
#             if not source_code: continue
#             app_names.append(app_name)
#             labels, pc = {}, 0
#             instr_count = sum(1 for i in source_code if i[0] != "LABEL")
#             instr_zone_size = instr_count * 16 
#             for inst in source_code:
#                 if inst[0] == "LABEL": labels[inst[1]] = pc
#                 else: pc += 16
            
#             machine_code, string_data, current_str_offset, pc, symbol_table, fd_allocator = [], [], 0, 0, {}, 1
#             for inst in source_code:
#                 if inst[0] == "LABEL": continue
#                 op, args = inst[0], inst[1:]
#                 opcode_val, reg0, reg1, reg2 = 0, 0, 0, 0
#                 def to_int(val, param_name):
#                     try: return int(val)
#                     except: raise ValueError(f"'{op}' 的 {param_name} ('{val}') 必须是数字！")

#                 if op in symbol_table:
#                     obj = symbol_table[op]; dev_class = obj["class"]; action = args[0]
#                     if action == "WRITE":
#                         opcode_val, reg0, reg1 = OPCODES["SYSCALL"], SYSCALLS.get("WRITE", 4), obj["fd"]
#                         reg2 = instr_zone_size + current_str_offset
#                         s = args[1].replace("\\r", "\r").replace("\\n", "\n").encode('utf-8') + b'\0'
#                         string_data.append(s); current_str_offset += len(s)
#                     elif action == "READ":
#                         opcode_val, reg0, reg1, reg2 = OPCODES["SYSCALL"], SYSCALLS.get("READ", 2), obj["fd"], to_int(args[1], "目标寄存器")
#                     else:
#                         opcode_val, reg0, reg1, reg2 = OPCODES["SYSCALL"], SYSCALLS.get("IOCTL", 3), obj["fd"], CLASSES.get(dev_class, {}).get(action, 0)
#                 elif op == "OPEN":
#                     opcode_val, reg0 = OPCODES["SYSCALL"], SYSCALLS.get("OPEN", 1)
#                     dev_class = current_peri.get("BINDINGS", {}).get(args[0], "unknown")
#                     if len(args) >= 3 and args[1] == "AS":
#                         symbol_table[args[2]] = {"fd": fd_allocator, "class": dev_class}
#                         reg1, fd_allocator = fd_allocator, fd_allocator + 1
#                     reg2 = instr_zone_size + current_str_offset
#                     s = args[0].encode('utf-8') + b'\0'
#                     string_data.append(s); current_str_offset += len(s)
#                 elif op in SYSCALLS:
#                     opcode_val, reg0 = OPCODES["SYSCALL"], SYSCALLS[op]
#                     reg1 = to_int(args[0], "参数1") if len(args) > 0 else 0
#                     reg2 = to_int(args[1], "参数2") if len(args) > 1 else 0
#                 elif op in OPCODES:
#                     opcode_val = OPCODES[op]
#                     if op == "JUMP": reg0 = labels[args[0]] - pc
#                     else: 
#                         reg0 = to_int(args[0], "参数1") if len(args) > 0 else 0
#                         reg1 = to_int(args[1], "参数2") if len(args) > 1 else 0
#                         reg2 = to_int(args[2], "参数3") if len(args) > 2 else 0
#                 else: raise ValueError(f"未知指令或对象: {op}")
#                 machine_code.append(struct.pack('<iiii', opcode_val, reg0, reg1, reg2)); pc += 16
#             apps_bin_data_list.append(b"".join(machine_code) + b"".join(string_data))

#         magic_word, app_count = b'MQVM', len(app_names)
#         header_size = 8 + app_count * 8
#         toc_bytes, payload_bytes, current_offset = b"", b"", header_size
#         for app_bin_data in apps_bin_data_list:
#             app_size = len(app_bin_data)
#             toc_bytes += struct.pack('<II', current_offset, app_size)
#             payload_bytes += app_bin_data
#             current_offset += app_size
            
#         final_bin = magic_word + struct.pack('<I', app_count) + toc_bytes + payload_bytes
#         with open("apps_bundle.bin", "wb") as f: f.write(final_bin)
        
#         log_gui(f"[√] 编译成功！生成 {len(app_names)} 个 App，共 {len(final_bin)} 字节。")
#     except Exception as e:
#         log_gui(f"[X] 编译错误: {e}")
#         messagebox.showerror("编译错误", str(e))

# # ===============================================================
# # 5. 文件操作逻辑
# # ===============================================================
# def update_title():
#     filename = os.path.basename(current_filepath) if current_filepath else "未命名.xapp"
#     lbl_editor_title.config(text=f"💻 编辑器 - [{filename}]")

# def open_file():
#     global current_filepath
#     filepath = filedialog.askopenfilename(
#         title="打开 App 脚本",
#         filetypes=[("X-Studio App", "*.xapp"), ("文本文件", "*.txt"), ("所有文件", "*.*")]
#     )
#     if filepath:
#         try:
#             with open(filepath, 'r', encoding='utf-8') as f:
#                 content = f.read()
#             text_code.delete("1.0", tk.END)
#             text_code.insert("1.0", content)
#             current_filepath = filepath
#             update_title()
#         except Exception as e:
#             messagebox.showerror("错误", f"读取文件失败: {e}")

# def save_file():
#     global current_filepath
#     if current_filepath:
#         try:
#             with open(current_filepath, 'w', encoding='utf-8') as f:
#                 f.write(text_code.get("1.0", tk.END))
#             log_gui(f"[√] 文件已保存: {os.path.basename(current_filepath)}")
#         except Exception as e:
#             messagebox.showerror("错误", f"保存文件失败: {e}")
#     else:
#         save_file_as()

# def save_file_as():
#     global current_filepath
#     filepath = filedialog.asksaveasfilename(
#         title="另存为 App 脚本",
#         defaultextension=".xapp",
#         filetypes=[("X-Studio App", "*.xapp"), ("文本文件", "*.txt")]
#     )
#     if filepath:
#         try:
#             with open(filepath, 'w', encoding='utf-8') as f:
#                 f.write(text_code.get("1.0", tk.END))
#             current_filepath = filepath
#             update_title()
#             log_gui(f"[√] 文件已另存为: {os.path.basename(current_filepath)}")
#         except Exception as e:
#             messagebox.showerror("错误", f"保存文件失败: {e}")

# def load_example(name):
#     global current_filepath
#     text_code.delete("1.0", tk.END)
#     text_code.insert("1.0", EXAMPLES[name])
#     current_filepath = None
#     update_title()
#     log_gui(f"[*] 已加载示例模板: {name}")

# # ===============================================================
# # 6. GUI 主界面设计 (左右分栏布局)
# # ===============================================================
# root = tk.Tk()
# root.title("X-Studio - minirtos 专用开发环境")
# root.geometry("1100x700")

# # 顶部状态栏
# status_frame = tk.Frame(root, bg="#2f3542", height=30)
# status_frame.pack(fill=tk.X)
# lbl_isa = tk.Label(status_frame, text="ISA: --", bg="#2f3542", fg="white", font=("Arial", 10, "bold")); lbl_isa.pack(side=tk.LEFT, padx=10, pady=5)
# lbl_peri = tk.Label(status_frame, text="Peri: --", bg="#2f3542", fg="white", font=("Arial", 10, "bold")); lbl_peri.pack(side=tk.LEFT, padx=10, pady=5)

# # 主体分栏 (PanedWindow)
# pane = ttk.PanedWindow(root, orient=tk.HORIZONTAL)
# pane.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

# # --- 左侧：代码编辑区 ---
# left_frame = tk.Frame(pane)
# pane.add(left_frame, weight=1)

# # 编辑器工具栏
# editor_toolbar = tk.Frame(left_frame)
# editor_toolbar.pack(fill=tk.X, pady=2)

# lbl_editor_title = tk.Label(editor_toolbar, text="💻 编辑器 - [未命名.xapp]", font=("微软雅黑", 10, "bold"))
# lbl_editor_title.pack(side=tk.LEFT, padx=5)

# # 示例代码下拉菜单
# btn_examples = ttk.Menubutton(editor_toolbar, text="📝 载入示例")
# examples_menu = tk.Menu(btn_examples, tearoff=0)
# for ex_name in EXAMPLES.keys():
#     # 使用 default 参数绑定循环变量
#     examples_menu.add_command(label=ex_name, command=lambda n=ex_name: load_example(n))
# btn_examples["menu"] = examples_menu
# btn_examples.pack(side=tk.RIGHT, padx=5)

# btn_save_as = tk.Button(editor_toolbar, text="另存为", command=save_file_as, padx=5)
# btn_save_as.pack(side=tk.RIGHT, padx=2)
# btn_save = tk.Button(editor_toolbar, text="💾 保存", command=save_file, padx=5)
# btn_save.pack(side=tk.RIGHT, padx=2)
# btn_open = tk.Button(editor_toolbar, text="📁 打开", command=open_file, padx=5)
# btn_open.pack(side=tk.RIGHT, padx=2)


# text_code = tk.Text(left_frame, font=("Consolas", 12), bg="#f1f2f6", undo=True)
# text_code.pack(fill=tk.BOTH, expand=True, pady=2)
# # 默认填入闪灯代码
# text_code.insert("1.0", EXAMPLES["1. 基础闪灯 (Blink)"])

# btn_compile = tk.Button(left_frame, text="⚙️ 编译代码 (生成 .bin)", bg="#3742fa", fg="white", font=("微软雅黑", 12, "bold"), command=do_compile)
# btn_compile.pack(fill=tk.X, pady=5)

# # --- 右侧：控制台与烧录区 ---
# right_frame = tk.Frame(pane)
# pane.add(right_frame, weight=1)

# tk.Label(right_frame, text="📡 终端与日志输出 (RTT / IAP)", font=("微软雅黑", 10, "bold")).pack(anchor=tk.W)
# text_log = tk.Text(right_frame, font=("Consolas", 10), bg="#1e272e", fg="#00d8d6", state=tk.DISABLED)
# text_log.pack(fill=tk.BOTH, expand=True, pady=2)

# # 右下角操作面板
# ctrl_frame = tk.Frame(right_frame)
# ctrl_frame.pack(fill=tk.X, pady=5)

# tk.Label(ctrl_frame, text="串口号:").pack(side=tk.LEFT)
# entry_port = tk.Entry(ctrl_frame, width=10, font=("Consolas", 12))
# entry_port.insert(0, "COM17") # 默认填你的端口
# entry_port.pack(side=tk.LEFT, padx=5)

# btn_upload = tk.Button(ctrl_frame, text="🚀 一键烧录 (IAP)", bg="#ff4757", fg="white", font=("微软雅黑", 11, "bold"), command=on_btn_upload)
# btn_upload.pack(side=tk.LEFT, padx=5, fill=tk.X, expand=True)

# btn_rtt = tk.Button(ctrl_frame, text="🔌 监听 RTT 日志", bg="#2ed573", fg="white", font=("微软雅黑", 11, "bold"), command=on_btn_rtt)
# btn_rtt.pack(side=tk.LEFT, padx=5, fill=tk.X, expand=True)

# # 启动初始化
# auto_init_configs()
# root.mainloop()
import tkinter as tk
from tkinter import messagebox, ttk, filedialog
import json
import struct
import shlex
import os
import serial
import time
import socket
import threading
import sys

# ===============================================================
# 宏定义与状态管理
# ===============================================================
current_isa = None
current_peri = None
current_filepath = None  # 记录当前打开的文件路径

# RTT 配置
HOST = '127.0.0.1'
CMD_PORT = 4444
RTT_PORT = 8765
RTT_SETUP_CMD = 'rtt setup 0x20000000 0x5000 "SEGGER RTT"\n'

# IAP 配置
IAP_HEADER      = 0xA5
IAP_TAIL        = 0x5A
CMD_START       = 0x01
CMD_DATA        = 0x02
CMD_FINISH      = 0x03
IAP_ACK         = 0x06
IAP_NACK        = 0x15
IAP_DATA_MAX    = 256
APP_START_ADDR  = 0x08000000 + 16 * 1024  # 0x08004000

# ===============================================================
# 示例代码库
# ===============================================================
EXAMPLES = {
    "1. 基础闪灯 (Blink)": """APP0:
# 初始化系统LED
OPEN sys_led AS led1
LABEL main_loop
led1 TOGGLE
DELAY 500
JUMP main_loop
""",
    "2. 串口打印 (UART)": """APP0:
# 初始化串口
OPEN sys_uart AS uart1
LABEL loop
uart1 WRITE "Hello from X-Studio!\\r\\n"
DELAY 1000
JUMP loop
""",
    "3. 多任务并发 (LED + UART)": """APP0:
# 任务0：高频闪灯
OPEN sys_led AS led1
LABEL l1
led1 TOGGLE
DELAY 200
JUMP l1

APP1:
# 任务1：定时发送心跳
OPEN sys_uart AS u1
LABEL l2
u1 WRITE "Ping...\\r\\n"
DELAY 1000
JUMP l2
""",
    "4. 高阶循环与运算 (While + Math)": """APP0:
OPEN sys_led AS led1
OPEN uart AS u1

# 初始化寄存器 (R15存步长，R0存计数器)
R15 = 1
R0 = 0

WHILE R0 < 5
    # 打印循环次数并闪灯
    u1 WRITE "Blink Loop...\\r\\n"
    led1 TOGGLE
    DELAY 500
    
    # 计数器自增 R0 = R0 + 1
    R0 = R0 + R15
ENDWHILE

# 循环结束，关闭LED
led1 OFF
u1 WRITE "Loop Finished!\\r\\n"
""",
    "5. 进阶：子程序与位运算": """APP0:
OPEN sys_led AS led1
OPEN sys_uart AS u1

# 测试位运算: R2 = 1 << 3 (即等于 8)
R0 = 1
R1 = 3
R2 = R0 << R1 

LABEL loop
u1 WRITE "Calling subroutine...\\r\\n"

# 调用子程序，执行双闪灯效果
CALL blink_twice

DELAY 1000
JUMP loop

# ================================
# 以下为子程序区域
# ================================
LABEL blink_twice
led1 ON
DELAY 100
led1 OFF
DELAY 100
led1 ON
DELAY 100
led1 OFF
RET
"""
}

# ===============================================================
# GUI 日志重定向 (线程安全)
# ===============================================================
def log_gui(msg, end="\n"):
    """安全地从其他线程将文本输出到 GUI 日志窗口"""
    def append():
        text_log.config(state=tk.NORMAL)
        text_log.insert(tk.END, msg + end)
        text_log.see(tk.END)
        text_log.config(state=tk.DISABLED)
    root.after(0, append)

# ===============================================================
# 1. CRC 与 IAP 烧录底层逻辑
# ===============================================================
def stm32_crc32_word(crc, word):
    crc ^= word
    for _ in range(32):
        if crc & 0x80000000:
            crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF
        else:
            crc = (crc << 1) & 0xFFFFFFFF
    return crc

def calc_stm32_crc(data: bytes) -> int:
    crc = 0xFFFFFFFF
    length = len(data)
    i = 0
    while i + 4 <= length:
        word = data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24)
        crc = stm32_crc32_word(crc, word)
        i += 4
    if i < length:
        last = 0
        for j in range(length - i):
            last |= (data[i+j] << (8 * j))
        crc = stm32_crc32_word(crc, last)
    return crc

class IAP_Host:
    def __init__(self, port, baudrate=115200):
        self.ser = None
        try:
            self.ser = serial.Serial(port, baudrate, timeout=2.0)
            log_gui(f"[+] 串口 {port} 打开成功")
        except Exception as e:
            log_gui(f"[-] 串口打开失败: {e}")

    def wait_ack(self):
        if not self.ser: return False
        res = self.ser.read(1)
        if not res:
            log_gui("[-] 接收超时！")
            return False
        if res[0] == IAP_ACK: return True
        elif res[0] == IAP_NACK:
            log_gui(f"[-] 收到 NACK (0x15)！")
            return False
        else:
            log_gui(f"[-] 收到未知响应: 0x{res[0]:02X}")
            return False

    def send_frame(self, cmd, size, addr, crc, data_bytes):
        head = struct.pack('<BBHII', IAP_HEADER, cmd, size, addr, crc)
        tail = struct.pack('<B', IAP_TAIL)
        frame = head + data_bytes + tail
        self.ser.write(frame)
        self.ser.flush()
        return self.wait_ack()

    def upgrade(self, firmware: bytes):
        if not self.ser: return
        total_size = len(firmware)
        total_crc = calc_stm32_crc(firmware)
        log_gui(f"[*] 准备烧录... 总大小: {total_size} 字节, CRC: 0x{total_crc:08X}")

        log_gui("[*] 发送 START 帧 (正在擦除 Flash)...")
        start_data = struct.pack('<I', total_size) 
        if not self.send_frame(CMD_START, 4, 0, total_crc, start_data):
            log_gui("[-] START 帧失败！")
            return
        time.sleep(0.2)
        offset = 0
        while offset < total_size:
            chunk = firmware[offset : offset + IAP_DATA_MAX]
            chunk_crc = calc_stm32_crc(chunk)
            target_addr = APP_START_ADDR + offset
            if not self.send_frame(CMD_DATA, len(chunk), target_addr, chunk_crc, chunk):
                log_gui(f"[-] DATA 帧失败 (偏移量 {offset})！")
                return
            offset += len(chunk)

        log_gui("[*] 发送 FINISH 帧 (整包校验)...")
        if not self.send_frame(CMD_FINISH, 0, 0, 0, b''):
            log_gui("[-] FINISH 帧失败！校验未通过。")
            return

        log_gui("[+] 升级圆满成功！🚀")

# ===============================================================
# 2. 线程任务：RTT 与 IAP 烧录
# ===============================================================
def thread_rtt_listen():
    log_gui(f"[*] 连接 OpenOCD 控制台 ({CMD_PORT})...")
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((HOST, CMD_PORT))
            time.sleep(0.2)
            s.recv(1024) 
            for cmd in [RTT_SETUP_CMD, 'rtt start\n', f'rtt server start {RTT_PORT} 0\n']:
                s.sendall(cmd.encode('utf-8'))
                time.sleep(0.2)
                s.recv(1024)
        log_gui("[+] OpenOCD RTT 配置完毕！")
    except Exception as e:
        log_gui(f"[!] 配置 RTT 失败: {e}\n(确认 OpenOCD 已启动)")
        return

    time.sleep(0.5) 
    log_gui(f"[*] 连接 RTT 数据通道 ({RTT_PORT})...")
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((HOST, RTT_PORT))
            log_gui("[+] 日志通道连接成功！(等待单片机输出...)\n" + "="*40)
            while True:
                data = s.recv(1024)
                if not data:
                    log_gui("\n[-] RTT 连接已断开。")
                    break
                text = data.decode('utf-8', errors='ignore')
                log_gui(text, end='')
    except Exception as e:
        log_gui(f"[!] RTT 数据接收失败: {e}")

def thread_upload_task(port):
    if not os.path.exists("apps_bundle.bin"):
        log_gui("[-] 未找到 apps_bundle.bin，请先点击编译！")
        return
    btn_upload.config(state=tk.DISABLED)
    try:
        with open("apps_bundle.bin", "rb") as f:
            firmware = f.read()
        host = IAP_Host(port=port, baudrate=115200)
        host.upgrade(firmware)
        if host.ser: host.ser.close()
    except Exception as e:
        log_gui(f"[-] 烧录过程发生异常: {e}")
    finally:
        btn_upload.config(state=tk.NORMAL)

def on_btn_upload():
    port = entry_port.get().strip()
    if not port:
        messagebox.showwarning("提示", "请输入串口号！(如 COM17)")
        return
    threading.Thread(target=thread_upload_task, args=(port,), daemon=True).start()

def on_btn_rtt():
    btn_rtt.config(state=tk.DISABLED)
    threading.Thread(target=thread_rtt_listen, daemon=True).start()

# ===============================================================
# 3. 编译器核心逻辑 (AST语法糖无缝集成)
# ===============================================================
def auto_init_configs():
    global current_isa, current_peri
    base_dir = os.path.dirname(os.path.abspath(__file__))
    try:
        with open(os.path.join(base_dir, "isa.json"), 'r', encoding='utf-8') as f:
            current_isa = json.load(f)
        lbl_isa.config(text="ISA: 就绪", fg="#2ed573")
    except: lbl_isa.config(text="ISA: 失败", fg="#ff4757")
        
    try:
        with open(os.path.join(base_dir, "peri.json"), 'r', encoding='utf-8') as f:
            current_peri = json.load(f)
        lbl_peri.config(text="Peri: 就绪", fg="#2ed573")
    except: lbl_peri.config(text="Peri: 失败", fg="#ff4757")

def do_compile():
    if current_isa is None or current_peri is None:
        messagebox.showerror("配置缺失", "编译失败：请确保 isa.json 和 peri.json 存在！")
        return
    try:
        OPCODES, SYSCALLS, CLASSES = current_isa, current_peri.get("SYSCALLS", {}), current_peri.get("CLASSES", {})
        raw_text = text_code.get("1.0", tk.END)
        
        # =========================================================
        # 3.1 语法糖宏展开 (AST 预处理引擎)
        # =========================================================
        expanded_lines = []
        if_counter = 0
        if_stack = []
        while_counter = 0
        while_stack = []

        for line in raw_text.splitlines():
            line = line.strip().replace("->", " ").replace(",", " ")
            line = line.replace("<<", " LSHIFT ").replace(">>", " RSHIFT ")
            if not line or line.startswith('#') or line.startswith(';'): continue
            
            tokens = shlex.split(line)
            if not tokens: continue
            
            # --- 魔法 1: 数学与赋值 (R0 = R1 + R2, R0 = 100) ---
            if len(tokens) >= 3 and tokens[1] == "=":
                dest = tokens[0]
                if len(tokens) == 3:
                    src = tokens[2]
                    # 区分是寄存器赋值 (R0 = R1) 还是立即数加载 (R0 = 100)
                    if src.upper().startswith('R'): 
                        expanded_lines.append(f"MOV {dest} {src}")
                    else: 
                        expanded_lines.append(f"LOAD {dest} {src}")
                elif len(tokens) == 5:
                    src1, operator, src2 = tokens[2], tokens[3], tokens[4]
                    if operator == "+": 
                        expanded_lines.append(f"ADD {dest} {src1} {src2}")
                    elif operator == "-": 
                        expanded_lines.append(f"SUB {dest} {src1} {src2}")
                    elif operator == "&": 
                        expanded_lines.append(f"AND {dest} {src1} {src2}")
                    elif operator == "|": 
                        expanded_lines.append(f"OR {dest} {src1} {src2}")
                    elif operator == "^": 
                        expanded_lines.append(f"XOR {dest} {src1} {src2}")
                    elif operator == "LSHIFT": 
                        expanded_lines.append(f"LSHIFT {dest} {src1} {src2}")
                    elif operator == "RSHIFT": 
                        expanded_lines.append(f"RSHIFT {dest} {src1} {src2}")
                continue

            op = tokens[0].upper()
            
            # --- 魔法 2: IF / ELSE / ENDIF ---
            if op == "IF":
                if_counter += 1
                lbl_else = f"__IF_ELSE_{if_counter}"
                lbl_end = f"__IF_END_{if_counter}"
                if_stack.append({"else": lbl_else, "end": lbl_end, "has_else": False})
                
                expanded_lines.append(f"CMP {tokens[1]} {tokens[3]}")
                if tokens[2] == "==": expanded_lines.append(f"JNE {lbl_else}")
                elif tokens[2] == "!=": expanded_lines.append(f"JEQ {lbl_else}")
                elif tokens[2] == ">": 
                    expanded_lines.append(f"JGT __IF_TRUE_{if_counter}")
                    expanded_lines.append(f"JUMP {lbl_else}")
                    expanded_lines.append(f"LABEL __IF_TRUE_{if_counter}")
                elif tokens[2] == "<":
                    expanded_lines.append(f"JLT __IF_TRUE_{if_counter}")
                    expanded_lines.append(f"JUMP {lbl_else}")
                    expanded_lines.append(f"LABEL __IF_TRUE_{if_counter}")
                    
            elif op == "ELSE":
                top = if_stack[-1]
                top["has_else"] = True
                expanded_lines.append(f"JUMP {top['end']}")
                expanded_lines.append(f"LABEL {top['else']}")
                
            elif op == "ENDIF":
                top = if_stack.pop()
                if top["has_else"]: expanded_lines.append(f"LABEL {top['end']}")
                else: expanded_lines.append(f"LABEL {top['else']}")
            
            # --- 魔法 3: WHILE / ENDWHILE ---
            elif op == "WHILE":
                while_counter += 1
                lbl_start = f"__WHILE_START_{while_counter}"
                lbl_end = f"__WHILE_END_{while_counter}"
                while_stack.append({"start": lbl_start, "end": lbl_end})
                
                expanded_lines.append(f"LABEL {lbl_start}")
                expanded_lines.append(f"CMP {tokens[1]} {tokens[3]}")
                if tokens[2] == "==": expanded_lines.append(f"JNE {lbl_end}")
                elif tokens[2] == "!=": expanded_lines.append(f"JEQ {lbl_end}")
                elif tokens[2] == ">": 
                    expanded_lines.append(f"JGT __WHILE_BODY_{while_counter}")
                    expanded_lines.append(f"JUMP {lbl_end}")
                    expanded_lines.append(f"LABEL __WHILE_BODY_{while_counter}")
                elif tokens[2] == "<": 
                    expanded_lines.append(f"JLT __WHILE_BODY_{while_counter}")
                    expanded_lines.append(f"JUMP {lbl_end}")
                    expanded_lines.append(f"LABEL __WHILE_BODY_{while_counter}")
                    
            elif op == "ENDWHILE":
                top = while_stack.pop()
                expanded_lines.append(f"JUMP {top['start']}")
                expanded_lines.append(f"LABEL {top['end']}")
            
            else:
                # 普通指令原样保留
                expanded_lines.append(line)

        # =========================================================
        # 3.2 生成底层机器码 (兼容寄存器提取)
        # =========================================================
        apps_source = {}
        current_app = "app0"
        apps_source[current_app] = []

        for line in expanded_lines:
            tokens = shlex.split(line)
            if tokens[0].upper().startswith('APP') and tokens[0].endswith(':'):
                current_app = tokens[0][:-1].lower() 
                apps_source[current_app] = []
                continue

            parsed = []
            for t in tokens:
                # 动态识别寄存器 (将 "R0"~"R15" 自动提取为数字 0~15)
                if isinstance(t, str) and t.upper().startswith('R') and t[1:].isdigit():
                    parsed.append(int(t[1:]))
                elif isinstance(t, str) and t.startswith('0x'): 
                    parsed.append(int(t, 16))
                elif isinstance(t, str) and (t.isdigit() or (t.startswith('-') and t[1:].isdigit())): 
                    parsed.append(int(t))
                else: 
                    parsed.append(t)
            
            if parsed:
                apps_source[current_app].append(parsed)
            
        app_names, apps_bin_data_list = [], []
        for app_name, source_code in apps_source.items():
            if not source_code: continue
            app_names.append(app_name)
            labels, pc = {}, 0
            instr_count = sum(1 for i in source_code if i[0] != "LABEL")
            instr_zone_size = instr_count * 16 
            for inst in source_code:
                if inst[0] == "LABEL": labels[inst[1]] = pc
                else: pc += 16
            
            machine_code, string_data, current_str_offset, pc, symbol_table, fd_allocator = [], [], 0, 0, {}, 1
            for inst in source_code:
                if inst[0] == "LABEL": continue
                op, args = inst[0], inst[1:]
                opcode_val, reg0, reg1, reg2 = 0, 0, 0, 0

                if op in symbol_table:
                    obj = symbol_table[op]; dev_class = obj["class"]; action = args[0]
                    if action == "WRITE":
                        opcode_val, reg0, reg1 = OPCODES["SYSCALL"], SYSCALLS.get("WRITE", 4), obj["fd"]
                        reg2 = instr_zone_size + current_str_offset
                        s = args[1].replace("\\r", "\r").replace("\\n", "\n").encode('utf-8') + b'\0'
                        string_data.append(s); current_str_offset += len(s)
                    elif action == "READ":
                        opcode_val, reg0, reg1, reg2 = OPCODES["SYSCALL"], SYSCALLS.get("READ", 2), obj["fd"], args[1]
                    else:
                        opcode_val, reg0, reg1, reg2 = OPCODES["SYSCALL"], SYSCALLS.get("IOCTL", 3), obj["fd"], CLASSES.get(dev_class, {}).get(action, 0)
                elif op == "OPEN":
                    opcode_val, reg0 = OPCODES["SYSCALL"], SYSCALLS.get("OPEN", 1)
                    dev_class = current_peri.get("BINDINGS", {}).get(args[0], "unknown")
                    if len(args) >= 3 and args[1] == "AS":
                        symbol_table[args[2]] = {"fd": fd_allocator, "class": dev_class}
                        reg1, fd_allocator = fd_allocator, fd_allocator + 1
                    reg2 = instr_zone_size + current_str_offset
                    s = args[0].encode('utf-8') + b'\0'
                    string_data.append(s); current_str_offset += len(s)
                elif op in SYSCALLS:
                    opcode_val, reg0 = OPCODES["SYSCALL"], SYSCALLS[op]
                    reg1 = args[0] if len(args) > 0 else 0
                    reg2 = args[1] if len(args) > 1 else 0
                elif op in OPCODES:
                    opcode_val = OPCODES[op]
                    if op in ["JUMP", "JEQ", "JNE", "JGT", "JLT", "CALL"]: 
                        reg0 = labels[args[0]] - pc
                    else: 
                        reg0 = args[0] if len(args) > 0 else 0
                        reg1 = args[1] if len(args) > 1 else 0
                        reg2 = args[2] if len(args) > 2 else 0
                else: raise ValueError(f"未知指令或对象: {op}")
                machine_code.append(struct.pack('<iiii', opcode_val, reg0, reg1, reg2)); pc += 16

            # 将指令和字符串合并
            raw_app_bin = b"".join(machine_code) + b"".join(string_data)
            
            # 魔法机制：强制 4 字节对齐，不足的用 \0 补齐
            pad_len = (4 - (len(raw_app_bin) % 4)) % 4
            aligned_app_bin = raw_app_bin + (b'\0' * pad_len)
            
            apps_bin_data_list.append(aligned_app_bin)

        magic_word, app_count = b'MQVM', len(app_names)
        header_size = 8 + app_count * 8
        toc_bytes, payload_bytes, current_offset = b"", b"", header_size
        for app_bin_data in apps_bin_data_list:
            app_size = len(app_bin_data)
            toc_bytes += struct.pack('<II', current_offset, app_size)
            payload_bytes += app_bin_data
            current_offset += app_size
            
        final_bin = magic_word + struct.pack('<I', app_count) + toc_bytes + payload_bytes
        with open("apps_bundle.bin", "wb") as f: f.write(final_bin)
        
        log_gui(f"[√] 编译成功！生成 {len(app_names)} 个 App，共 {len(final_bin)} 字节。")
    except Exception as e:
        log_gui(f"[X] 编译错误: {e}")
        messagebox.showerror("编译错误", str(e))

# ===============================================================
# 4. 文件操作逻辑
# ===============================================================
def update_title():
    filename = os.path.basename(current_filepath) if current_filepath else "未命名.xapp"
    lbl_editor_title.config(text=f"💻 编辑器 - [{filename}]")

def open_file():
    global current_filepath
    filepath = filedialog.askopenfilename(
        title="打开 App 脚本",
        filetypes=[("X-Studio App", "*.xapp"), ("文本文件", "*.txt"), ("所有文件", "*.*")]
    )
    if filepath:
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
            text_code.delete("1.0", tk.END)
            text_code.insert("1.0", content)
            current_filepath = filepath
            update_title()
        except Exception as e:
            messagebox.showerror("错误", f"读取文件失败: {e}")

def save_file():
    global current_filepath
    if current_filepath:
        try:
            with open(current_filepath, 'w', encoding='utf-8') as f:
                f.write(text_code.get("1.0", tk.END))
            log_gui(f"[√] 文件已保存: {os.path.basename(current_filepath)}")
        except Exception as e:
            messagebox.showerror("错误", f"保存文件失败: {e}")
    else:
        save_file_as()

def save_file_as():
    global current_filepath
    filepath = filedialog.asksaveasfilename(
        title="另存为 App 脚本",
        defaultextension=".xapp",
        filetypes=[("X-Studio App", "*.xapp"), ("文本文件", "*.txt")]
    )
    if filepath:
        try:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(text_code.get("1.0", tk.END))
            current_filepath = filepath
            update_title()
            log_gui(f"[√] 文件已另存为: {os.path.basename(current_filepath)}")
        except Exception as e:
            messagebox.showerror("错误", f"保存文件失败: {e}")

def load_example(name):
    global current_filepath
    text_code.delete("1.0", tk.END)
    text_code.insert("1.0", EXAMPLES[name])
    current_filepath = None
    update_title()
    log_gui(f"[*] 已加载示例模板: {name}")

# ===============================================================
# 5. GUI 主界面设计 (左右分栏布局)
# ===============================================================
root = tk.Tk()
root.title("X-Studio - minirtos 专用开发环境")
root.geometry("1100x700")

# 顶部状态栏
status_frame = tk.Frame(root, bg="#2f3542", height=30)
status_frame.pack(fill=tk.X)
lbl_isa = tk.Label(status_frame, text="ISA: --", bg="#2f3542", fg="white", font=("Arial", 10, "bold")); lbl_isa.pack(side=tk.LEFT, padx=10, pady=5)
lbl_peri = tk.Label(status_frame, text="Peri: --", bg="#2f3542", fg="white", font=("Arial", 10, "bold")); lbl_peri.pack(side=tk.LEFT, padx=10, pady=5)

# 主体分栏 (PanedWindow)
pane = ttk.PanedWindow(root, orient=tk.HORIZONTAL)
pane.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

# --- 左侧：代码编辑区 ---
left_frame = tk.Frame(pane)
pane.add(left_frame, weight=1)

# 编辑器工具栏
editor_toolbar = tk.Frame(left_frame)
editor_toolbar.pack(fill=tk.X, pady=2)

lbl_editor_title = tk.Label(editor_toolbar, text="💻 编辑器 - [未命名.xapp]", font=("微软雅黑", 10, "bold"))
lbl_editor_title.pack(side=tk.LEFT, padx=5)

# 示例代码下拉菜单
btn_examples = ttk.Menubutton(editor_toolbar, text="📝 载入示例")
examples_menu = tk.Menu(btn_examples, tearoff=0)
for ex_name in EXAMPLES.keys():
    # 使用 default 参数绑定循环变量
    examples_menu.add_command(label=ex_name, command=lambda n=ex_name: load_example(n))
btn_examples["menu"] = examples_menu
btn_examples.pack(side=tk.RIGHT, padx=5)

btn_save_as = tk.Button(editor_toolbar, text="另存为", command=save_file_as, padx=5)
btn_save_as.pack(side=tk.RIGHT, padx=2)
btn_save = tk.Button(editor_toolbar, text="💾 保存", command=save_file, padx=5)
btn_save.pack(side=tk.RIGHT, padx=2)
btn_open = tk.Button(editor_toolbar, text="📁 打开", command=open_file, padx=5)
btn_open.pack(side=tk.RIGHT, padx=2)


text_code = tk.Text(left_frame, font=("Consolas", 12), bg="#f1f2f6", undo=True)
text_code.pack(fill=tk.BOTH, expand=True, pady=2)
# 默认填入闪灯代码
text_code.insert("1.0", EXAMPLES["1. 基础闪灯 (Blink)"])

btn_compile = tk.Button(left_frame, text="⚙️ 编译代码 (生成 .bin)", bg="#3742fa", fg="white", font=("微软雅黑", 12, "bold"), command=do_compile)
btn_compile.pack(fill=tk.X, pady=5)

# --- 右侧：控制台与烧录区 ---
right_frame = tk.Frame(pane)
pane.add(right_frame, weight=1)

tk.Label(right_frame, text="📡 终端与日志输出 (RTT / IAP)", font=("微软雅黑", 10, "bold")).pack(anchor=tk.W)
text_log = tk.Text(right_frame, font=("Consolas", 10), bg="#1e272e", fg="#00d8d6", state=tk.DISABLED)
text_log.pack(fill=tk.BOTH, expand=True, pady=2)

# 右下角操作面板
ctrl_frame = tk.Frame(right_frame)
ctrl_frame.pack(fill=tk.X, pady=5)

tk.Label(ctrl_frame, text="串口号:").pack(side=tk.LEFT)
entry_port = tk.Entry(ctrl_frame, width=10, font=("Consolas", 12))
entry_port.insert(0, "COM17") # 默认填你的端口
entry_port.pack(side=tk.LEFT, padx=5)

btn_upload = tk.Button(ctrl_frame, text="🚀 一键烧录 (IAP)", bg="#ff4757", fg="white", font=("微软雅黑", 11, "bold"), command=on_btn_upload)
btn_upload.pack(side=tk.LEFT, padx=5, fill=tk.X, expand=True)

btn_rtt = tk.Button(ctrl_frame, text="🔌 监听 RTT 日志", bg="#2ed573", fg="white", font=("微软雅黑", 11, "bold"), command=on_btn_rtt)
btn_rtt.pack(side=tk.LEFT, padx=5, fill=tk.X, expand=True)

# 启动初始化
auto_init_configs()
root.mainloop()