# MiniRTOS

一个从零手写的、运行在 **STM32F103 (Cortex-M3)** 上的轻量级实时操作系统教学实践项目。

除了一个功能完整的抢占式 RTOS 内核外，本工程还集成了 **VFS 统一设备框架**、**自研字节码虚拟机（VM）**、**串口 IAP 在线升级** 以及基于 **SEGGER RTT** 的交互式命令行 Shell，是一套麻雀虽小、五脏俱全的嵌入式软件学习样板。

> 工程名取自 "mini RTOS"，寓意用尽量少的代码讲清楚 RTOS 的核心原理。

---

## ✨ 特性

- **抢占式多任务内核**
  - 32 级位图优先级就绪队列，`O(1)` 时间找到最高优先级任务
  - 基于 **SysTick**（1ms）时基 + **PendSV** 汇编上下文切换，支持时间片轮转
  - 任务、信号量、互斥锁（含**优先级继承**）、消息队列、软件定时器
  - 空闲任务 + **CPU 使用率动态校准**（开机自动测量）
  - 任务状态/栈水位实时监控（`ps` 命令）
- **类 Linux VFS 设备框架**
  - 统一 `open/read/write/ioctl/close` 抽象，设备即文件
  - 已适配：LED、UART、ADC、PWM、按键、OLED、消息队列
  - 设备访问自带互斥锁保护
- **自研字节码虚拟机（VM）**
  - 16 寄存器、8 级调用栈，20+ 条指令（算术/逻辑/跳转/条件/调用/系统调用）
  - 支持多个 App 并发运行，字节码可**内嵌编译**或**从 Flash 动态加载**（MQVM 打包格式）
  - App 通过 `syscall` 安全访问 VFS 外设（沙箱隔离）
- **串口 IAP 在线升级**
  - `START → DATA → FINISH` 三段式帧协议，硬件 CRC32 逐帧校验 + 整包回读校验
  - Flash 槽位化管理（8 槽 × 1KB），自动寻找最高版本
- **RTT 交互式 Shell**
  - 无需占用串口资源，通过 SEGGER RTT + OpenOCD 双向交互
  - 内置 `ps / dev / app / lsdev / help / clear / reboot` 命令
- **双构建链**：EIDE（Embedded IDE）与 Keil MDK 均可用

---

## 📁 目录结构

```
minirtos/
├── OS_Core/               # RTOS 内核核心（平台无关部分）
│   ├── os_core.c/.h       #   调度器、任务、信号量、互斥锁、消息队列、软件定时器、CPU 统计
│   ├── os_list.h          #   通用双向环形链表
│   └── shell.c/.h         #   RTT 命令行 Shell
├── OS_Port/               # 平台移植层（Cortex-M3）
│   ├── os_port.c/.h       #   SysTick 初始化、位图取最高优先级、任务切换触发
│   └── os_port.s          #   PendSV_Handler 汇编上下文切换（保存/恢复 R4~R11）
├── minirtos/              # 最早的极简 RTOS 原型（已废弃，保留作学习笔记）
├── vfs/                   # 类 Linux 虚拟文件系统设备框架
├── vm/                    # 字节码虚拟机
│   ├── vm_isa.c/.h        #   指令集解释器（VM 运行时）
│   └── vm_task.c/.h       #   VM 调度任务、App 内嵌字节码、MQVM 动态加载
├── driver/                # 外设驱动（全部注册到 VFS）
│   ├── led_driver.*       #   LED
│   ├── uart_driver.*      #   USART
│   ├── adc_driver.*       #   ADC
│   ├── pwm_driver.*       #   TIM PWM
│   ├── key_driver.*       #   按键
│   ├── oled_driver.*      #   OLED（含软件 I2C sw_iic.*）
│   ├── mq_driver.*        #   消息队列设备
│   └── flash_iap.* / iap_protocol.*   #  Flash 擦写 / IAP 升级协议
├── User/                  # 用户应用
│   ├── main.c             #   系统入口：设备注册、任务创建、启动调度
│   └── SEGGER_RTT.*       #   RTT 调试输出库
├── System/                # 通用工具（Delay / Serial，当前未启用）
├── Start/                 # 启动文件、core_cm3、寄存器定义
├── Library/               # STM32F10x 标准外设库（StdPeriph）
├── *.py                   # 上位机工具脚本（详见下文）
├── isa.json               # VM 指令集定义
├── peri.json              # VM 外设绑定 / 系统调用 / 设备类定义
├── apps_bundle.bin        # VM 编译器生成的 App 打包产物（MQVM 格式）
└── Project.uvprojx        # Keil MDK 工程
```

