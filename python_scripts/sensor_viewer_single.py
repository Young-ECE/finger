#!/usr/bin/env python3
"""
传感器实时可视化 - 单窗口版
所有传感器数据显示在同一个可滚动窗口中
"""

import sys
import time
from collections import deque

import numpy as np
import pyqtgraph as pg
import serial
import serial.tools.list_ports
from PySide6.QtCore import QThread, Signal, QTimer
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QComboBox, QLabel, QMessageBox, QGroupBox, QGridLayout,
    QScrollArea
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
                        if line and line not in ['OK', 'RESET'] and not line.startswith('===') and not line.startswith('CH'):
                            data = self.parse_data(line)
                            if data:
                                self.data_received.emit(data)
                    except (UnicodeDecodeError, Exception):
                        pass
                else:
                    self.msleep(10)

        except serial.SerialException as e:
            self.error_occurred.emit(f"串口错误: {str(e)}")
        finally:
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()

    def parse_data(self, line):
        """解析数据：ALS,PS,AccX,AccY,AccZ,GyroX,GyroY,GyroZ,IMU_Temp,T1,H1,...,T7,H7"""
        try:
            parts = line.split(',')
            if len(parts) != 23:
                return None

            return {
                'timestamp': time.time(),
                'als': int(parts[0]),
                'ps': int(parts[1]),
                'accel_x': int(parts[2]) / 100.0,
                'accel_y': int(parts[3]) / 100.0,
                'accel_z': int(parts[4]) / 100.0,
                'gyro_x': int(parts[5]) / 10.0,
                'gyro_y': int(parts[6]) / 10.0,
                'gyro_z': int(parts[7]) / 10.0,
                'imu_temp': int(parts[8]) / 10.0,
                'T1': float(parts[9]), 'H1': float(parts[10]),
                'T2': float(parts[11]), 'H2': float(parts[12]),
                'T3': float(parts[13]), 'H3': float(parts[14]),
                'T4': float(parts[15]), 'H4': float(parts[16]),
                'T5': float(parts[17]), 'H5': float(parts[18]),
                'T6': float(parts[19]), 'H6': float(parts[20]),
                'T7': float(parts[21]), 'H7': float(parts[22]),
            }
        except (ValueError, IndexError):
            return None

    def stop(self):
        self.running = False


