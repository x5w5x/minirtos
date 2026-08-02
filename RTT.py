import socket
import time
import sys
import threading

# ================= 配置区 =================
HOST = '127.0.0.1'
CMD_PORT = 4444   # OpenOCD 的 Telnet 控制端口
RTT_PORT = 8765   # RTT 的交互数据端口

# 注意：针对 STM32F103C6T6 (10KB RAM)，RAM 范围是 0x20000000 ~ 0x20002800 (10KB = 0x2800)
# 如果发现搜索不到 RTT，可以将搜索范围调整为 0x2800
RTT_SETUP_CMD = 'rtt setup 0x20000000 0x2800 "SEGGER RTT"\n'
# ==========================================

def send_openocd_commands():
    """第一步：连接 4444 端口，配置并启动 OpenOCD RTT 服务"""
    print(f"[*] 正在连接 OpenOCD 控制台 ({HOST}:{CMD_PORT})...")
    
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((HOST, CMD_PORT))
            time.sleep(0.2)
            s.recv(1024) # 清理欢迎标语

            commands = [
                RTT_SETUP_CMD,
                'rtt start\n',
                f'rtt server start {RTT_PORT} 0\n' # 开启通道 0 的 TCP 服务
            ]
            
            for cmd in commands:
                print(f"  -> 自动发送命令: {cmd.strip()}")
                s.sendall(cmd.encode('utf-8'))
                time.sleep(0.2)
                s.recv(1024)
                
        print("[+] OpenOCD RTT 配置指令发送完毕！\n")
        return True
        
    except ConnectionRefusedError:
        print("\n[!] ❌ 无法连接到 4444 控制端口！请确认 OpenOCD 是否在后台运行。")
        return False
    except Exception as e:
        print(f"\n[!] 发送控制指令时出现未知错误: {e}")
        return False


def rtt_recv_thread(sock, stop_event):
    """后台子线程：负责持续接收并打印单片机发来的 RTT 日志"""
    while not stop_event.is_set():
        try:
            data = sock.recv(1024)
            if not data:
                print("\n[-] 连接已断开 (OpenOCD 已关闭或单片机复位)。")
                stop_event.set()
                break
            
            # 解码并即时刷新打印到屏幕
            text = data.decode('utf-8', errors='ignore')
            print(text, end='', flush=True)

        except (socket.timeout, TimeoutError):
            # 配合超时检测 stop_event，防止退出时线程死锁
            continue
        except Exception:
            if not stop_event.is_set():
                print("\n[-] RTT 接收线程异常退出。")
                stop_event.set()
            break


def interactive_rtt_shell():
    """第二步：连接 8765 端口，实现双向 Shell 终端交互"""
    print(f"[*] 正在尝试连接 RTT 数据端口 ({HOST}:{RTT_PORT})...")
    time.sleep(0.5) 
    
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((HOST, RTT_PORT))
        s.settimeout(1.0) # 设置 1 秒接收超时，方便响应退出信号

        print("[+] RTT 双向终端连接成功！")
        print("[+] 现在可以直接输入命令 (如 'ps', 'help', 'reboot') 并按回车发送给单片机。")
        print("[+] 按 Ctrl+C 退出交互终端。\n")
        print("=" * 60)

        stop_event = threading.Event()

        # 创建并启动后台接收线程
        t = threading.Thread(target=rtt_recv_thread, args=(s, stop_event), daemon=True)
        t.start()

        # 主线程：捕获键盘输入，发送到单片机 Down-Buffer
        while not stop_event.is_set():
            try:
                # 从终端读取用户输入的指令（包含末尾的 \n）
                user_input = sys.stdin.readline()
                if not user_input:
                    break

                # 通过 TCP 发送给 OpenOCD，OpenOCD 会写入单片机 RTT Down-Buffer
                s.sendall(user_input.encode('utf-8'))

            except KeyboardInterrupt:
                print("\n\n[*] 用户按下 Ctrl+C，正在退出 RTT Shell...")
                stop_event.set()
                break
            except Exception as e:
                print(f"\n[!] 发送指令失败: {e}")
                stop_event.set()
                break

        # 清理连接
        s.close()
        t.join(timeout=1.0)
        print("[*] RTT 终端已关闭。")

    except ConnectionRefusedError:
        print("\n[!] ❌ 连接 8765 端口被拒绝， OpenOCD 没有成功开启 RTT 服务。")
    except Exception as e:
        print(f"\n[!] 终端交互发生错误: {e}")


if __name__ == "__main__":
    if send_openocd_commands():
        interactive_rtt_shell()