---

## 🖥️ 硬件平台

| 项目       | 说明                                   |
| ---------- | -------------------------------------- |
| 主控       | STM32F103C6T6（Cortex-M3，72MHz）      |
| Flash      | 32 KB（0x08000000 ~ 0x08008000）       |
| RAM        | 10 KB（0x20000000 ~ 0x20002800）       |
| 调试       | ST-Link + OpenOCD + SEGGER RTT         |
| 外设       | LED ×2、UART ×2、ADC、PWM、按键、OLED 等 |

> 工程默认使用 `startup_stm32f10x_md.s`（中等容量）启动文件；`Library` 中同时保留了全系列标准外设库，便于移植到 F103 其他型号。

---

## 🚀 快速开始

### 1. 构建

**方式一：EIDE（推荐，VS Code 插件）**
```bash
# 用 VS Code 打开工程，安装 "Embedded IDE" 插件
# 点击状态栏构建/烧录按钮即可
```

**方式二：Keil MDK**
```
用 Keil MDK 打开 Project.uvprojx，编译烧录
```

### 2. 运行与调试

1. 启动 OpenOCD（连接 ST-Link）：
   ```bash
   openocd -f wifi_dap.cfg
   ```
2. 运行 RTT 交互终端（自动配置 RTT 并进入双向 Shell）：
   ```bash
   python RTT.py
   ```
3. 开机后 `main.c` 会依次完成：
   - 注册全部 VFS 设备（LED / UART / ADC / PWM / OLED / 按键 / 消息队列）
   - 创建 `TaskStart` 任务 → 等待 100ms 动态校准空闲任务速度（CPU 使用率基准）
   - 创建 `TaskIAP`（IAP 升级守护）、`vmtask`（虚拟机调度）任务
   - 初始化 Shell，进入 `minirtos# ` 命令行

### 3. 常用 Shell 命令

| 命令 | 说明 |
| ---- | ---- |
| `help`    | 列出全部命令 |
| `ps`      | 打印所有任务状态、优先级、栈使用水位、CPU 使用率 |
| `lsdev`   | 列出已注册的 VFS 设备 |
| `dev read <name>`   | 读取设备（如 `dev read adc_pot`） |
| `dev write <name> <val>` | 写入设备（如 `dev write led 1`） |
| `dev ioctl <name> <cmd>` | 设备控制 |
| `app load / app unload`  | 动态加载 / 卸载 VM App |
| `clear`   | 清屏 |
| `reboot`  | 软复位 |

---

