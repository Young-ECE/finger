#!/usr/bin/env python3
"""
传感器数据实时可视化界面
支持从USB CDC读取14个传感器数据并实时绘制
"""

import sys
import time
from collections import deque
from datetime import datetime

import numpy as np
import pyqtgraph as pg
import serial
import serial.tools.list_ports
from PySide6.QtCore import QThread, Signal, QTimer
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QComboBox, QLabel, QFileDialog, QMessageBox, QGroupBox,
    QGridLayout
)


class SerialReader(QThread):
    """串口读取线程"""
    data_received = Signal(dict)
    error_occurred = Signal(str)

    def __init__(self, port, baudrate=115200):
        super().__init__()
        self.port = port
        self.baudrate = baudrate
        self.running = False
        self.serial_port = None

    def run(self):
        """线程主循环"""
        try:
            self.serial_port = serial.Serial(self.port, self.baudrate, timeout=1)
            self.running = True

            while self.running:
                if self.serial_port.in_waiting:
                    try:
                        line = self.serial_port.readline().decode('utf-8').strip()
                        if line:
                            data = self.parse_data(line)
                            if data:
                                self.data_received.emit(data)
                    except UnicodeDecodeError:
                        continue
                    except Exception as e:
                        self.error_occurred.emit(f"数据解析错误: {str(e)}")
                else:
                    self.msleep(10)

        except serial.SerialException as e:
            self.error_occurred.emit(f"串口错误: {str(e)}")
        finally:
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()

    def parse_data(self, line):
        """解析CSV格式的数据"""
        try:
            parts = line.split(',')
            if len(parts) != 14:
                return None

            return {
                'timestamp': time.time(),
                'aqi': int(parts[0]),
                'tvoc': int(parts[1]),
                'eco2': int(parts[2]),
                'als': int(parts[3]),
                'ps': int(parts[4]),
                'T1': int(parts[5]) / 10.0,
                'H1': int(parts[6]) / 10.0,
                'T2': int(parts[7]) / 10.0,
                'H2': int(parts[8]) / 10.0,
                'T3': int(parts[9]) / 10.0,
                'H3': int(parts[10]) / 10.0,
                'T4': int(parts[11]) / 10.0,
                'H4': int(parts[12]) / 10.0,
                'audio': int(parts[13])
            }
        except (ValueError, IndexError):
            return None

    def stop(self):
        """停止线程"""
        self.running = False
        self.wait()


class DataBuffer:
    """数据缓冲区，存储历史数据"""
    def __init__(self, max_points=500):
        self.max_points = max_points
        self.data = {
            'timestamp': deque(maxlen=max_points),
            'aqi': deque(maxlen=max_points),
            'tvoc': deque(maxlen=max_points),
            'eco2': deque(maxlen=max_points),
            'als': deque(maxlen=max_points),
            'ps': deque(maxlen=max_points),
            'T1': deque(maxlen=max_points),
            'H1': deque(maxlen=max_points),
            'T2': deque(maxlen=max_points),
            'H2': deque(maxlen=max_points),
            'T3': deque(maxlen=max_points),
            'H3': deque(maxlen=max_points),
            'T4': deque(maxlen=max_points),
            'H4': deque(maxlen=max_points),
            'audio': deque(maxlen=max_points)
        }
        self.start_time = time.time()

    def add_data(self, data_dict):
        """添加新数据"""
        for key, value in data_dict.items():
            if key in self.data:
                if key == 'timestamp':
                    # 转换为相对时间（秒）
                    self.data[key].append(value - self.start_time)
                else:
                    self.data[key].append(value)

    def get_array(self, key):
        """获取数据数组"""
        return np.array(self.data[key])

    def clear(self):
        """清空所有数据"""
        for key in self.data:
            self.data[key].clear()
        self.start_time = time.time()

    def save_to_csv(self, filename):
        """保存数据到CSV文件"""
        import csv

        if len(self.data['timestamp']) == 0:
            return False

        with open(filename, 'w', newline='') as f:
            writer = csv.writer(f)
            # 写入表头
            headers = ['timestamp', 'aqi', 'tvoc', 'eco2', 'als', 'ps',
                      'T1', 'H1', 'T2', 'H2', 'T3', 'H3', 'T4', 'H4', 'audio']
            writer.writerow(headers)

            # 写入数据
            for i in range(len(self.data['timestamp'])):
                row = [self.data[key][i] for key in headers]
                writer.writerow(row)

        return True


