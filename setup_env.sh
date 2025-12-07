#!/bin/bash
# STM32开发环境激活脚本
# 使用方法: source setup_env.sh

# 颜色定义
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== STM32开发环境设置 ===${NC}"

# 检测conda是否存在
if ! command -v conda &> /dev/null; then
    echo -e "${RED}错误: 未找到conda。请先安装Miniconda或Anaconda。${NC}"
    return 1
fi

# 激活conda环境
echo -e "${YELLOW}激活stm32-dev conda环境...${NC}"
conda activate stm32-dev

if [ $? -ne 0 ]; then
    echo -e "${RED}错误: 无法激活stm32-dev环境。请运行 ./install_toolchain.sh 先安装环境。${NC}"
    return 1
fi

# 设置项目路径
export STM32_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo -e "${GREEN}项目根目录: ${STM32_PROJECT_ROOT}${NC}"

# 添加工具链路径到PATH（如果需要）
# export PATH="/usr/bin:$PATH"

# 验证工具是否可用
echo ""
echo -e "${YELLOW}验证开发工具...${NC}"

# 检查arm-none-eabi-gcc
if command -v arm-none-eabi-gcc &> /dev/null; then
    echo -e "${GREEN}✓ arm-none-eabi-gcc: $(arm-none-eabi-gcc --version | head -n1)${NC}"
else
    echo -e "${RED}✗ arm-none-eabi-gcc 未安装${NC}"
fi

# 检查gdb-multiarch
if command -v gdb-multiarch &> /dev/null; then
    echo -e "${GREEN}✓ gdb-multiarch: $(gdb-multiarch --version | head -n1)${NC}"
else
    echo -e "${RED}✗ gdb-multiarch 未安装${NC}"
fi

# 检查OpenOCD
if command -v openocd &> /dev/null; then
    echo -e "${GREEN}✓ openocd: $(openocd --version 2>&1 | head -n1)${NC}"
else
    echo -e "${RED}✗ openocd 未安装${NC}"
fi

# 检查make
if command -v make &> /dev/null; then
    echo -e "${GREEN}✓ make: $(make --version | head -n1)${NC}"
else
    echo -e "${RED}✗ make 未安装${NC}"
fi

echo ""
echo -e "${GREEN}环境设置完成！${NC}"
echo ""
echo "常用命令："
echo "  make           - 编译项目"
echo "  make clean     - 清理构建文件"
echo "  make flash     - 烧录固件（如果tasks.json中配置）"
echo "  openocd -f openocd.cfg  - 启动OpenOCD调试服务器"
echo ""
echo "VSCode调试："
echo "  1. 按 F5 开始调试"
echo "  2. 或点击左侧的 '运行和调试' 图标"
echo "  3. 选择 'Debug STM32F446 (OpenOCD - ST-Link)' 配置"
echo ""