## 🏗️ 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                         User / App                          │
│    TaskStart │ TaskIAP │ vmtask(VM) │ Shell │ ...(时间片)    │
├─────────────────────────────────────────────────────────────┤
│                     VM 虚拟机 (vm/)                          │
│      字节码解释器  ◄── syscall ──►  VFS 设备框架 (vfs/)      │
├─────────────────────────────────────────────────────────────┤
│                   RTOS 内核 (OS_Core/)                       │
│   调度器 │ 信号量 │ 互斥锁 │ 消息队列 │ 软件定时器 │ CPU统计   │
├─────────────────────────────────────────────────────────────┤
│                  Cortex-M3 移植层 (OS_Port/)                 │
│         SysTick 时基 │ PendSV 上下文切换 (汇编)              │
├─────────────────────────────────────────────────────────────┤
│                    STM32F103 硬件 / 标准外设库               │
└─────────────────────────────────────────────────────────────┘
```

### 内核调度原理

- 每个优先级一条就绪链表，共 32 级；`os_ready_bitmap` 位图标记非空链表，`__clz` 指令 **O(1)** 找到最高优先级就绪任务。
- **SysTick 中断**（1ms）驱动：递减延时任务、轮转时间片、刷新 CPU 使用率，最后触发调度。
- **PendSV 汇编**（`os_port.s`）完成真正的上下文切换：保存当前任务 R4~R11 到其私有栈（PSP），再从新任务栈恢复，实现任务之间的无缝切换。
- 互斥锁实现了 **优先级继承**：低优先级任务持有锁时若被高优先级任务请求，会临时提升持有者优先级，避免优先级反转。

---

## 🧩 关键设计细节

### VFS 设备框架

```c
int vfs_register_device(os_device_t *dev);  // 注册设备
int vfs_open(const char *name);             // 按名字打开，返回 fd
int vfs_read(int fd, uint32_t pos, void *buffer, uint32_t size);
int vfs_write(int fd, uint32_t pos, const void *buffer, uint32_t size);
int vfs_ioctl(int fd, int cmd, void *args);
int vfs_close(int fd);
```

设备结构体 `os_device_t` 内含 `open/read/write/ioctl/close` 函数指针与一把互斥锁；所有 `read/write/ioctl` 调用自动加锁，保证多任务并发访问安全。设备表支持最多 16 个句柄。

### VM 字节码虚拟机

**指令格式**：每条指令固定 16 字节 `[opcode:int32][reg0:int32][reg1:int32][reg2:int32]`。

```c
op_load / op_mov / op_add / op_sub / op_and / op_or / op_xor / op_lshift / op_rshift
op_jump / op_cmp / op_jeq / op_jne / op_jgt / op_jlt
op_call / op_ret
op_delay                       // VM 级睡眠，配合 os_sys_tick
op_syscall                     // 安全访问 VFS：OPEN/CLOSE/IOCTL/WRITE/READ
```

**App 打包格式（MQVM）**：`[Magic:"MQVM"][app_count][TOC 数组(offset,size)...][各 App 二进制]`，烧录于 Flash 槽位，可被 VM 动态挂载为独立沙箱运行。

**指令集定义**见 `isa.json`，**外设绑定与系统调用号**见 `peri.json`。

### IAP 在线升级协议

```
帧格式: [0xA5][cmd][size:2B][addr:4B][crc:4B][data...][0x5A]
流程:   START(擦除+总CRC) → DATA×n(逐帧CRC校验写Flash) → FINISH(回读校验)
```

`flash_iap` 模块负责安全地址校验、页擦除与编程；`iap_protocol` 负责帧解析；上位机 `iap.py` 实现完整发送流程。App 数据存放在 `0x08006000`（24KB）起的槽位区，每个槽位末尾 4 字节记录版本号，系统启动时自动寻找最高版本加载。

---

## 🛠️ 上位机工具脚本

| 脚本 | 作用 |
| ---- | ---- |
| `vm.py`       | VM 编译器（Tkinter GUI）：把汇编式源码编译为字节码 C 数组，并打包生成 `apps_bundle.bin`（MQVM 格式） |
| `iap.py`      | IAP 升级上位机：通过串口将 `apps_bundle.bin` 分包发送到单片机烧录 |
| `RTT.py`      | OpenOCD RTT 双向终端：自动配置 RTT 服务并进入交互式 Shell |
| `Xstdio.py`   | 综合开发工具（集成 VM 编译 / RTT / IAP，当前主逻辑已注释，可作为代码库参考） |

> 依赖：`pyserial`；VM 编译器还需 Python 内置的 `tkinter`。

---

## 🔍 内存布局（Flash）

```
0x08000000  ┌───────────────────────┐
            │   Bootloader / 固件   │  0 ~ 23KB
0x08006000  ├───────────────────────┤
            │  App 槽位区 ×8 (1KB)  │  24KB ~ 31KB
            │  每槽末尾: 版本号      │
0x08008000  └───────────────────────┘  32KB 结束
```

---

## 📌 已知限制 / 后续规划

- 内核仅支持单核单 CPU（符合目标平台），`os_msg_send` 在队列满时当前直接丢弃，未实现发送阻塞
- `System/` 下 Delay/Serial 模块暂未接入
- 后续可扩展：内存堆管理（`malloc/free`）、互斥锁递归支持、时间戳/时钟驱动、更多设备驱动

---

## 📄 许可

本项目仅供学习与交流使用。如需引用，请保留作者署名。

---

*项目名称与注释风格保留了个人学习笔记的痕迹，欢迎 Fork 一起完善。*
