import socket
import time
import sys

# ================= 配置区 =================
HOST = '127.0.0.1'
CMD_PORT = 4444   # OpenOCD 的 Telnet 控制端口
RTT_PORT = 8765   # RTT 的日志数据转发端口

# 注意：如果你的单片机是 64KB RAM (如 RCT6)，请把 0x5000 改为 0x10000
RTT_SETUP_CMD = 'rtt setup 0x20000000 0x5000 "SEGGER RTT"\n'
# ==========================================

def send_openocd_commands():
    """第一步：连接 4444 端口，告诉 OpenOCD 开启 RTT"""
    print(f"[*] 正在连接 OpenOCD 控制台 ({HOST}:{CMD_PORT})...")
    
    try:
        # 创建网络连接
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((HOST, CMD_PORT))
            
            # 等待并清理 OpenOCD 刚连上时的欢迎信息
            time.sleep(0.2)
            s.recv(1024) 

            # 我们要发送的 3 条命令 (末尾必须带 \n 模拟回车)
            commands = [
                RTT_SETUP_CMD,
                'rtt start\n',
                f'rtt server start {RTT_PORT} 0\n'
            ]
            
            # 依次发送命令
            for cmd in commands:
                print(f"  -> 自动发送命令: {cmd.strip()}")
                s.sendall(cmd.encode('utf-8'))
                time.sleep(0.2) # 给 OpenOCD 留一点执行时间
                s.recv(1024)    # 读取回显，防止阻塞
                
        print("[+] OpenOCD RTT 配置指令发送完毕！\n")
        return True
        
    except ConnectionRefusedError:
        print("\n[!] ❌ 无法连接到 4444 控制端口！")
        print("请确认：你的 OpenOCD 程序是否已经启动并在后台运行？")
        return False
    except Exception as e:
        print(f"\n[!] 发送控制指令时出现未知错误: {e}")
        return False


def listen_rtt_data():
    """第二步：连接 8765 端口，持续接收日志"""
    print(f"[*] 正在尝试连接 RTT 数据端口 ({HOST}:{RTT_PORT})...")
    
    # 稍微等半秒钟，确保 OpenOCD 已经把 8765 端口开好了
    time.sleep(0.5) 
    
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((HOST, RTT_PORT))
            print("[+] 日志通道连接成功！(按下单片机复位键查看输出，按 Ctrl+C 退出)\n")
            print("=" * 60)
            
            # 死循环接收数据
            while True:
                data = s.recv(1024)
                if not data:
                    print("\n[-] 连接已断开 (OpenOCD 可能已关闭)。")
                    break
                
                # 解码并打印，过滤掉无法识别的乱码
                text = data.decode('utf-8', errors='ignore')
                print(text, end='', flush=True)
                
    except ConnectionRefusedError:
        print("\n[!] ❌ 连接 8765 端口被拒绝，可能 OpenOCD 没有成功开启 RTT 服务。")
    except KeyboardInterrupt:
        print("\n\n[*] 退出日志查看。")

if __name__ == "__main__":
    # 1. 自动发送配置命令
    if send_openocd_commands():
        # 2. 如果配置成功，立刻开始监听数据
        listen_rtt_data()