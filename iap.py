import serial
import struct
import time
import sys
import os

# ==========================================
# 协议宏定义 (严格对齐下位机 C 代码)
# ==========================================
IAP_HEADER      = 0xA5
IAP_TAIL        = 0x5A
CMD_START       = 0x01
CMD_DATA        = 0x02
CMD_FINISH      = 0x03
IAP_ACK         = 0x06
IAP_NACK        = 0x15
IAP_DATA_MAX    = 256
APP_START_ADDR  = 0x08000000 + 16 * 1024  # 0x08004000

# ==========================================
# STM32 硬件 CRC32 模拟
# ==========================================
def stm32_crc32_word(crc, word):
    """计算单个 32-bit 数据的 STM32 CRC"""
    crc ^= word
    for _ in range(32):
        if crc & 0x80000000:
            crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF
        else:
            crc = (crc << 1) & 0xFFFFFFFF
    return crc

def calc_stm32_crc(data: bytes) -> int:
    """完全等效于下位机的 crc32_hw 函数"""
    crc = 0xFFFFFFFF
    length = len(data)
    i = 0
    # 处理完整的 4 字节块
    while i + 4 <= length:
        word = data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24)
        crc = stm32_crc32_word(crc, word)
        i += 4
    # 处理尾部不足 4 字节的部分
    if i < length:
        last = 0
        for j in range(length - i):
            last |= (data[i+j] << (8 * j))
        crc = stm32_crc32_word(crc, last)
    return crc

# ==========================================
# 串口通信类
# ==========================================
class IAP_Host:
    def __init__(self, port, baudrate=115200):
        try:
            self.ser = serial.Serial(port, baudrate, timeout=2.0)
            print(f"[+] 串口 {port} 打开成功")
        except Exception as e:
            print(f"[-] 串口打开失败: {e}")
            sys.exit(1)

    def wait_ack(self):
        """等待下位机返回 ACK"""
        res = self.ser.read(1)
        if not res:
            print("[-] 接收超时！")
            return False
        if res[0] == IAP_ACK:
            return True
        elif res[0] == IAP_NACK:
            print(f"[-] 收到 NACK (0x15)！")
            return False
        else:
            print(f"[-] 收到未知响应: 0x{res[0]:02X}")
            return False

    def send_frame(self, cmd, size, addr, crc, data_bytes):
        """打包并发送一帧数据"""
        # <BBHII 表示: 小端模式, uint8, uint8, uint16, uint32, uint32
        head = struct.pack('<BBHII', IAP_HEADER, cmd, size, addr, crc)
        tail = struct.pack('<B', IAP_TAIL)
        frame = head + data_bytes + tail
        
        self.ser.write(frame)
        self.ser.flush()
        return self.wait_ack()

    def upgrade(self, firmware: bytes):
        total_size = len(firmware)
        total_crc = calc_stm32_crc(firmware)
        print(f"[*] 准备烧录... 总大小: {total_size} 字节, 总CRC: 0x{total_crc:08X}")

        # 1. 发送 START 帧
        print("[*] 正在发送 START 帧 (擦除Flash可能需要几秒)...")
        # 将总大小打包为 4 字节小端发在 data 区
        start_data = struct.pack('<I', total_size) 
        if not self.send_frame(CMD_START, 4, 0, total_crc, start_data):
            print("[-] START 帧失败！")
            return

        # 2. 发送 DATA 帧 (分包)
        offset = 0
        while offset < total_size:
            chunk = firmware[offset : offset + IAP_DATA_MAX]
            chunk_crc = calc_stm32_crc(chunk)
            target_addr = APP_START_ADDR + offset
            
            print(f"[*] 发送数据块... 地址: 0x{target_addr:08X}, 长度: {len(chunk)}, CRC: 0x{chunk_crc:08X}")
            
            if not self.send_frame(CMD_DATA, len(chunk), target_addr, chunk_crc, chunk):
                print(f"[-] DATA 帧失败 (偏移量 {offset})！")
                return
            offset += len(chunk)

        # 3. 发送 FINISH 帧
        print("[*] 正在发送 FINISH 帧 (整包校验)...")
        if not self.send_frame(CMD_FINISH, 0, 0, 0, b''):
            print("[-] FINISH 帧失败！校验未通过。")
            return

        print("[+] 升级圆满成功！🚀")

# ==========================================
# 测试入口
# ==========================================
if __name__ == '__main__':
    # 替换为你实际的串口号，例如 'COM3' (Windows) 或 '/dev/ttyUSB0' (Linux)
    PORT = 'COM17' 
    BAUD = 115200

    # 生成一段虚拟固件数据 (1030 字节，包含奇数结尾用于测试填充逻辑)
    # dummy_firmware = bytes([i % 256 for i in range(1030)])
    
    # 如果你想测试真实的 .bin 文件，取消下面两行的注释：
    with open("apps_bundle.bin", "rb") as f:
        dummy_firmware = f.read()

    host = IAP_Host(port=PORT, baudrate=BAUD)
    host.upgrade(dummy_firmware)
    host.ser.close()