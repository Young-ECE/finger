#!/usr/bin/env python3
"""
传感器实时可视化 - 无麦克风版
支持：VCNL4040 + ICM42688 + 8xBME280(温湿压)
数据格式：35字段，10Hz采样率
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
    QPushButton, QComboBox, QLabel, QMessageBox, QGroupBox, QGridLayout, QTabWidget
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
                        pass  # 静默处理解析错误
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
        格式: ALS,PS,AccX,AccY,AccZ,GyroX,GyroY,GyroZ,IMU_Temp,
              T0-T7(8个),H0-H7(8个),P0-P7(8个),MicL,MicR (35字段)
        """
        try:
            parts = line.split(',')
            if len(parts) < 33:  # 至少需要33字段（不含麦克风）
                return None

            data = {
                'timestamp': time.time(),
                # VCNL4040
                'als': int(parts[0]),
                'ps': int(parts[1]),
                # ICM42688 IMU (高精度浮点数)
                'accel_x': float(parts[2]),  # g (3位小数)
                'accel_y': float(parts[3]),
                'accel_z': float(parts[4]),
                'gyro_x': float(parts[5]),   # °/s (2位小数)
                'gyro_y': float(parts[6]),
                'gyro_z': float(parts[7]),
                'imu_temp': float(parts[8]), # °C (2位小数)
            }
            
            # BME280 x8 温度 (索引9-16)
            for i in range(8):
                data[f'T{i}'] = float(parts[9 + i])
            
            # BME280 x8 湿度 (索引17-24)
            for i in range(8):
                data[f'H{i}'] = float(parts[17 + i])
            
            # BME280 x8 气压 (索引25-32)
            for i in range(8):
                data[f'P{i}'] = float(parts[25 + i])
            
            return data
            
        except (ValueError, IndexError):
            return None

    def stop(self):
        """停止线程"""
        self.running = False