class SensorViewer(QMainWindow):
    """主窗口 - 单页面版本"""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("传感器实时监控 - 全景视图")
        self.setGeometry(50, 50, 1800, 1000)

        # 数据缓冲区
        self.max_points = 300
        self.data_buffer = {'timestamp': deque(maxlen=self.max_points)}
        
        # 初始化所有数据缓冲
        for key in ['als', 'ps', 'accel_x', 'accel_y', 'accel_z', 
                    'gyro_x', 'gyro_y', 'gyro_z', 'imu_temp']:
            self.data_buffer[key] = deque(maxlen=self.max_points)
        
        for i in range(1, 8):
            self.data_buffer[f'T{i}'] = deque(maxlen=self.max_points)
            self.data_buffer[f'H{i}'] = deque(maxlen=self.max_points)

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
        main_layout = QVBoxLayout(central_widget)

        # 控制面板
        control_group = QGroupBox("连接控制")
        control_layout = QHBoxLayout()

        self.port_combo = QComboBox()
        self.refresh_ports()
        control_layout.addWidget(QLabel("串口:"))
        control_layout.addWidget(self.port_combo)

        self.refresh_btn = QPushButton("🔄 刷新")
        self.refresh_btn.clicked.connect(self.refresh_ports)
        control_layout.addWidget(self.refresh_btn)

        self.connect_btn = QPushButton("🔌 连接")
        self.connect_btn.clicked.connect(self.toggle_connection)
        control_layout.addWidget(self.connect_btn)

        self.status_label = QLabel("⚪ 未连接")
        self.status_label.setStyleSheet("font-weight: bold; font-size: 14px;")
        control_layout.addWidget(self.status_label)

        control_layout.addStretch()
        control_group.setLayout(control_layout)
        main_layout.addWidget(control_group)

        # 滚动区域
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll_content = QWidget()
        layout = QVBoxLayout(scroll_content)

        # =================== 温湿度传感器区域 ===================
        temp_hum_group = QGroupBox("🌡️ 温湿度传感器 (7x BME280)")
        temp_hum_group.setStyleSheet("QGroupBox { font-weight: bold; font-size: 14px; }")
        temp_hum_layout = QVBoxLayout()

        # 实时数值显示（紧凑型）
        values_layout = QGridLayout()
        self.temp_labels = []
        self.hum_labels = []
        
        for i in range(1, 8):
            col = i - 1
            sensor_label = QLabel(f"S{i}")
            sensor_label.setStyleSheet("font-weight: bold; color: #7f8c8d;")
            values_layout.addWidget(sensor_label, 0, col)
            
            temp_label = QLabel("--°C")
            temp_label.setStyleSheet("font-weight: bold; color: #e74c3c; font-size: 16px;")
            values_layout.addWidget(temp_label, 1, col)
            self.temp_labels.append(temp_label)
            
            hum_label = QLabel("--%")
            hum_label.setStyleSheet("font-weight: bold; color: #3498db; font-size: 16px;")
            values_layout.addWidget(hum_label, 2, col)
            self.hum_labels.append(hum_label)
        
        temp_hum_layout.addLayout(values_layout)

        # 温度曲线（紧凑）
        self.temp_plot = pg.PlotWidget(title="温度 (°C)")
        self.temp_plot.setMaximumHeight(200)
        self.temp_plot.setLabel('left', 'T(°C)')
        self.temp_plot.showGrid(x=True, y=True, alpha=0.3)
        self.temp_plot.addLegend(offset=(10, 10))
        
        colors = ['#e74c3c', '#3498db', '#2ecc71', '#f39c12', '#9b59b6', '#1abc9c', '#34495e']
        self.temp_curves = []
        for i in range(1, 8):
            curve = self.temp_plot.plot(pen=pg.mkPen(color=colors[i-1], width=2), name=f'S{i}')
            self.temp_curves.append(curve)
        
        temp_hum_layout.addWidget(self.temp_plot)

        # 湿度曲线（紧凑）
        self.hum_plot = pg.PlotWidget(title="湿度 (%)")
        self.hum_plot.setMaximumHeight(200)
        self.hum_plot.setLabel('left', 'RH(%)')
        self.hum_plot.showGrid(x=True, y=True, alpha=0.3)
        self.hum_plot.addLegend(offset=(10, 10))
        
        self.hum_curves = []
        for i in range(1, 8):
            curve = self.hum_plot.plot(pen=pg.mkPen(color=colors[i-1], width=2), name=f'S{i}')
            self.hum_curves.append(curve)
        
        temp_hum_layout.addWidget(self.hum_plot)
        temp_hum_group.setLayout(temp_hum_layout)
        layout.addWidget(temp_hum_group)

        # =================== IMU传感器区域 ===================
        imu_group = QGroupBox("📐 IMU传感器 (ICM42688)")
        imu_group.setStyleSheet("QGroupBox { font-weight: bold; font-size: 14px; }")
        imu_layout = QVBoxLayout()

        # IMU实时数值
        imu_values_layout = QHBoxLayout()
        self.accel_x_label = QLabel("AccX: --")
        self.accel_y_label = QLabel("AccY: --")
        self.accel_z_label = QLabel("AccZ: --")
        self.gyro_x_label = QLabel("GyroX: --")
        self.gyro_y_label = QLabel("GyroY: --")
        self.gyro_z_label = QLabel("GyroZ: --")
        self.imu_temp_label = QLabel("Temp: --")
        
        for label in [self.accel_x_label, self.accel_y_label, self.accel_z_label,
                      self.gyro_x_label, self.gyro_y_label, self.gyro_z_label, self.imu_temp_label]:
            label.setStyleSheet("font-weight: bold; font-size: 13px; padding: 3px;")
            imu_values_layout.addWidget(label)
        
        imu_values_layout.addStretch()
        imu_layout.addLayout(imu_values_layout)

        # 加速度+陀螺仪组合图（更紧凑）
        imu_plots_layout = QHBoxLayout()
        
        self.accel_plot = pg.PlotWidget(title="加速度 (g)")
        self.accel_plot.setMaximumHeight(180)
        self.accel_plot.showGrid(x=True, y=True, alpha=0.3)
        self.accel_plot.addLegend(offset=(10, 10))
        self.accel_x_curve = self.accel_plot.plot(pen=pg.mkPen(color='r', width=2), name='X')
        self.accel_y_curve = self.accel_plot.plot(pen=pg.mkPen(color='g', width=2), name='Y')
        self.accel_z_curve = self.accel_plot.plot(pen=pg.mkPen(color='b', width=2), name='Z')
        imu_plots_layout.addWidget(self.accel_plot)
        
        self.gyro_plot = pg.PlotWidget(title="陀螺仪 (°/s)")
        self.gyro_plot.setMaximumHeight(180)
        self.gyro_plot.showGrid(x=True, y=True, alpha=0.3)
        self.gyro_plot.addLegend(offset=(10, 10))
        self.gyro_x_curve = self.gyro_plot.plot(pen=pg.mkPen(color='r', width=2), name='X')
        self.gyro_y_curve = self.gyro_plot.plot(pen=pg.mkPen(color='g', width=2), name='Y')
        self.gyro_z_curve = self.gyro_plot.plot(pen=pg.mkPen(color='b', width=2), name='Z')
        imu_plots_layout.addWidget(self.gyro_plot)
        
        imu_layout.addLayout(imu_plots_layout)
        imu_group.setLayout(imu_layout)
        layout.addWidget(imu_group)

        # =================== 光学传感器区域 ===================
        optical_group = QGroupBox("💡 光学传感器 (VCNL4040)")
        optical_group.setStyleSheet("QGroupBox { font-weight: bold; font-size: 14px; }")
        optical_layout = QVBoxLayout()

        # 光学传感器实时数值
        optical_values_layout = QHBoxLayout()
        self.als_label = QLabel("环境光: -- lux")
        self.ps_label = QLabel("接近度: --")
        
        for label in [self.als_label, self.ps_label]:
            label.setStyleSheet("font-weight: bold; font-size: 14px; padding: 5px;")
            optical_values_layout.addWidget(label)
        
        optical_values_layout.addStretch()
        optical_layout.addLayout(optical_values_layout)

        # 光学传感器图表（并排显示）
        optical_plots_layout = QHBoxLayout()
        
        self.als_plot = pg.PlotWidget(title="环境光强度")
        self.als_plot.setMaximumHeight(180)
        self.als_plot.showGrid(x=True, y=True, alpha=0.3)
        self.als_curve = self.als_plot.plot(pen=pg.mkPen(color='y', width=2))
        optical_plots_layout.addWidget(self.als_plot)
        
        self.ps_plot = pg.PlotWidget(title="接近度")
        self.ps_plot.setMaximumHeight(180)
        self.ps_plot.showGrid(x=True, y=True, alpha=0.3)
        self.ps_curve = self.ps_plot.plot(pen=pg.mkPen(color='m', width=2))
        optical_plots_layout.addWidget(self.ps_plot)
        
        optical_layout.addLayout(optical_plots_layout)
        optical_group.setLayout(optical_layout)
        layout.addWidget(optical_group)

        # 设置滚动区域
        scroll.setWidget(scroll_content)
        main_layout.addWidget(scroll)

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
            self.connect_btn.setText("🔌 连接")
            self.status_label.setText("⚪ 未连接")
        else:
            port = self.port_combo.currentData()
            if not port:
                QMessageBox.warning(self, "错误", "请选择一个串口")
                return

            self.serial_reader = SerialReader(port)
            self.serial_reader.data_received.connect(self.on_data_received)
            self.serial_reader.error_occurred.connect(self.on_error)
            self.serial_reader.start()
            
            self.connect_btn.setText("🔌 断开")
            self.status_label.setText(f"🟢 已连接到 {port}")

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
        self.status_label.setText(f"🔴 错误: {error_msg}")

    def update_plots(self):
        """更新所有图表"""
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
                self.temp_labels[i-1].setText(f"{temps[-1]:.1f}°C")
                self.hum_labels[i-1].setText(f"{hums[-1]:.1f}%")
        
        # 更新IMU
        if len(self.data_buffer['accel_x']) > 0:
            self.accel_x_curve.setData(timestamps, np.array(self.data_buffer['accel_x']))
            self.accel_y_curve.setData(timestamps, np.array(self.data_buffer['accel_y']))
            self.accel_z_curve.setData(timestamps, np.array(self.data_buffer['accel_z']))
            
            self.gyro_x_curve.setData(timestamps, np.array(self.data_buffer['gyro_x']))
            self.gyro_y_curve.setData(timestamps, np.array(self.data_buffer['gyro_y']))
            self.gyro_z_curve.setData(timestamps, np.array(self.data_buffer['gyro_z']))
            
            self.accel_x_label.setText(f"AccX: {self.data_buffer['accel_x'][-1]:.2f}g")
            self.accel_y_label.setText(f"AccY: {self.data_buffer['accel_y'][-1]:.2f}g")
            self.accel_z_label.setText(f"AccZ: {self.data_buffer['accel_z'][-1]:.2f}g")
            
            self.gyro_x_label.setText(f"GyroX: {self.data_buffer['gyro_x'][-1]:.0f}°/s")
            self.gyro_y_label.setText(f"GyroY: {self.data_buffer['gyro_y'][-1]:.0f}°/s")
            self.gyro_z_label.setText(f"GyroZ: {self.data_buffer['gyro_z'][-1]:.0f}°/s")
            
            self.imu_temp_label.setText(f"Temp: {self.data_buffer['imu_temp'][-1]:.1f}°C")
        
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

