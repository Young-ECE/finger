#!/usr/bin/env python3
"""
传感器数据实时可视化界面
支持从USB CDC读取传感器数据并实时绘制
更新: 支持ICM42688 IMU + 8xBME280 + 双麦克风
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
    QGridLayout, QTabWidget
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
                        if line and not line.startswith('===') and not line.startswith('CH') and not line.startswith('PROFILE'):
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
        格式: AQI,TVOC,ECO2,ALS,PS,AccelX,AccelY,AccelZ,GyroX,GyroY,GyroZ,IMU_Temp,
              T0,H0,P0,T1,H1,P1,T2,H2,P2,T3,H3,P3,T4,H4,P4,T5,H5,P5,T6,H6,P6,T7,H7,P7,MicL,MicR
        """
        try:
            parts = line.split(',')
            if len(parts) != 38:
                return None

            return {
                'timestamp': time.time(),
                # ENS160 (已移除，数据为0)
                'aqi': int(parts[0]),
                'tvoc': int(parts[1]),
                'eco2': int(parts[2]),
                # VCNL4040
                'als': int(parts[3]),
                'ps': int(parts[4]),
                # ICM42688 IMU
                'accel_x': int(parts[5]) / 100.0,  # g
                'accel_y': int(parts[6]) / 100.0,
                'accel_z': int(parts[7]) / 100.0,
                'gyro_x': int(parts[8]) / 10.0,    # °/s
                'gyro_y': int(parts[9]) / 10.0,
                'gyro_z': int(parts[10]) / 10.0,
                'imu_temp': int(parts[11]) / 10.0, # °C
                # BME280 x8 (温度/湿度/气压)
                'T0': int(parts[12]) / 10.0,
                'H0': int(parts[13]) / 10.0,
                'P0': int(parts[14]) / 10.0,
                'T1': int(parts[15]) / 10.0,
                'H1': int(parts[16]) / 10.0,
                'P1': int(parts[17]) / 10.0,
                'T2': int(parts[18]) / 10.0,
                'H2': int(parts[19]) / 10.0,
                'P2': int(parts[20]) / 10.0,
                'T3': int(parts[21]) / 10.0,
                'H3': int(parts[22]) / 10.0,
                'P3': int(parts[23]) / 10.0,
                'T4': int(parts[24]) / 10.0,
                'H4': int(parts[25]) / 10.0,
                'P4': int(parts[26]) / 10.0,
                'T5': int(parts[27]) / 10.0,
                'H5': int(parts[28]) / 10.0,
                'P5': int(parts[29]) / 10.0,
                'T6': int(parts[30]) / 10.0,
                'H6': int(parts[31]) / 10.0,
                'P6': int(parts[32]) / 10.0,
                'T7': int(parts[33]) / 10.0,
                'H7': int(parts[34]) / 10.0,
                'P7': int(parts[35]) / 10.0,
                # 双麦克风
                'mic_left': int(parts[36]),
                'mic_right': int(parts[37])
            }
        except (ValueError, IndexError) as e:
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
            'accel_x': deque(maxlen=max_points),
            'accel_y': deque(maxlen=max_points),
            'accel_z': deque(maxlen=max_points),
            'gyro_x': deque(maxlen=max_points),
            'gyro_y': deque(maxlen=max_points),
            'gyro_z': deque(maxlen=max_points),
            'imu_temp': deque(maxlen=max_points),
        }
        # 添加8组BME280传感器
        for i in range(8):
            self.data[f'T{i}'] = deque(maxlen=max_points)
            self.data[f'H{i}'] = deque(maxlen=max_points)
            self.data[f'P{i}'] = deque(maxlen=max_points)
        # 双麦克风
        self.data['mic_left'] = deque(maxlen=max_points)
        self.data['mic_right'] = deque(maxlen=max_points)

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
            headers = ['timestamp', 'als', 'ps',
                      'accel_x', 'accel_y', 'accel_z',
                      'gyro_x', 'gyro_y', 'gyro_z', 'imu_temp']
            for i in range(8):
                headers.extend([f'T{i}', f'H{i}', f'P{i}'])
            headers.extend(['mic_left', 'mic_right'])
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
        self.setWindowTitle('传感器数据实时监控 - Finger项目')
        self.setGeometry(100, 100, 1600, 1000)

        # 中心部件
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)

        # 控制面板
        control_panel = self.create_control_panel()
        main_layout.addWidget(control_panel)

        # 使用Tab切换不同传感器页面
        self.tab_widget = QTabWidget()

        # Tab 1: 环境传感器 (温湿度气压)
        env_tab = self.create_env_tab()
        self.tab_widget.addTab(env_tab, "环境传感器")

        # Tab 2: IMU传感器
        imu_tab = self.create_imu_tab()
        self.tab_widget.addTab(imu_tab, "IMU传感器")

        # Tab 3: 光学+音频
        optical_tab = self.create_optical_tab()
        self.tab_widget.addTab(optical_tab, "光学+音频")

        main_layout.addWidget(self.tab_widget)

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

    def create_env_tab(self):
        """创建环境传感器Tab (8x BME280)"""
        widget = QWidget()
        layout = QGridLayout()

        # 设置pyqtgraph样式
        pg.setConfigOption('background', 'w')
        pg.setConfigOption('foreground', 'k')

        # 温度图表 (CH1-CH7，跳过CH0)
        self.temp_plot = pg.PlotWidget(title="温度传感器 (BME280 CH1-7)")
        self.temp_plot.setLabel('left', '温度', units='°C')
        self.temp_plot.setLabel('bottom', '时间', units='s')
        self.temp_plot.addLegend()

        colors = ['r', 'g', 'b', 'c', 'm', 'y', 'w']
        self.temp_curves = []
        for i in range(1, 8):  # CH1-CH7
            curve = self.temp_plot.plot(pen=pg.mkPen(color=colors[i-1], width=2), name=f'CH{i}')
            self.temp_curves.append(curve)

        # 湿度图表
        self.humid_plot = pg.PlotWidget(title="湿度传感器 (BME280 CH1-7)")
        self.humid_plot.setLabel('left', '湿度', units='%')
        self.humid_plot.setLabel('bottom', '时间', units='s')
        self.humid_plot.addLegend()

        self.humid_curves = []
        for i in range(1, 8):
            curve = self.humid_plot.plot(pen=pg.mkPen(color=colors[i-1], width=2), name=f'CH{i}')
            self.humid_curves.append(curve)

        # 气压图表
        self.press_plot = pg.PlotWidget(title="气压传感器 (BME280 CH1-7)")
        self.press_plot.setLabel('left', '气压', units='hPa')
        self.press_plot.setLabel('bottom', '时间', units='s')
        self.press_plot.addLegend()

        self.press_curves = []
        for i in range(1, 8):
            curve = self.press_plot.plot(pen=pg.mkPen(color=colors[i-1], width=2), name=f'CH{i}')
            self.press_curves.append(curve)

        layout.addWidget(self.temp_plot, 0, 0, 1, 2)
        layout.addWidget(self.humid_plot, 1, 0)
        layout.addWidget(self.press_plot, 1, 1)

        widget.setLayout(layout)
        return widget

    def create_imu_tab(self):
        """创建IMU传感器Tab (ICM42688)"""
        widget = QWidget()
        layout = QGridLayout()

        # 加速度计
        self.accel_plot = pg.PlotWidget(title="加速度计 (ICM42688)")
        self.accel_plot.setLabel('left', '加速度', units='g')
        self.accel_plot.setLabel('bottom', '时间', units='s')
        self.accel_plot.addLegend()
        self.accel_x_curve = self.accel_plot.plot(pen=pg.mkPen(color='r', width=2), name='X')
        self.accel_y_curve = self.accel_plot.plot(pen=pg.mkPen(color='g', width=2), name='Y')
        self.accel_z_curve = self.accel_plot.plot(pen=pg.mkPen(color='b', width=2), name='Z')

        # 陀螺仪
        self.gyro_plot = pg.PlotWidget(title="陀螺仪 (ICM42688)")
        self.gyro_plot.setLabel('left', '角速度', units='°/s')
        self.gyro_plot.setLabel('bottom', '时间', units='s')
        self.gyro_plot.addLegend()
        self.gyro_x_curve = self.gyro_plot.plot(pen=pg.mkPen(color='r', width=2), name='X')
        self.gyro_y_curve = self.gyro_plot.plot(pen=pg.mkPen(color='g', width=2), name='Y')
        self.gyro_z_curve = self.gyro_plot.plot(pen=pg.mkPen(color='b', width=2), name='Z')

        # IMU温度
        self.imu_temp_plot = pg.PlotWidget(title="IMU温度")
        self.imu_temp_plot.setLabel('left', '温度', units='°C')
        self.imu_temp_plot.setLabel('bottom', '时间', units='s')
        self.imu_temp_curve = self.imu_temp_plot.plot(pen=pg.mkPen(color='r', width=2))

        layout.addWidget(self.accel_plot, 0, 0)
        layout.addWidget(self.gyro_plot, 0, 1)
        layout.addWidget(self.imu_temp_plot, 1, 0, 1, 2)

        widget.setLayout(layout)
        return widget

    def create_optical_tab(self):
        """创建光学和音频传感器Tab"""
        widget = QWidget()
        layout = QGridLayout()

        # 环境光强度
        self.als_plot = pg.PlotWidget(title="环境光强度 (VCNL4040)")
        self.als_plot.setLabel('left', '光强', units='')
        self.als_plot.setLabel('bottom', '时间', units='s')
        self.als_curve = self.als_plot.plot(pen=pg.mkPen(color='y', width=2))

        # 接近传感器
        self.ps_plot = pg.PlotWidget(title="接近传感器 (VCNL4040)")
        self.ps_plot.setLabel('left', '接近值', units='')
        self.ps_plot.setLabel('bottom', '时间', units='s')
        self.ps_curve = self.ps_plot.plot(pen=pg.mkPen(color='m', width=2))

        # 左声道麦克风
        self.mic_left_plot = pg.PlotWidget(title="麦克风 - 左声道")
        self.mic_left_plot.setLabel('left', '音频值', units='')
        self.mic_left_plot.setLabel('bottom', '时间', units='s')
        self.mic_left_curve = self.mic_left_plot.plot(pen=pg.mkPen(color='c', width=2))

        # 右声道麦克风
        self.mic_right_plot = pg.PlotWidget(title="麦克风 - 右声道")
        self.mic_right_plot.setLabel('left', '音频值', units='')
        self.mic_right_plot.setLabel('bottom', '时间', units='s')
        self.mic_right_curve = self.mic_right_plot.plot(pen=pg.mkPen(color='b', width=2))

        layout.addWidget(self.als_plot, 0, 0)
        layout.addWidget(self.ps_plot, 0, 1)
        layout.addWidget(self.mic_left_plot, 1, 0)
        layout.addWidget(self.mic_right_plot, 1, 1)

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

        # 更新环境传感器 (BME280 CH1-7)
        for i in range(1, 8):
            self.temp_curves[i-1].setData(time_data, self.data_buffer.get_array(f'T{i}'))
            self.humid_curves[i-1].setData(time_data, self.data_buffer.get_array(f'H{i}'))
            self.press_curves[i-1].setData(time_data, self.data_buffer.get_array(f'P{i}'))

        # 更新IMU
        self.accel_x_curve.setData(time_data, self.data_buffer.get_array('accel_x'))
        self.accel_y_curve.setData(time_data, self.data_buffer.get_array('accel_y'))
        self.accel_z_curve.setData(time_data, self.data_buffer.get_array('accel_z'))
        self.gyro_x_curve.setData(time_data, self.data_buffer.get_array('gyro_x'))
        self.gyro_y_curve.setData(time_data, self.data_buffer.get_array('gyro_y'))
        self.gyro_z_curve.setData(time_data, self.data_buffer.get_array('gyro_z'))
        self.imu_temp_curve.setData(time_data, self.data_buffer.get_array('imu_temp'))

        # 更新光学传感器
        self.als_curve.setData(time_data, self.data_buffer.get_array('als'))
        self.ps_curve.setData(time_data, self.data_buffer.get_array('ps'))

        # 更新双麦克风
        self.mic_left_curve.setData(time_data, self.data_buffer.get_array('mic_left'))
        self.mic_right_curve.setData(time_data, self.data_buffer.get_array('mic_right'))

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
