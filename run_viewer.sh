#!/bin/bash
# 传感器数据可视化启动脚本

echo "=========================================="
echo " Finger 传感器实时监控"
echo "=========================================="
echo ""
echo "当前固件配置："
echo "  - 采样率: 100Hz (10ms间隔)"
echo "  - 传感器: VCNL4040 + ICM42688 + 8x BME280"
echo "  - 数据字段: 35字段（温湿压）"
echo "  - 数据精度: IMU 3位小数, 温湿压 2位小数"
echo "  - USB缓冲: 4096字节"
echo "  - 麦克风: 已禁用（避免I2C/I2S冲突）"
echo ""
echo "启动完整版可视化工具..."
echo ""

# 检查依赖
if ! python3 -c "import PySide6" 2>/dev/null; then
    echo "错误：缺少PySide6，请安装："
    echo "  pip install PySide6 pyqtgraph pyserial numpy"
    exit 1
fi

# 运行可视化
python3 sensor_viewer_complete.py

