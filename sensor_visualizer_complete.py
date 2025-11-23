#!/usr/bin/env python3
"""
完整传感器实时可视化
支持所有传感器：VCNL4040, ICM42688, 8×BME280(温湿压), 双麦克风

数据格式(35字段):
als,ps,                                          # VCNL4040: 环境光,接近度
ax,ay,az,gx,gy,gz,imu_temp,                     # ICM42688: 加速度×3,角速度×3,温度
t0,h0,t1,h1,t2,h2,t3,h3,t4,h4,t5,h5,t6,h6,t7,h7, # BME280×8: 温湿度交替
p0,p1,p2,p3,p4,p5,p6,p7,                        # BME280×8: 气压
mic_l,mic_r                                      # 双麦克风: 左右声道

注意：温湿度数据已×10，压力为整数hPa
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
    QPushButton, QComboBox, QLabel, QMessageBox, QTabWidget, QGridLayout
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
                        if line and not line.startswith('===') and not line.startswith('初始化'):
                            data = self.parse_data(line)
                            if data:
                                self.data_received.emit(data)
                    except (UnicodeDecodeError, Exception):
                        pass
                time.sleep(0.001)  # 1ms

        except serial.SerialException as e:
            self.error_occurred.emit(f"串口错误: {str(e)}")
        finally:
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()

    def parse_data(self, line):
        """解析CSV数据行"""
        try:
            parts = line.split(',')
            if len(parts) != 35:  # 期望35个字段 (双麦克风)
                return None

            data = {
                # VCNL4040 (2)
                'als': int(parts[0]),
                'ps': int(parts[1]),
                # ICM42688 (7)
                'accel_x': int(parts[2]) / 100.0,
                'accel_y': int(parts[3]) / 100.0,
                'accel_z': int(parts[4]) / 100.0,
                'gyro_x': int(parts[5]) / 10.0,
                'gyro_y': int(parts[6]) / 10.0,
                'gyro_z': int(parts[7]) / 10.0,
                'imu_temp': int(parts[8]) / 10.0,
                # BME280 温湿度交替排列: temp0,hum0,temp1,hum1,...temp7,hum7
                'temp': [int(parts[9 + i*2]) / 10.0 for i in range(8)],      # 9,11,13,15,17,19,21,23
                'humidity': [int(parts[10 + i*2]) / 10.0 for i in range(8)], # 10,12,14,16,18,20,22,24
                # BME280 压力 (8) - 数据已经是整数hPa
                'pressure': [int(parts[25+i]) for i in range(8)],  # 25-32
                # 麦克风 (2) - 立体声
                'mic_left': int(parts[33]),
                'mic_right': int(parts[34]),
            }
            return data
        except (ValueError, IndexError):
            return None

    def stop(self):
        """停止线程"""
        self.running = False


class SensorVisualizer(QMainWindow):
    """主窗口"""
    def __init__(self):
        super().__init__()
        self.setWindowTitle("完整传感器可视化 - 35通道实时监控 (双麦克风)")
        self.setGeometry(100, 100, 1400, 900)

        # 数据缓冲区
        self.max_points = 500
        self.init_data_buffers()

        # 串口
        self.serial_reader = None

        # 设置UI
        self.setup_ui()

        # 更新定时器
        self.update_timer = QTimer()
        self.update_timer.timeout.connect(self.update_plots)
        self.update_timer.start(50)  # 20Hz刷新

    def init_data_buffers(self):
        """初始化数据缓冲区"""
        self.time_data = deque(maxlen=self.max_points)
        self.start_time = time.time()
        
        # VCNL4040
        self.als_data = deque(maxlen=self.max_points)
        self.ps_data = deque(maxlen=self.max_points)
        
        # ICM42688
        self.accel_x = deque(maxlen=self.max_points)
        self.accel_y = deque(maxlen=self.max_points)
        self.accel_z = deque(maxlen=self.max_points)
        self.gyro_x = deque(maxlen=self.max_points)
        self.gyro_y = deque(maxlen=self.max_points)
        self.gyro_z = deque(maxlen=self.max_points)
        
        # BME280 (8个传感器)
        self.temp_data = [deque(maxlen=self.max_points) for _ in range(8)]
        self.hum_data = [deque(maxlen=self.max_points) for _ in range(8)]
        self.press_data = [deque(maxlen=self.max_points) for _ in range(8)]
        
        # 麦克风 (立体声)
        self.mic_left = deque(maxlen=self.max_points)
        self.mic_right = deque(maxlen=self.max_points)

    def setup_ui(self):
        """设置用户界面"""
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)

        # 控制面板
        control_panel = self.create_control_panel()
        layout.addWidget(control_panel)

        # 标签页
        self.tabs = QTabWidget()
        
        # Tab 1: 光学+IMU
        tab1 = self.create_optical_imu_tab()
        self.tabs.addTab(tab1, "光学+IMU")
        
        # Tab 2: 温度
        tab2 = self.create_temperature_tab()
        self.tabs.addTab(tab2, "温度×8")
        
        # Tab 3: 湿度
        tab3 = self.create_humidity_tab()
        self.tabs.addTab(tab3, "湿度×8")
        
        # Tab 4: 压力
        tab4 = self.create_pressure_tab()
        self.tabs.addTab(tab4, "压力×8")
        
        # Tab 5: 麦克风
        tab5 = self.create_microphone_tab()
        self.tabs.addTab(tab5, "麦克风")

        layout.addWidget(self.tabs)

    def create_control_panel(self):
        """创建控制面板"""
        panel = QWidget()
        layout = QHBoxLayout(panel)

        # 串口选择
        layout.addWidget(QLabel("串口:"))
        self.port_combo = QComboBox()
        self.refresh_ports()
        layout.addWidget(self.port_combo)

        # 刷新按钮
        refresh_btn = QPushButton("刷新")
        refresh_btn.clicked.connect(self.refresh_ports)
        layout.addWidget(refresh_btn)

        # 连接/断开按钮
        self.connect_btn = QPushButton("连接")
        self.connect_btn.clicked.connect(self.toggle_connection)
        layout.addWidget(self.connect_btn)

        # 状态标签
        self.status_label = QLabel("未连接")
        layout.addWidget(self.status_label)

        layout.addStretch()
        return panel

    def create_optical_imu_tab(self):
        """创建光学+IMU标签页"""
        widget = QWidget()
        layout = QGridLayout(widget)

        # VCNL4040 图表
        self.als_plot = pg.PlotWidget(title="环境光 (ALS)")
        self.als_plot.setLabel('left', 'Lux')
        self.als_plot.setLabel('bottom', 'Time', 's')
        self.als_curve = self.als_plot.plot(pen='y')
        layout.addWidget(self.als_plot, 0, 0)

        self.ps_plot = pg.PlotWidget(title="接近度 (PS)")
        self.ps_plot.setLabel('left', 'Value')
        self.ps_plot.setLabel('bottom', 'Time', 's')
        self.ps_curve = self.ps_plot.plot(pen='m')
        layout.addWidget(self.ps_plot, 0, 1)

        # 加速度
        self.accel_plot = pg.PlotWidget(title="加速度 (m/s²)")
        self.accel_plot.setLabel('left', 'Acceleration', 'm/s²')
        self.accel_plot.setLabel('bottom', 'Time', 's')
        self.accel_plot.addLegend()
        self.accel_x_curve = self.accel_plot.plot(pen='r', name='X')
        self.accel_y_curve = self.accel_plot.plot(pen='g', name='Y')
        self.accel_z_curve = self.accel_plot.plot(pen='b', name='Z')
        layout.addWidget(self.accel_plot, 1, 0)

        # 陀螺仪
        self.gyro_plot = pg.PlotWidget(title="陀螺仪 (°/s)")
        self.gyro_plot.setLabel('left', 'Angular Velocity', '°/s')
        self.gyro_plot.setLabel('bottom', 'Time', 's')
        self.gyro_plot.addLegend()
        self.gyro_x_curve = self.gyro_plot.plot(pen='r', name='X')
        self.gyro_y_curve = self.gyro_plot.plot(pen='g', name='Y')
        self.gyro_z_curve = self.gyro_plot.plot(pen='b', name='Z')
        layout.addWidget(self.gyro_plot, 1, 1)

        return widget

    def create_temperature_tab(self):
        """创建温度标签页"""
        widget = QWidget()
        layout = QGridLayout(widget)

        self.temp_plots = []
        self.temp_curves = []
        colors = ['r', 'g', 'b', 'c', 'm', 'y', 'w', (255, 128, 0)]

        for i in range(8):
            plot = pg.PlotWidget(title=f"温度传感器 {i+1}")
            plot.setLabel('left', '温度', '°C')
            plot.setLabel('bottom', 'Time', 's')
            curve = plot.plot(pen=colors[i])
            self.temp_plots.append(plot)
            self.temp_curves.append(curve)
            layout.addWidget(plot, i // 4, i % 4)

        return widget

    def create_humidity_tab(self):
        """创建湿度标签页"""
        widget = QWidget()
        layout = QGridLayout(widget)

        self.hum_plots = []
        self.hum_curves = []
        colors = ['r', 'g', 'b', 'c', 'm', 'y', 'w', (255, 128, 0)]

        for i in range(8):
            plot = pg.PlotWidget(title=f"湿度传感器 {i+1}")
            plot.setLabel('left', '湿度', '%')
            plot.setLabel('bottom', 'Time', 's')
            curve = plot.plot(pen=colors[i])
            self.hum_plots.append(plot)
            self.hum_curves.append(curve)
            layout.addWidget(plot, i // 4, i % 4)

        return widget

    def create_pressure_tab(self):
        """创建压力标签页"""
        widget = QWidget()
        layout = QGridLayout(widget)

        self.press_plots = []
        self.press_curves = []
        colors = ['r', 'g', 'b', 'c', 'm', 'y', 'w', (255, 128, 0)]

        for i in range(8):
            plot = pg.PlotWidget(title=f"压力传感器 {i+1}")
            plot.setLabel('left', '压力', 'hPa')
            plot.setLabel('bottom', 'Time', 's')
            curve = plot.plot(pen=colors[i])
            self.press_plots.append(plot)
            self.press_curves.append(curve)
            layout.addWidget(plot, i // 4, i % 4)

        return widget

    def create_microphone_tab(self):
        """创建麦克风标签页"""
        widget = QWidget()
        layout = QVBoxLayout(widget)

        # 左声道
        self.mic_left_plot = pg.PlotWidget(title="麦克风 - 左声道")
        self.mic_left_plot.setLabel('left', 'Amplitude')
        self.mic_left_plot.setLabel('bottom', 'Time', 's')
        self.mic_left_curve = self.mic_left_plot.plot(pen='c')
        layout.addWidget(self.mic_left_plot)

        # 右声道
        self.mic_right_plot = pg.PlotWidget(title="麦克风 - 右声道")
        self.mic_right_plot.setLabel('left', 'Amplitude')
        self.mic_right_plot.setLabel('bottom', 'Time', 's')
        self.mic_right_curve = self.mic_right_plot.plot(pen='m')
        layout.addWidget(self.mic_right_plot)

        return widget

    def refresh_ports(self):
        """刷新串口列表"""
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_combo.addItem(f"{port.device} - {port.description}")

    def toggle_connection(self):
        """切换连接状态"""
        if self.serial_reader is None or not self.serial_reader.running:
            self.connect_serial()
        else:
            self.disconnect_serial()

    def connect_serial(self):
        """连接串口"""
        port_text = self.port_combo.currentText()
        if not port_text:
            QMessageBox.warning(self, "错误", "请选择串口")
            return

        port = port_text.split(' - ')[0]
        self.serial_reader = SerialReader(port)
        self.serial_reader.data_received.connect(self.on_data_received)
        self.serial_reader.error_occurred.connect(self.on_error)
        self.serial_reader.start()

        self.connect_btn.setText("断开")
        self.status_label.setText(f"已连接: {port}")

    def disconnect_serial(self):
        """断开串口"""
        if self.serial_reader:
            self.serial_reader.stop()
            self.serial_reader.wait()
            self.serial_reader = None

        self.connect_btn.setText("连接")
        self.status_label.setText("未连接")

    def on_data_received(self, data):
        """接收到数据"""
        current_time = time.time() - self.start_time
        self.time_data.append(current_time)

        # VCNL4040
        self.als_data.append(data['als'])
        self.ps_data.append(data['ps'])

        # ICM42688
        self.accel_x.append(data['accel_x'])
        self.accel_y.append(data['accel_y'])
        self.accel_z.append(data['accel_z'])
        self.gyro_x.append(data['gyro_x'])
        self.gyro_y.append(data['gyro_y'])
        self.gyro_z.append(data['gyro_z'])

        # BME280
        for i in range(8):
            self.temp_data[i].append(data['temp'][i])
            self.hum_data[i].append(data['humidity'][i])
            self.press_data[i].append(data['pressure'][i])

        # 麦克风 (立体声)
        self.mic_left.append(data['mic_left'])
        self.mic_right.append(data['mic_right'])

    def on_error(self, error_msg):
        """错误处理"""
        QMessageBox.critical(self, "错误", error_msg)
        self.disconnect_serial()

    def update_plots(self):
        """更新所有图表"""
        if len(self.time_data) == 0:
            return

        t = np.array(self.time_data)

        # VCNL4040
        self.als_curve.setData(t, np.array(self.als_data))
        self.ps_curve.setData(t, np.array(self.ps_data))

        # 加速度
        self.accel_x_curve.setData(t, np.array(self.accel_x))
        self.accel_y_curve.setData(t, np.array(self.accel_y))
        self.accel_z_curve.setData(t, np.array(self.accel_z))

        # 陀螺仪
        self.gyro_x_curve.setData(t, np.array(self.gyro_x))
        self.gyro_y_curve.setData(t, np.array(self.gyro_y))
        self.gyro_z_curve.setData(t, np.array(self.gyro_z))

        # BME280
        for i in range(8):
            self.temp_curves[i].setData(t, np.array(self.temp_data[i]))
            self.hum_curves[i].setData(t, np.array(self.hum_data[i]))
            self.press_curves[i].setData(t, np.array(self.press_data[i]))

        # 麦克风 (立体声)
        self.mic_left_curve.setData(t, np.array(self.mic_left))
        self.mic_right_curve.setData(t, np.array(self.mic_right))

    def closeEvent(self, event):
        """关闭窗口"""
        self.disconnect_serial()
        event.accept()


def main():
    app = QApplication(sys.argv)
    window = SensorVisualizer()
    window.show()
    sys.exit(app.exec())


if __name__ == '__main__':
    main()

