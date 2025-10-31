# STM32F446 开发环境配置指南

## 项目概述

- **芯片型号**: STM32F446RET6 (Cortex-M4, 512KB Flash, 128KB RAM)
- **开发板**: 自定义PCB
- **工具链**: ARM GCC (arm-none-eabi-gcc)
- **构建系统**: Makefile
- **调试器**: ST-Link V2/V3 (SWD接口)
- **IDE**: VSCode + Cortex-Debug插件

---

## 快速开始

### 1. 首次安装工具链

```bash
# 进入项目目录
cd /home/gong3ubuntu/Finger/finger

# 执行安装脚本（需要sudo权限）
./install_toolchain.sh

# 安装完成后，重新登录或运行
newgrp plugdev
```

### 2. 每次开发前激活环境

```bash
# 进入项目目录
cd /home/gong3ubuntu/Finger/finger

# 激活stm32-dev conda环境
source setup_env.sh
```

### 3. 编译项目

```bash
# 清理并重新编译
make clean
make

# 或者直接编译
make
```

### 4. 烧录固件

#### 方法1: 使用OpenOCD命令行
```bash
# 连接ST-Link并烧录
openocd -f openocd.cfg -c "program build/finger.elf verify reset exit"
```

#### 方法2: 使用VSCode任务
- 按 `Ctrl+Shift+B` 选择构建任务
- 或在命令面板 (`Ctrl+Shift+P`) 中运行 "Tasks: Run Task"

#### 方法3: 使用STM32_Programmer_CLI
```bash
STM32_Programmer_CLI -c port=SWD -w build/finger.bin 0x08000000 -v -rst
```

### 5. 调试

#### VSCode图形化调试
1. 连接ST-Link到开发板
2. 在VSCode中按 `F5` 或点击左侧"运行和调试"图标
3. 选择以下调试配置之一：
   - **Debug STM32F446 (OpenOCD - ST-Link)**: 使用系统OpenOCD配置
   - **Debug STM32F446 (OpenOCD - Project Config)**: 使用项目的openocd.cfg
   - **Attach to STM32F446 (OpenOCD)**: 附加到正在运行的程序

#### 命令行调试
```bash
# 终端1: 启动OpenOCD服务器
openocd -f openocd.cfg

# 终端2: 启动GDB
gdb-multiarch build/finger.elf
(gdb) target remote :3333
(gdb) load
(gdb) monitor reset halt
(gdb) continue
```

---

## 安装的工具

### 系统级工具（通过apt安装）
- `arm-none-eabi-gcc`: ARM GCC编译器
- `arm-none-eabi-binutils`: 二进制工具（ld, as, objcopy等）
- `arm-none-eabi-newlib`: C标准库
- `gdb-multiarch`: 多架构GDB调试器
- `openocd`: 开源片上调试器
- `make`: 构建工具

### Python工具（conda环境stm32-dev）
- `pyserial`: 串口通信库
- `pyocd`: Python实现的调试器（备用）

---

## VSCode推荐插件

### 必装插件
1. **Cortex-Debug** (marus25.cortex-debug)
   - ARM Cortex-M调试支持
   - 外设寄存器查看（通过SVD文件）

2. **C/C++** (ms-vscode.cpptools)
   - 代码补全、智能感知
   - 已配置ARM交叉编译器路径

### 可选插件
3. **STM32 VS Code Extension** (stmicroelectronics.stm32-vscode-extension)
   - ST官方扩展
   - 集成STM32CubeMX
   - 安装方法：在VSCode扩展市场搜索 "STM32"

4. **LinkerScript** (ZixuanWang.linkerscript)
   - .ld链接脚本语法高亮

5. **Arm Assembly** (dan-c-underwood.arm)
   - ARM汇编语法支持

---

## 项目结构

