#!/bin/bash
# STM32开发工具链安装脚本
# 直接运行: ./install_toolchain.sh (不要加sudo)

set -e

# 获取真实用户名（即使脚本被sudo执行）
REAL_USER="${SUDO_USER:-$USER}"

# 检查是否有sudo权限
if ! sudo -v; then
    echo "错误: 此脚本需要sudo权限"
    exit 1
fi

echo "=== 开始安装STM32开发工具链 ==="
echo "当前用户: $REAL_USER"

# 更新软件包列表
echo "1. 更新软件包列表..."
sudo apt update

# 安装ARM工具链
echo "2. 安装ARM GCC工具链..."
sudo apt install -y gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi

# 安装GDB调试器
echo "3. 安装GDB调试器..."
sudo apt install -y gdb-multiarch

# 安装Make和构建工具
echo "4. 安装Make和构建工具..."
sudo apt install -y make build-essential

# 安装OpenOCD
echo "5. 安装OpenOCD调试工具..."
sudo apt install -y openocd

# 配置ST-Link USB访问权限（udev规则）
echo "6. 配置ST-Link USB访问权限..."
sudo tee /etc/udev/rules.d/99-stlink.rules > /dev/null << 'EOF'
# ST-Link V2
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="3748", MODE="0666", GROUP="plugdev"
# ST-Link V2.1
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="374b", MODE="0666", GROUP="plugdev"
# ST-Link V3
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="374d", MODE="0666", GROUP="plugdev"
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="374e", MODE="0666", GROUP="plugdev"
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="374f", MODE="0666", GROUP="plugdev"
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="3753", MODE="0666", GROUP="plugdev"
EOF

# 重新加载udev规则
echo "7. 重新加载udev规则..."
sudo udevadm control --reload-rules
sudo udevadm trigger

# 将当前用户添加到plugdev组（如果还未添加）
echo "8. 将当前用户 $REAL_USER 添加到plugdev组..."
sudo usermod -a -G plugdev "$REAL_USER"

# 可选：安装STM32CubeProgrammer（如果需要）
echo ""
echo "=== 工具链安装完成 ==="
echo ""
echo "已安装的工具："
echo "  - arm-none-eabi-gcc: $(arm-none-eabi-gcc --version | head -n1)"
echo "  - gdb-multiarch: $(gdb-multiarch --version | head -n1)"
echo "  - openocd: $(openocd --version 2>&1 | head -n1)"
echo "  - make: $(make --version | head -n1)"
echo ""
echo "重要提示："
echo "  1. 需要重新登录或重启系统才能使plugdev组生效"
echo "  2. 连接ST-Link后，运行 'lsusb' 查看设备是否被识别"
echo "  3. 测试OpenOCD连接: 'openocd -f interface/stlink.cfg -f target/stm32f4x.cfg'"
echo ""