class SensorViewer(QMainWindow):
    """主窗口"""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("传感器实时监控（无麦克风版）")
        self.setGeometry(100, 100, 1600, 1000)

        # 数据缓冲区
        self.max_points = 300
        self.data_buffer = {
            'timestamp': deque(maxlen=self.max_points),
            'als': deque(maxlen=self.max_points),
            'ps': deque(maxlen=self.max_points),
            'accel_x': deque(maxlen=self.max_points),
            'accel_y': deque(maxlen=self.max_points),
            'accel_z': deque(maxlen=self.max_points),
            'gyro_x': deque(maxlen=self.max_points),
            'gyro_y': deque(maxlen=self.max_points),
            'gyro_z': deque(maxlen=self.max_points),
            'imu_temp': deque(maxlen=self.max_points),
        }
        # 8个BME280传感器：温湿压
        for i in range(8):
            self.data_buffer[f'T{i}'] = deque(maxlen=self.max_points)
            self.data_buffer[f'H{i}'] = deque(maxlen=self.max_points)
            self.data_buffer[f'P{i}'] = deque(maxlen=self.max_points)

        self.serial_reader = None
        self.setup_ui()

        # 更新定时器
        self.update_timer = QTimer()
        self.update_timer.timeout.connect(self.update_plots)
        self.update_timer.start(100)

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

        # 标签页
        self.tabs = QTabWidget()
        
        # 标签页1: 温湿度
        self.temp_hum_tab = self.create_temp_hum_tab()
        self.tabs.addTab(self.temp_hum_tab, "温湿度")
        
        # 标签页2: IMU
        self.imu_tab = self.create_imu_tab()
        self.tabs.addTab(self.imu_tab, "IMU")
        
        # 标签页3: 光学传感器
        self.optical_tab = self.create_optical_tab()
        self.tabs.addTab(self.optical_tab, "光学传感器")
        
        layout.addWidget(self.tabs)

    def create_temp_hum_tab(self):
        """创建温湿度标签页"""
        widget = QWidget()
        layout = QVBoxLayout(widget)

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
            temp_label.setStyleSheet("font-weight: bold; color: #e74c3c; font-size: 14px;")
            values_layout.addWidget(temp_label, row, col + 1)
            self.temp_labels.append(temp_label)
            
            hum_label = QLabel("H: -- %")
            hum_label.setStyleSheet("font-weight: bold; color: #3498db; font-size: 14px;")
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
        
        self.hum_curves = []
        for i in range(1, 8):
            curve = self.hum_plot_widget.plot(pen=pg.mkPen(color=colors[i-1], width=2), 
                                              name=f'传感器{i}')
            self.hum_curves.append(curve)
        
        layout.addWidget(self.hum_plot_widget, 1)
        
        return widget

    def create_imu_tab(self):
        """创建IMU标签页"""
        widget = QWidget()
        layout = QVBoxLayout(widget)

        # 实时数值
        values_group = QGroupBox("实时数值")
        values_layout = QGridLayout()
        
        self.accel_x_label = QLabel("AccelX: -- g")
        self.accel_y_label = QLabel("AccelY: -- g")
        self.accel_z_label = QLabel("AccelZ: -- g")
        self.gyro_x_label = QLabel("GyroX: -- °/s")
        self.gyro_y_label = QLabel("GyroY: -- °/s")
        self.gyro_z_label = QLabel("GyroZ: -- °/s")
        self.imu_temp_label = QLabel("温度: -- °C")
        
        for label in [self.accel_x_label, self.accel_y_label, self.accel_z_label,
                      self.gyro_x_label, self.gyro_y_label, self.gyro_z_label, self.imu_temp_label]:
            label.setStyleSheet("font-weight: bold; font-size: 14px;")
        
        values_layout.addWidget(QLabel("加速度:"), 0, 0)
        values_layout.addWidget(self.accel_x_label, 0, 1)
        values_layout.addWidget(self.accel_y_label, 0, 2)
        values_layout.addWidget(self.accel_z_label, 0, 3)
        
        values_layout.addWidget(QLabel("陀螺仪:"), 1, 0)
        values_layout.addWidget(self.gyro_x_label, 1, 1)
        values_layout.addWidget(self.gyro_y_label, 1, 2)
        values_layout.addWidget(self.gyro_z_label, 1, 3)
        
        values_layout.addWidget(self.imu_temp_label, 2, 0, 1, 2)
        
        values_group.setLayout(values_layout)
        layout.addWidget(values_group)

        # 加速度曲线
        self.accel_plot = pg.PlotWidget(title="加速度 (g)")
        self.accel_plot.setLabel('left', '加速度 (g)')
        self.accel_plot.setLabel('bottom', '时间 (秒)')
        self.accel_plot.addLegend()
        self.accel_plot.showGrid(x=True, y=True)
        
        self.accel_x_curve = self.accel_plot.plot(pen=pg.mkPen(color='r', width=2), name='X')
        self.accel_y_curve = self.accel_plot.plot(pen=pg.mkPen(color='g', width=2), name='Y')
        self.accel_z_curve = self.accel_plot.plot(pen=pg.mkPen(color='b', width=2), name='Z')
        
        layout.addWidget(self.accel_plot, 1)

        # 陀螺仪曲线
        self.gyro_plot = pg.PlotWidget(title="陀螺仪 (°/s)")
        self.gyro_plot.setLabel('left', '角速度 (°/s)')
        self.gyro_plot.setLabel('bottom', '时间 (秒)')
        self.gyro_plot.addLegend()
        self.gyro_plot.showGrid(x=True, y=True)
        
        self.gyro_x_curve = self.gyro_plot.plot(pen=pg.mkPen(color='r', width=2), name='X')
        self.gyro_y_curve = self.gyro_plot.plot(pen=pg.mkPen(color='g', width=2), name='Y')
        self.gyro_z_curve = self.gyro_plot.plot(pen=pg.mkPen(color='b', width=2), name='Z')
        
        layout.addWidget(self.gyro_plot, 1)
        
        return widget

    def create_optical_tab(self):
        """创建光学传感器标签页"""
        widget = QWidget()
        layout = QVBoxLayout(widget)

        # 实时数值
        values_group = QGroupBox("实时数值")
        values_layout = QHBoxLayout()
        
        self.als_label = QLabel("环境光: -- lux")
        self.ps_label = QLabel("接近度: --")
        
        for label in [self.als_label, self.ps_label]:
            label.setStyleSheet("font-weight: bold; font-size: 16px;")
        
        values_layout.addWidget(self.als_label)
        values_layout.addWidget(self.ps_label)
        values_layout.addStretch()
        
        values_group.setLayout(values_layout)
        layout.addWidget(values_group)

        # 环境光曲线
        self.als_plot = pg.PlotWidget(title="环境光强度")
        self.als_plot.setLabel('left', '光强 (lux)')
        self.als_plot.setLabel('bottom', '时间 (秒)')
        self.als_plot.showGrid(x=True, y=True)
        self.als_curve = self.als_plot.plot(pen=pg.mkPen(color='y', width=2))
        
        layout.addWidget(self.als_plot, 1)

        # 接近度曲线
        self.ps_plot = pg.PlotWidget(title="接近度")
        self.ps_plot.setLabel('left', '接近度值')
        self.ps_plot.setLabel('bottom', '时间 (秒)')
        self.ps_plot.showGrid(x=True, y=True)
        self.ps_curve = self.ps_plot.plot(pen=pg.mkPen(color='m', width=2))
        
        layout.addWidget(self.ps_plot, 1)
        
        return widget

    def refresh_ports(self):
        """刷新串口列表"""
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_combo.addItem(f"{port.device} - {port.description}", port.device)

    def toggle_connection(self):
        """切换连接状态"""
        if self.serial_reader and self.serial_reader.running:
            self.serial_reader.stop()
            self.serial_reader.wait()
            self.serial_reader = None
            self.connect_btn.setText("连接")
            self.status_label.setText("未连接")
        else:
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
        if len(self.data_buffer['timestamp']) == 0:
            self.start_time = data['timestamp']
        
        relative_time = data['timestamp'] - self.start_time
        self.data_buffer['timestamp'].append(relative_time)
        
        # 存储所有数据
        for key in data:
            if key != 'timestamp' and key in self.data_buffer:
                self.data_buffer[key].append(data[key])

    def on_error(self, error_msg):
        """处理错误"""
        self.status_label.setText(f"错误: {error_msg}")

    def update_plots(self):
        """更新图表"""
        if len(self.data_buffer['timestamp']) < 2:
            return

        timestamps = np.array(self.data_buffer['timestamp'])
        
        # 更新温湿度
        for i in range(1, 8):
            if len(self.data_buffer[f'T{i}']) > 0:
                temps = np.array(self.data_buffer[f'T{i}'])
                hums = np.array(self.data_buffer[f'H{i}'])
                self.temp_curves[i-1].setData(timestamps, temps)
                self.hum_curves[i-1].setData(timestamps, hums)
                self.temp_labels[i-1].setText(f"T: {temps[-1]:.1f} °C")
                self.hum_labels[i-1].setText(f"H: {hums[-1]:.1f} %")
        
        # 更新IMU
        if len(self.data_buffer['accel_x']) > 0:
            self.accel_x_curve.setData(timestamps, np.array(self.data_buffer['accel_x']))
            self.accel_y_curve.setData(timestamps, np.array(self.data_buffer['accel_y']))
            self.accel_z_curve.setData(timestamps, np.array(self.data_buffer['accel_z']))
            
            self.gyro_x_curve.setData(timestamps, np.array(self.data_buffer['gyro_x']))
            self.gyro_y_curve.setData(timestamps, np.array(self.data_buffer['gyro_y']))
            self.gyro_z_curve.setData(timestamps, np.array(self.data_buffer['gyro_z']))
            
            self.accel_x_label.setText(f"AccelX: {self.data_buffer['accel_x'][-1]:.2f} g")
            self.accel_y_label.setText(f"AccelY: {self.data_buffer['accel_y'][-1]:.2f} g")
            self.accel_z_label.setText(f"AccelZ: {self.data_buffer['accel_z'][-1]:.2f} g")
            
            self.gyro_x_label.setText(f"GyroX: {self.data_buffer['gyro_x'][-1]:.1f} °/s")
            self.gyro_y_label.setText(f"GyroY: {self.data_buffer['gyro_y'][-1]:.1f} °/s")
            self.gyro_z_label.setText(f"GyroZ: {self.data_buffer['gyro_z'][-1]:.1f} °/s")
            
            self.imu_temp_label.setText(f"温度: {self.data_buffer['imu_temp'][-1]:.1f} °C")
        
        # 更新光学传感器
        if len(self.data_buffer['als']) > 0:
            self.als_curve.setData(timestamps, np.array(self.data_buffer['als']))
            self.ps_curve.setData(timestamps, np.array(self.data_buffer['ps']))
            
            self.als_label.setText(f"环境光: {self.data_buffer['als'][-1]} lux")
            self.ps_label.setText(f"接近度: {self.data_buffer['ps'][-1]}")

    def closeEvent(self, event):
        """关闭窗口时的清理工作"""
        if self.serial_reader and self.serial_reader.running:
            self.serial_reader.stop()
            self.serial_reader.wait()
        event.accept()


def main():
    app = QApplication(sys.argv)
    window = SensorViewer()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()

