#!/usr/bin/env python3
"""
温湿度传感器实时可视化 - 简化版
只显示7个BME280传感器的温度和湿度数据
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
    QPushButton, QComboBox, QLabel, QMessageBox, QGroupBox, QGridLayout
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
                        # 跳过状态消息
                        if line and line not in ['OK', 'RESET'] and not line.startswith('===') and not line.startswith('CH'):
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
        """
        解析CSV格式的数据
        格式: T1,H1,T2,H2,T3,H3,T4,H4,T5,H5,T6,H6,T7,H7
        """
        try:
            parts = line.split(',')
            if len(parts) != 14:
                return None

            return {
                'timestamp': time.time(),
                'T1': float(parts[0]),
                'H1': float(parts[1]),
                'T2': float(parts[2]),
                'H2': float(parts[3]),
                'T3': float(parts[4]),
                'H3': float(parts[5]),
                'T4': float(parts[6]),
                'H4': float(parts[7]),
                'T5': float(parts[8]),
                'H5': float(parts[9]),
                'T6': float(parts[10]),
                'H6': float(parts[11]),
                'T7': float(parts[12]),
                'H7': float(parts[13]),
            }
        except (ValueError, IndexError) as e:
            return None

    def stop(self):
        """停止线程"""
        self.running = False


class TempHumidityViewer(QMainWindow):
    """主窗口"""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("温湿度传感器实时监控")
        self.setGeometry(100, 100, 1400, 900)

        # 数据缓冲区（保存最近300个数据点，约5分钟）
        self.max_points = 300
        self.data_buffer = {
            'timestamp': deque(maxlen=self.max_points),
        }
        for i in range(1, 8):
            self.data_buffer[f'T{i}'] = deque(maxlen=self.max_points)
            self.data_buffer[f'H{i}'] = deque(maxlen=self.max_points)

        # 串口读取线程
        self.serial_reader = None

        # 设置UI
        self.setup_ui()

        # 更新定时器
        self.update_timer = QTimer()
        self.update_timer.timeout.connect(self.update_plots)
        self.update_timer.start(100)  # 100ms更新一次

    def setup_ui(self):
        """设置UI界面"""
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)

        # 控制面板
        control_group = QGroupBox("连接控制")
        control_layout = QHBoxLayout()

        self.port_combo = QComboBox()
        self.refresh_ports()
        control_layout.addWidget(QLabel("串口:"))
        control_layout.addWidget(self.port_combo)

        self.refresh_btn = QPushButton("刷新端口")
        self.refresh_btn.clicked.connect(self.refresh_ports)
        control_layout.addWidget(self.refresh_btn)

        self.connect_btn = QPushButton("连接")
        self.connect_btn.clicked.connect(self.toggle_connection)
        control_layout.addWidget(self.connect_btn)

        self.status_label = QLabel("未连接")
        control_layout.addWidget(self.status_label)

        control_layout.addStretch()
        control_group.setLayout(control_layout)
        layout.addWidget(control_group)

        # 实时数值显示
        values_group = QGroupBox("实时数值")
        values_layout = QGridLayout()
        
        self.temp_labels = []
        self.hum_labels = []
        
        for i in range(1, 8):
            row = (i - 1) // 2
            col = ((i - 1) % 2) * 3
            
            values_layout.addWidget(QLabel(f"传感器{i}:"), row, col)
            
            temp_label = QLabel("T: -- °C")
            temp_label.setStyleSheet("font-weight: bold; color: #e74c3c;")
            values_layout.addWidget(temp_label, row, col + 1)
            self.temp_labels.append(temp_label)
            
            hum_label = QLabel("H: -- %")
            hum_label.setStyleSheet("font-weight: bold; color: #3498db;")
            values_layout.addWidget(hum_label, row, col + 2)
            self.hum_labels.append(hum_label)
        
        values_group.setLayout(values_layout)
        layout.addWidget(values_group)

        # 温度曲线图
        self.temp_plot_widget = pg.PlotWidget(title="温度曲线 (°C)")
        self.temp_plot_widget.setLabel('left', '温度 (°C)')
        self.temp_plot_widget.setLabel('bottom', '时间 (秒)')
        self.temp_plot_widget.addLegend()
        self.temp_plot_widget.showGrid(x=True, y=True)
        
        # 7条温度曲线
        colors = ['#e74c3c', '#3498db', '#2ecc71', '#f39c12', '#9b59b6', '#1abc9c', '#34495e']
        self.temp_curves = []
        for i in range(1, 8):
            curve = self.temp_plot_widget.plot(pen=pg.mkPen(color=colors[i-1], width=2), 
                                               name=f'传感器{i}')
            self.temp_curves.append(curve)
        
        layout.addWidget(self.temp_plot_widget, 1)

        # 湿度曲线图
        self.hum_plot_widget = pg.PlotWidget(title="湿度曲线 (%RH)")
        self.hum_plot_widget.setLabel('left', '相对湿度 (%)')
        self.hum_plot_widget.setLabel('bottom', '时间 (秒)')
        self.hum_plot_widget.addLegend()
        self.hum_plot_widget.showGrid(x=True, y=True)
        
        # 7条湿度曲线
        self.hum_curves = []
        for i in range(1, 8):
            curve = self.hum_plot_widget.plot(pen=pg.mkPen(color=colors[i-1], width=2), 
                                              name=f'传感器{i}')
            self.hum_curves.append(curve)
        
        layout.addWidget(self.hum_plot_widget, 1)

    def refresh_ports(self):
        """刷新串口列表"""
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_combo.addItem(f"{port.device} - {port.description}", port.device)

    def toggle_connection(self):
        """切换连接状态"""
        if self.serial_reader and self.serial_reader.running:
            # 断开连接
            self.serial_reader.stop()
            self.serial_reader.wait()
            self.serial_reader = None
            self.connect_btn.setText("连接")
            self.status_label.setText("未连接")
        else:
            # 建立连接
            port = self.port_combo.currentData()
            if not port:
                QMessageBox.warning(self, "错误", "请选择一个串口")
                return

            self.serial_reader = SerialReader(port)
            self.serial_reader.data_received.connect(self.on_data_received)
            self.serial_reader.error_occurred.connect(self.on_error)
            self.serial_reader.start()
            
            self.connect_btn.setText("断开")
            self.status_label.setText(f"已连接到 {port}")

    def on_data_received(self, data):
        """处理接收到的数据"""
        # 添加到缓冲区
        if len(self.data_buffer['timestamp']) == 0:
            self.start_time = data['timestamp']
        
        relative_time = data['timestamp'] - self.start_time
        self.data_buffer['timestamp'].append(relative_time)
        
        for i in range(1, 8):
            self.data_buffer[f'T{i}'].append(data[f'T{i}'])
            self.data_buffer[f'H{i}'].append(data[f'H{i}'])

    def on_error(self, error_msg):
        """处理错误"""
        self.status_label.setText(f"错误: {error_msg}")
        QMessageBox.warning(self, "错误", error_msg)

    def update_plots(self):
        """更新图表"""
        if len(self.data_buffer['timestamp']) < 2:
            return

        timestamps = np.array(self.data_buffer['timestamp'])
        
        # 更新温度曲线
        for i in range(1, 8):
            temps = np.array(self.data_buffer[f'T{i}'])
            self.temp_curves[i-1].setData(timestamps, temps)
            
            # 更新实时数值显示
            if len(temps) > 0:
                self.temp_labels[i-1].setText(f"T: {temps[-1]:.1f} °C")
        
        # 更新湿度曲线
        for i in range(1, 8):
            hums = np.array(self.data_buffer[f'H{i}'])
            self.hum_curves[i-1].setData(timestamps, hums)
            
            # 更新实时数值显示
            if len(hums) > 0:
                self.hum_labels[i-1].setText(f"H: {hums[-1]:.1f} %")

    def closeEvent(self, event):
        """关闭窗口时的清理工作"""
        if self.serial_reader and self.serial_reader.running:
            self.serial_reader.stop()
            self.serial_reader.wait()
        event.accept()


def main():
    app = QApplication(sys.argv)
    window = TempHumidityViewer()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()