```
finger/
├── Core/                      # 核心代码
│   ├── Inc/                   # 头文件
│   │   ├── main.h
│   │   ├── *_sensor.h         # 传感器驱动头文件
│   │   └── ...
│   └── Src/                   # 源文件
│       ├── main.c
│       ├── *_sensor.c         # 传感器驱动实现
│       └── ...
├── Drivers/                   # STM32 HAL驱动和CMSIS
│   ├── STM32F4xx_HAL_Driver/
│   └── CMSIS/
├── USB_DEVICE/                # USB CDC设备
├── Middlewares/               # USB中间件
├── build/                     # 构建输出目录
├── .vscode/                   # VSCode配置
│   ├── launch.json            # 调试配置
│   ├── tasks.json             # 构建任务
│   ├── c_cpp_properties.json  # C/C++配置
│   └── settings.json          # 工作区设置
├── Makefile                   # 构建脚本
├── finger.ioc                 # STM32CubeMX项目文件
├── STM32F446XX_FLASH.ld       # 链接脚本
├── STM32F446x.svd             # 外设定义文件（用于调试）
├── openocd.cfg                # OpenOCD配置
├── install_toolchain.sh       # 工具链安装脚本
├── setup_env.sh               # 环境激活脚本
└── README_DEV.md              # 本文件
```

---

## 常见问题

### 1. 编译错误：找不到arm-none-eabi-gcc

**解决方法**:
```bash
# 检查是否安装
which arm-none-eabi-gcc

# 如果未安装，运行
./install_toolchain.sh
```

### 2. OpenOCD连接失败

**可能原因**:
- ST-Link未连接或未识别
- USB权限问题
- OpenOCD配置错误

**解决方法**:
```bash
# 检查ST-Link是否识别
lsusb | grep STMicro

# 重新加载udev规则
sudo udevadm control --reload-rules
sudo udevadm trigger

# 测试OpenOCD连接
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg
```

### 3. VSCode调试时无法启动GDB

**解决方法**:
```bash
# 确保gdb-multiarch已安装
which gdb-multiarch

# 如果未安装
sudo apt install gdb-multiarch
```

### 4. 外设寄存器查看器为空

**原因**: SVD文件未正确加载

**解决方法**:
- 确认 `STM32F446x.svd` 文件存在于项目根目录
- 检查 `.vscode/launch.json` 中的 `svdFile` 路径

### 5. USB CDC设备不显示

**可能原因**:
- 驱动问题
- USB连接问题

**解决方法**:
```bash
# 查看串口设备
ls /dev/ttyACM*

# 如果需要，添加用户到dialout组
sudo usermod -a -G dialout $USER
```

---

## 开发工作流

### 标准开发流程
1. 激活环境: `source setup_env.sh`
2. 修改代码
3. 编译: `make`
4. 烧录: `openocd -f openocd.cfg -c "program build/finger.elf verify reset exit"`
5. 测试功能

### 使用STM32CubeMX修改配置
1. 打开 `finger.ioc`
2. 修改外设配置
3. 生成代码（注意：会覆盖部分自动生成的文件）
4. 重新编译并测试

### 调试步骤
1. 设置断点（在VSCode编辑器左侧点击）
2. 按 `F5` 启动调试
3. 使用调试控制（F10单步、F11步入、F5继续）
4. 在"变量"面板查看变量值
5. 在"外设"面板查看寄存器值

---

## 性能优化提示

### 编译优化
- Debug模式: `-Og` (已配置)
- Release模式: 修改Makefile中的 `OPT = -O2` 或 `-O3`

### 代码大小优化
```makefile
# 在Makefile中添加
CFLAGS += -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections
```

### 启用Link Time Optimization (LTO)
```makefile
CFLAGS += -flto
LDFLAGS += -flto
```

---

## 串口监控

### 使用screen
```bash
screen /dev/ttyACM0 115200
# 退出: Ctrl+A, 然后按K
```

### 使用minicom
```bash
minicom -D /dev/ttyACM0 -b 115200
```

### 使用Python串口工具
```bash
# 激活stm32-dev环境后
python -m serial.tools.miniterm /dev/ttyACM0 115200
```

---

## 参考资源

### 官方文档
- [STM32F446 数据手册](stuff/STM32F446XX.pdf)
- [STM32F446 参考手册](https://www.st.com/resource/en/reference_manual/dm00135183.pdf)
- [STM32 HAL库文档](https://www.st.com/resource/en/user_manual/dm00105879.pdf)

### 在线资源
- [STM32官方Wiki](https://wiki.st.com/)
- [OpenOCD文档](http://openocd.org/documentation/)
- [Cortex-Debug插件文档](https://github.com/Marus/cortex-debug/wiki)

---

## 许可证

本项目遵循项目根目录的许可证声明。

---

## 更新日志

- 2025-10-30: 初始化开发环境配置
  - 创建conda环境stm32-dev
  - 配置VSCode调试环境
  - 添加OpenOCD配置文件
  - 下载SVD文件
