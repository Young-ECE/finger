#!/usr/bin/env python3
"""
传感器实时可视化 - 完整版
支持所有传感器：VCNL4040 + ICM42688 + 8xBME280(温湿压) + 单麦克风
数据格式：als,ps,ax,ay,az,gx,gy,gz,imu_temp,t1-t8,h1-h8,p1-p8,mic (34字段)
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
    QTabWidget
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
                        # 过滤调试信息
                        if line and not any(skip in line for skip in ['===', 'OK', '初始化', 'RAW', 'Proc', 'CH']):
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
        """
        解析数据：als,ps,ax,ay,az,gx,gy,gz,imu_temp,
                  t0,t1,t2,t3,t4,t5,t6,t7,
                  h0,h1,h2,h3,h4,h5,h6,h7,
                  p0,p1,p2,p3,p4,p5,p6,p7,
                  mic (单麦克风，参照temp工程)
        """
        try:
            parts = line.split(',')
            if len(parts) != 34:
                return None

            data = {
                'timestamp': time.time(),
                # VCNL4040
                'als': int(parts[0]),
                'ps': int(parts[1]),
                # ICM42688 (高精度浮点数)
                'accel_x': float(parts[2]),  # g (3位小数精度)
                'accel_y': float(parts[3]),
                'accel_z': float(parts[4]),
                'gyro_x': float(parts[5]),   # °/s (2位小数精度)
                'gyro_y': float(parts[6]),
                'gyro_z': float(parts[7]),
                'imu_temp': float(parts[8]), # °C (2位小数精度)
            }
            
            # 8x BME280 温度 (索引9-16)
            for i in range(8):
                data[f'T{i}'] = float(parts[9 + i])
            
            # 8x BME280 湿度 (索引17-24)
            for i in range(8):
                data[f'H{i}'] = float(parts[17 + i])
            
            # 8x BME280 气压 (索引25-32)
            for i in range(8):
                data[f'P{i}'] = float(parts[25 + i])
            
            # 单麦克风 (索引33, temp工程配置: 16kHz采样率)
            data['mic'] = int(parts[33])
            
            return data
            
        except (ValueError, IndexError):
            return None

    def stop(self):
        self.running = False
        self.wait()


class SensorViewer(QMainWindow):
    """主窗口 - 多Tab版本"""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("传感器实时监控 - 完整版 (34通道)")
        self.setGeometry(50, 50, 1800, 1000)

        # 数据缓冲区
        self.max_points = 500
        self.data_buffer = {'timestamp': deque(maxlen=self.max_points)}
        
        # 初始化所有数据缓冲
        for key in ['als', 'ps', 'accel_x', 'accel_y', 'accel_z',
                    'gyro_x', 'gyro_y', 'gyro_z', 'imu_temp',
                    'mic']:
            self.data_buffer[key] = deque(maxlen=self.max_points)
        
        for i in range(8):
            self.data_buffer[f'T{i}'] = deque(maxlen=self.max_points)
            self.data_buffer[f'H{i}'] = deque(maxlen=self.max_points)
            self.data_buffer[f'P{i}'] = deque(maxlen=self.max_points)

        self.serial_reader = None
        self.start_time = time.time()
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
        control_group = QGroupBox("控制面板")
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

        self.clear_btn = QPushButton("🗑️ 清空数据")
        self.clear_btn.clicked.connect(self.clear_data)
        control_layout.addWidget(self.clear_btn)

        self.status_label = QLabel("⚪ 未连接")
        self.status_label.setStyleSheet("font-weight: bold; font-size: 14px;")
        control_layout.addWidget(self.status_label)

        control_layout.addStretch()
        control_group.setLayout(control_layout)
        main_layout.addWidget(control_group)

        # Tab切换
        self.tab_widget = QTabWidget()
        self.tab_widget.addTab(self.create_env_tab(), "🌡️ 环境传感器(BME280)")
        self.tab_widget.addTab(self.create_imu_tab(), "📐 IMU传感器(ICM42688)")
        self.tab_widget.addTab(self.create_optical_tab(), "💡 光学(VCNL4040)")
        self.tab_widget.addTab(self.create_mic_tab(), "🎤 麦克风(ICS-43434)")
        
        main_layout.addWidget(self.tab_widget)

    def create_env_tab(self):
        """创建环境传感器Tab (8x BME280: 温湿压)"""
        widget = QWidget()
        layout = QVBoxLayout()

        # 实时数值显示
        values_group = QGroupBox("实时数值")
        values_layout = QGridLayout()
        
        self.temp_labels = []
        self.hum_labels = []
        self.press_labels = []
        
        for i in range(8):
            col = i
            # 传感器编号
            sensor_label = QLabel(f"S{i}")
            sensor_label.setStyleSheet("font-weight: bold; color: #7f8c8d;")
            values_layout.addWidget(sensor_label, 0, col)
            
            # 温度
            temp_label = QLabel("--°C")
            temp_label.setStyleSheet("font-weight: bold; color: #e74c3c; font-size: 14px;")
            values_layout.addWidget(temp_label, 1, col)
            self.temp_labels.append(temp_label)
            
            # 湿度
            hum_label = QLabel("--%")
            hum_label.setStyleSheet("font-weight: bold; color: #3498db; font-size: 14px;")
            values_layout.addWidget(hum_label, 2, col)
            self.hum_labels.append(hum_label)
            
            # 气压
            press_label = QLabel("-- hPa")
            press_label.setStyleSheet("font-weight: bold; color: #2ecc71; font-size: 12px;")
            values_layout.addWidget(press_label, 3, col)
            self.press_labels.append(press_label)
        
        values_group.setLayout(values_layout)
        layout.addWidget(values_group)

        # 温度曲线
        self.temp_plot = pg.PlotWidget(title="温度 (°C)")
        self.temp_plot.setLabel('left', 'T(°C)')
        self.temp_plot.setLabel('bottom', '时间(s)')
        self.temp_plot.showGrid(x=True, y=True, alpha=0.3)
        self.temp_plot.addLegend()
        
        colors = ['#e74c3c', '#3498db', '#2ecc71', '#f39c12', '#9b59b6', '#1abc9c', '#34495e', '#e67e22']
        self.temp_curves = []
        for i in range(8):
            curve = self.temp_plot.plot(pen=pg.mkPen(color=colors[i], width=2), name=f'S{i}')
            self.temp_curves.append(curve)
        
        layout.addWidget(self.temp_plot)

        # 湿度曲线
        self.hum_plot = pg.PlotWidget(title="湿度 (%)")
        self.hum_plot.setLabel('left', 'RH(%)')
        self.hum_plot.setLabel('bottom', '时间(s)')
        self.hum_plot.showGrid(x=True, y=True, alpha=0.3)
        self.hum_plot.addLegend()
        
        self.hum_curves = []
        for i in range(8):
            curve = self.hum_plot.plot(pen=pg.mkPen(color=colors[i], width=2), name=f'S{i}')
            self.hum_curves.append(curve)
        
        layout.addWidget(self.hum_plot)

        # 气压曲线
        self.press_plot = pg.PlotWidget(title="气压 (hPa)")
        self.press_plot.setLabel('left', 'P(hPa)')
        self.press_plot.setLabel('bottom', '时间(s)')
        self.press_plot.showGrid(x=True, y=True, alpha=0.3)
        self.press_plot.addLegend()
        
        self.press_curves = []
        for i in range(8):
            curve = self.press_plot.plot(pen=pg.mkPen(color=colors[i], width=2), name=f'S{i}')
            self.press_curves.append(curve)
        
        layout.addWidget(self.press_plot)

        widget.setLayout(layout)
        return widget

    def create_imu_tab(self):
        """创建IMU传感器Tab (ICM42688)"""
        widget = QWidget()
        layout = QVBoxLayout()

        # IMU实时数值
        values_group = QGroupBox("实时数值")
        values_layout = QHBoxLayout()
        
        self.accel_x_label = QLabel("AccX: --")
        self.accel_y_label = QLabel("AccY: --")
        self.accel_z_label = QLabel("AccZ: --")
        self.gyro_x_label = QLabel("GyroX: --")
        self.gyro_y_label = QLabel("GyroY: --")
        self.gyro_z_label = QLabel("GyroZ: --")
        self.imu_temp_label = QLabel("Temp: --")
        
        for label in [self.accel_x_label, self.accel_y_label, self.accel_z_label,
                      self.gyro_x_label, self.gyro_y_label, self.gyro_z_label, self.imu_temp_label]:
            label.setStyleSheet("font-weight: bold; font-size: 13px; padding: 5px;")
            values_layout.addWidget(label)
        
        values_layout.addStretch()
        values_group.setLayout(values_layout)
        layout.addWidget(values_group)

        # 加速度计
        self.accel_plot = pg.PlotWidget(title="加速度计 (g)")
        self.accel_plot.setLabel('left', '加速度(g)')
        self.accel_plot.setLabel('bottom', '时间(s)')
        self.accel_plot.showGrid(x=True, y=True, alpha=0.3)
        self.accel_plot.addLegend()
        self.accel_x_curve = self.accel_plot.plot(pen=pg.mkPen(color='r', width=2), name='X')
        self.accel_y_curve = self.accel_plot.plot(pen=pg.mkPen(color='g', width=2), name='Y')
        self.accel_z_curve = self.accel_plot.plot(pen=pg.mkPen(color='b', width=2), name='Z')
        layout.addWidget(self.accel_plot)

        # 陀螺仪
        self.gyro_plot = pg.PlotWidget(title="陀螺仪 (°/s)")
        self.gyro_plot.setLabel('left', '角速度(°/s)')
        self.gyro_plot.setLabel('bottom', '时间(s)')
        self.gyro_plot.showGrid(x=True, y=True, alpha=0.3)
        self.gyro_plot.addLegend()
        self.gyro_x_curve = self.gyro_plot.plot(pen=pg.mkPen(color='r', width=2), name='X')
        self.gyro_y_curve = self.gyro_plot.plot(pen=pg.mkPen(color='g', width=2), name='Y')
        self.gyro_z_curve = self.gyro_plot.plot(pen=pg.mkPen(color='b', width=2), name='Z')
        layout.addWidget(self.gyro_plot)

        # IMU温度
        self.imu_temp_plot = pg.PlotWidget(title="IMU温度 (°C)")
        self.imu_temp_plot.setLabel('left', '温度(°C)')
        self.imu_temp_plot.setLabel('bottom', '时间(s)')
        self.imu_temp_plot.showGrid(x=True, y=True, alpha=0.3)
        self.imu_temp_curve = self.imu_temp_plot.plot(pen=pg.mkPen(color='r', width=2))
        layout.addWidget(self.imu_temp_plot)

        widget.setLayout(layout)
        return widget

    def create_optical_tab(self):
        """创建光学传感器Tab (VCNL4040)"""
        widget = QWidget()
        layout = QVBoxLayout()

        # 实时数值
        values_group = QGroupBox("实时数值")
        values_layout = QHBoxLayout()
        
        self.als_label = QLabel("环境光: -- lux")
        self.ps_label = QLabel("接近度: --")
        
        for label in [self.als_label, self.ps_label]:
            label.setStyleSheet("font-weight: bold; font-size: 14px; padding: 5px;")
            values_layout.addWidget(label)
        
        values_layout.addStretch()
        values_group.setLayout(values_layout)
        layout.addWidget(values_group)

        # 环境光强度
        self.als_plot = pg.PlotWidget(title="环境光强度")
        self.als_plot.setLabel('left', '光强')
        self.als_plot.setLabel('bottom', '时间(s)')
        self.als_plot.showGrid(x=True, y=True, alpha=0.3)
        self.als_curve = self.als_plot.plot(pen=pg.mkPen(color='y', width=2))
        layout.addWidget(self.als_plot)

        # 接近传感器
        self.ps_plot = pg.PlotWidget(title="接近传感器")
        self.ps_plot.setLabel('left', '接近值')
        self.ps_plot.setLabel('bottom', '时间(s)')
        self.ps_plot.showGrid(x=True, y=True, alpha=0.3)
        self.ps_curve = self.ps_plot.plot(pen=pg.mkPen(color='m', width=2))
        layout.addWidget(self.ps_plot)

        widget.setLayout(layout)
        return widget

    def create_mic_tab(self):
        """创建麦克风Tab (ICS-43434 单麦克风 - temp工程配置)"""
        widget = QWidget()
        layout = QVBoxLayout()

        # 实时数值
        values_group = QGroupBox("实时数值 (16kHz采样率)")
        values_layout = QHBoxLayout()

        self.mic_label = QLabel("音频值: --")
        self.mic_label.setStyleSheet("font-weight: bold; font-size: 14px; padding: 5px;")
        values_layout.addWidget(self.mic_label)

        # 显示配置信息
        config_label = QLabel("配置: 单麦克风 | 16kHz | 24-bit | Buffer: 4字")
        config_label.setStyleSheet("color: #7f8c8d; font-size: 11px;")
        values_layout.addWidget(config_label)

        values_layout.addStretch()
        values_group.setLayout(values_layout)
        layout.addWidget(values_group)

        # 麦克风波形
        self.mic_plot = pg.PlotWidget(title="麦克风波形 (audio_result)")
        self.mic_plot.setLabel('left', '音频值 (24-bit)')
        self.mic_plot.setLabel('bottom', '时间(s)')
        self.mic_plot.showGrid(x=True, y=True, alpha=0.3)
        self.mic_curve = self.mic_plot.plot(pen=pg.mkPen(color='c', width=2))
        layout.addWidget(self.mic_plot)

        widget.setLayout(layout)
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

    def clear_data(self):
        """清空所有数据"""
        for key in self.data_buffer:
            self.data_buffer[key].clear()
        self.start_time = time.time()

    def update_plots(self):
        """更新所有图表"""
        if len(self.data_buffer['timestamp']) < 2:
            return

        timestamps = np.array(self.data_buffer['timestamp'])
        
        # 更新环境传感器 (BME280)
        for i in range(8):
            if len(self.data_buffer[f'T{i}']) > 0:
                temps = np.array(self.data_buffer[f'T{i}'])
                hums = np.array(self.data_buffer[f'H{i}'])
                press = np.array(self.data_buffer[f'P{i}'])
                
                self.temp_curves[i].setData(timestamps, temps)
                self.hum_curves[i].setData(timestamps, hums)
                self.press_curves[i].setData(timestamps, press)
                
                self.temp_labels[i].setText(f"{temps[-1]:.1f}°C")
                self.hum_labels[i].setText(f"{hums[-1]:.1f}%")
                self.press_labels[i].setText(f"{press[-1]:.1f} hPa")
        
        # 更新IMU
        if len(self.data_buffer['accel_x']) > 0:
            self.accel_x_curve.setData(timestamps, np.array(self.data_buffer['accel_x']))
            self.accel_y_curve.setData(timestamps, np.array(self.data_buffer['accel_y']))
            self.accel_z_curve.setData(timestamps, np.array(self.data_buffer['accel_z']))
            
            self.gyro_x_curve.setData(timestamps, np.array(self.data_buffer['gyro_x']))
            self.gyro_y_curve.setData(timestamps, np.array(self.data_buffer['gyro_y']))
            self.gyro_z_curve.setData(timestamps, np.array(self.data_buffer['gyro_z']))
            
            self.imu_temp_curve.setData(timestamps, np.array(self.data_buffer['imu_temp']))
            
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
        
        # 更新麦克风
        if len(self.data_buffer['mic']) > 0:
            self.mic_curve.setData(timestamps, np.array(self.data_buffer['mic']))
            self.mic_label.setText(f"音频值: {self.data_buffer['mic'][-1]}")

    def closeEvent(self, event):
        """关闭窗口时的清理工作"""
        if self.serial_reader and self.serial_reader.running:
            self.serial_reader.stop()
        event.accept()


def main():
    app = QApplication(sys.argv)
    window = SensorViewer()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()

