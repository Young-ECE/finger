#!/bin/bash
# 快速编译和烧录脚本

set -e

echo "================================"
echo "  STM32 编译和烧录脚本"
echo "================================"
echo ""

# 检查是否传入了参数
SKIP_BUILD=false
if [ "$1" == "--skip-build" ]; then
    SKIP_BUILD=true
fi

# 编译项目
if [ "$SKIP_BUILD" == false ]; then
    echo "[1/3] 正在编译项目..."
    make
    if [ $? -ne 0 ]; then
        echo "❌ 编译失败！"
        exit 1
    fi
    echo "✓ 编译成功"
    echo ""
else
    echo "[1/3] 跳过编译"
    echo ""
fi

# 检查ELF文件是否存在
if [ ! -f "build/finger.elf" ]; then
    echo "❌ 错误: build/finger.elf 不存在"
    echo "请先运行 'make' 编译项目"
    exit 1
fi

# 显示文件大小
echo "[2/3] 固件信息:"
arm-none-eabi-size build/finger.elf
echo ""

# 烧录固件
echo "[3/3] 正在烧录固件到STM32..."
openocd -f openocd.cfg -c "program build/finger.elf verify reset exit"

if [ $? -eq 0 ]; then
    echo ""
    echo "================================"
    echo "  ✓ 烧录成功！"
    echo "================================"
else
    echo ""
    echo "================================"
    echo "  ❌ 烧录失败！"
    echo "================================"
    echo ""
    echo "排查步骤："
    echo "  1. 检查ST-Link是否连接: lsusb | grep STMicro"
    echo "  2. 检查开发板是否供电"
    echo "  3. 检查SWD连接线是否正常"
    echo "  4. 尝试手动运行: openocd -f openocd.cfg"
    exit 1
fi