class MainWindow(QMainWindow):
    """主窗口"""
    def __init__(self):
        super().__init__()
        self.serial_reader = None
        self.data_buffer = DataBuffer(max_points=500)

        self.init_ui()
        self.setup_plots()
        self.refresh_ports()

        # 定时更新图表
        self.plot_timer = QTimer()
        self.plot_timer.timeout.connect(self.update_plots)
        self.plot_timer.start(100)  # 100ms更新一次

    def init_ui(self):
        """初始化UI"""
        self.setWindowTitle('传感器数据实时监控')
        self.setGeometry(100, 100, 1400, 900)

        # 中心部件
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)

        # 控制面板
        control_panel = self.create_control_panel()
        main_layout.addWidget(control_panel)

        # 图表区域
        plot_area = self.create_plot_area()
        main_layout.addWidget(plot_area)

    def create_control_panel(self):
        """创建控制面板"""
        group = QGroupBox("控制面板")
        layout = QHBoxLayout()

        # 串口选择
        layout.addWidget(QLabel("串口:"))
        self.port_combo = QComboBox()
        self.port_combo.setMinimumWidth(150)
        layout.addWidget(self.port_combo)

        self.refresh_btn = QPushButton("刷新")
        self.refresh_btn.clicked.connect(self.refresh_ports)
        layout.addWidget(self.refresh_btn)

        # 连接按钮
        self.connect_btn = QPushButton("连接")
        self.connect_btn.clicked.connect(self.toggle_connection)
        layout.addWidget(self.connect_btn)

        # 清空数据
        self.clear_btn = QPushButton("清空数据")
        self.clear_btn.clicked.connect(self.clear_data)
        layout.addWidget(self.clear_btn)

        # 保存数据
        self.save_btn = QPushButton("保存数据")
        self.save_btn.clicked.connect(self.save_data)
        layout.addWidget(self.save_btn)

        # 状态标签
        self.status_label = QLabel("状态: 未连接")
        layout.addWidget(self.status_label)

        layout.addStretch()

        group.setLayout(layout)
        return group

    def create_plot_area(self):
        """创建图表区域"""
        widget = QWidget()
        layout = QGridLayout()

        # 设置pyqtgraph样式
        pg.setConfigOption('background', 'w')
        pg.setConfigOption('foreground', 'k')

        # 创建各个图表
        # 第一行：空气质量
        self.aqi_plot = pg.PlotWidget(title="空气质量指数 (AQI)")
        self.aqi_plot.setLabel('left', 'AQI', units='')
        self.aqi_plot.setLabel('bottom', '时间', units='s')
        self.aqi_curve = self.aqi_plot.plot(pen=pg.mkPen(color='r', width=2))

        self.tvoc_plot = pg.PlotWidget(title="总挥发性有机物 (TVOC)")
        self.tvoc_plot.setLabel('left', 'TVOC', units='ppb')
        self.tvoc_plot.setLabel('bottom', '时间', units='s')
        self.tvoc_curve = self.tvoc_plot.plot(pen=pg.mkPen(color='g', width=2))

        self.eco2_plot = pg.PlotWidget(title="等效CO2 (eCO2)")
        self.eco2_plot.setLabel('left', 'eCO2', units='ppm')
        self.eco2_plot.setLabel('bottom', '时间', units='s')
        self.eco2_curve = self.eco2_plot.plot(pen=pg.mkPen(color='b', width=2))

        layout.addWidget(self.aqi_plot, 0, 0)
        layout.addWidget(self.tvoc_plot, 0, 1)
        layout.addWidget(self.eco2_plot, 0, 2)

        # 第二行：光学传感器
        self.als_plot = pg.PlotWidget(title="环境光强度 (ALS)")
        self.als_plot.setLabel('left', '光强', units='')
        self.als_plot.setLabel('bottom', '时间', units='s')
        self.als_curve = self.als_plot.plot(pen=pg.mkPen(color='y', width=2))

        self.ps_plot = pg.PlotWidget(title="接近传感器 (PS)")
        self.ps_plot.setLabel('left', '接近值', units='')
        self.ps_plot.setLabel('bottom', '时间', units='s')
        self.ps_curve = self.ps_plot.plot(pen=pg.mkPen(color='m', width=2))

        self.audio_plot = pg.PlotWidget(title="麦克风音频")
        self.audio_plot.setLabel('left', '音频值', units='')
        self.audio_plot.setLabel('bottom', '时间', units='s')
        self.audio_curve = self.audio_plot.plot(pen=pg.mkPen(color='c', width=2))

        layout.addWidget(self.als_plot, 1, 0)
        layout.addWidget(self.ps_plot, 1, 1)
        layout.addWidget(self.audio_plot, 1, 2)

        # 第三行：温度
        self.temp_plot = pg.PlotWidget(title="温度传感器 (4组)")
        self.temp_plot.setLabel('left', '温度', units='°C')
        self.temp_plot.setLabel('bottom', '时间', units='s')
        self.temp_plot.addLegend()
        self.temp1_curve = self.temp_plot.plot(pen=pg.mkPen(color='r', width=2), name='T1')
        self.temp2_curve = self.temp_plot.plot(pen=pg.mkPen(color='g', width=2), name='T2')
        self.temp3_curve = self.temp_plot.plot(pen=pg.mkPen(color='b', width=2), name='T3')
        self.temp4_curve = self.temp_plot.plot(pen=pg.mkPen(color='y', width=2), name='T4')

        # 第四行：湿度
        self.humid_plot = pg.PlotWidget(title="湿度传感器 (4组)")
        self.humid_plot.setLabel('left', '湿度', units='%')
        self.humid_plot.setLabel('bottom', '时间', units='s')
        self.humid_plot.addLegend()
        self.humid1_curve = self.humid_plot.plot(pen=pg.mkPen(color='r', width=2), name='H1')
        self.humid2_curve = self.humid_plot.plot(pen=pg.mkPen(color='g', width=2), name='H2')
        self.humid3_curve = self.humid_plot.plot(pen=pg.mkPen(color='b', width=2), name='H3')
        self.humid4_curve = self.humid_plot.plot(pen=pg.mkPen(color='y', width=2), name='H4')

        layout.addWidget(self.temp_plot, 2, 0, 1, 2)
        layout.addWidget(self.humid_plot, 2, 2, 1, 1)

        widget.setLayout(layout)
        return widget

    def setup_plots(self):
        """设置图表"""
        pass

    def refresh_ports(self):
        """刷新可用串口列表"""
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_combo.addItem(f"{port.device} - {port.description}")

    def toggle_connection(self):
        """切换连接状态"""
        if self.serial_reader is None or not self.serial_reader.isRunning():
            self.connect_serial()
        else:
            self.disconnect_serial()

    def connect_serial(self):
        """连接串口"""
        if self.port_combo.count() == 0:
            QMessageBox.warning(self, "错误", "没有可用的串口")
            return

        port_text = self.port_combo.currentText()
        port = port_text.split(' - ')[0]

        self.serial_reader = SerialReader(port)
        self.serial_reader.data_received.connect(self.on_data_received)
        self.serial_reader.error_occurred.connect(self.on_error)
        self.serial_reader.start()

        self.connect_btn.setText("断开")
        self.status_label.setText(f"状态: 已连接到 {port}")
        self.port_combo.setEnabled(False)
        self.refresh_btn.setEnabled(False)

    def disconnect_serial(self):
        """断开串口"""
        if self.serial_reader:
            self.serial_reader.stop()
            self.serial_reader = None

        self.connect_btn.setText("连接")
        self.status_label.setText("状态: 未连接")
        self.port_combo.setEnabled(True)
        self.refresh_btn.setEnabled(True)

    def on_data_received(self, data):
        """接收到新数据"""
        self.data_buffer.add_data(data)

    def on_error(self, error_msg):
        """处理错误"""
        QMessageBox.critical(self, "错误", error_msg)
        self.disconnect_serial()

    def update_plots(self):
        """更新所有图表"""
        if len(self.data_buffer.data['timestamp']) == 0:
            return

        time_data = self.data_buffer.get_array('timestamp')

        # 更新空气质量
        self.aqi_curve.setData(time_data, self.data_buffer.get_array('aqi'))
        self.tvoc_curve.setData(time_data, self.data_buffer.get_array('tvoc'))
        self.eco2_curve.setData(time_data, self.data_buffer.get_array('eco2'))

        # 更新光学传感器
        self.als_curve.setData(time_data, self.data_buffer.get_array('als'))
        self.ps_curve.setData(time_data, self.data_buffer.get_array('ps'))

        # 更新温度
        self.temp1_curve.setData(time_data, self.data_buffer.get_array('T1'))
        self.temp2_curve.setData(time_data, self.data_buffer.get_array('T2'))
        self.temp3_curve.setData(time_data, self.data_buffer.get_array('T3'))
        self.temp4_curve.setData(time_data, self.data_buffer.get_array('T4'))

        # 更新湿度
        self.humid1_curve.setData(time_data, self.data_buffer.get_array('H1'))
        self.humid2_curve.setData(time_data, self.data_buffer.get_array('H2'))
        self.humid3_curve.setData(time_data, self.data_buffer.get_array('H3'))
        self.humid4_curve.setData(time_data, self.data_buffer.get_array('H4'))

        # 更新音频
        self.audio_curve.setData(time_data, self.data_buffer.get_array('audio'))

    def clear_data(self):
        """清空数据"""
        self.data_buffer.clear()
        self.status_label.setText(self.status_label.text() + " | 数据已清空")

    def save_data(self):
        """保存数据到文件"""
        if len(self.data_buffer.data['timestamp']) == 0:
            QMessageBox.warning(self, "警告", "没有数据可保存")
            return

        filename, _ = QFileDialog.getSaveFileName(
            self, "保存数据",
            f"sensor_data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv",
            "CSV文件 (*.csv)"
        )

        if filename:
            if self.data_buffer.save_to_csv(filename):
                QMessageBox.information(self, "成功", f"数据已保存到:\n{filename}")
            else:
                QMessageBox.critical(self, "错误", "保存数据失败")

    def closeEvent(self, event):
        """关闭窗口时清理资源"""
        self.disconnect_serial()
        event.accept()


def main():
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == '__main__':
    main()
