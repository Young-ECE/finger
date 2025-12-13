#!/usr/bin/env python3
"""
性能分析脚本 - 解析传感器读取耗时
"""
import serial
import time
import re

def analyze_performance(port='/dev/ttyACM0', baudrate=115200, duration=15):
    """
    分析传感器性能

    Args:
        port: 串口设备路径
        baudrate: 波特率
        duration: 测试持续时间（秒）
    """
    print(f"正在连接到 {port}...")
    print("=" * 80)

    profile_data = {
        'ENS': [],
        'VCNL': [],
        'HDC1': [],
        'HDC2': [],
        'HDC3': [],
        'HDC4': [],
        'USB': [],
        'TOTAL': []
    }

    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"已连接! 正在收集性能数据 {duration} 秒...")
        print("等待性能分析数据...\n")

        start_time = time.time()
        data_count = 0
        profile_count = 0

        while time.time() - start_time < duration:
            line = ser.readline()
            if line:
                try:
                    decoded = line.decode('utf-8').strip()

                    # 检查是否是性能分析数据
                    if decoded.startswith('PROFILE:'):
                        profile_count += 1
                        print(f"\n[{profile_count}] {decoded}")

                        # 解析性能数据: PROFILE: ENS=123 VCNL=45 HDC1=67 ...
                        match = re.search(
                            r'ENS=(\d+)\s+VCNL=(\d+)\s+HDC1=(\d+)\s+HDC2=(\d+)\s+HDC3=(\d+)\s+HDC4=(\d+)\s+USB=(\d+)\s+TOTAL=(\d+)',
                            decoded
                        )

                        if match:
                            ens, vcnl, hdc1, hdc2, hdc3, hdc4, usb, total = map(int, match.groups())

                            profile_data['ENS'].append(ens)
                            profile_data['VCNL'].append(vcnl)
                            profile_data['HDC1'].append(hdc1)
                            profile_data['HDC2'].append(hdc2)
                            profile_data['HDC3'].append(hdc3)
                            profile_data['HDC4'].append(hdc4)
                            profile_data['USB'].append(usb)
                            profile_data['TOTAL'].append(total)

                            # 实时显示平均值
                            print(f"    平均值: ENS={sum(profile_data['ENS'])/len(profile_data['ENS']):.1f}us "
                                  f"VCNL={sum(profile_data['VCNL'])/len(profile_data['VCNL']):.1f}us "
                                  f"HDC(平均)={sum(profile_data['HDC1']+profile_data['HDC2']+profile_data['HDC3']+profile_data['HDC4'])/(4*len(profile_data['HDC1'])):.1f}us "
                                  f"USB={sum(profile_data['USB'])/len(profile_data['USB']):.1f}us "
                                  f"TOTAL={sum(profile_data['TOTAL'])/len(profile_data['TOTAL']):.1f}us")
                    else:
                        # 普通数据
                        data_count += 1

                except UnicodeDecodeError:
                    pass

        # 统计分析
        print("\n" + "=" * 80)
        print("性能分析结果:")
        print("=" * 80)

        if not profile_data['TOTAL']:
            print("⚠️  未收到性能分析数据！请检查固件是否正确烧录。")
            ser.close()
            return

        def calc_stats(data):
            if not data:
                return 0, 0, 0, 0
            avg = sum(data) / len(data)
            min_val = min(data)
            max_val = max(data)
            return avg, min_val, max_val, len(data)

        print(f"\n📊 传感器读取时间统计 (样本数: {len(profile_data['TOTAL'])}):\n")

        for sensor in ['ENS', 'VCNL', 'HDC1', 'HDC2', 'HDC3', 'HDC4', 'USB', 'TOTAL']:
            avg, min_val, max_val, count = calc_stats(profile_data[sensor])
            print(f"  {sensor:6s}: 平均={avg:6.1f}us  最小={min_val:4d}us  最大={max_val:4d}us")

        # 计算总的HDC平均值
        all_hdc = profile_data['HDC1'] + profile_data['HDC2'] + profile_data['HDC3'] + profile_data['HDC4']
        hdc_avg, hdc_min, hdc_max, _ = calc_stats(all_hdc)
        print(f"\n  HDC(所有): 平均={hdc_avg:6.1f}us  最小={hdc_min:4d}us  最大={hdc_max:4d}us")

        # 百分比分析
        total_avg = sum(profile_data['TOTAL']) / len(profile_data['TOTAL'])
        print(f"\n📈 耗时占比分析 (总循环时间: {total_avg:.1f}us):\n")

        for sensor in ['ENS', 'VCNL', 'HDC1', 'HDC2', 'HDC3', 'HDC4', 'USB']:
            avg = sum(profile_data[sensor]) / len(profile_data[sensor])
            percentage = (avg / total_avg) * 100
            bar_length = int(percentage / 2)
            bar = '█' * bar_length
            print(f"  {sensor:6s}: {percentage:5.1f}% {bar}")

        # 计算理论刷新率
        freq_hz = 1000000 / total_avg  # 1秒 = 1,000,000微秒
        print(f"\n⚡ 性能评估:")
        print(f"  单次循环时间: {total_avg:.1f} us")
        print(f"  理论刷新率: {freq_hz:.1f} Hz")
        print(f"  实际数据量: {data_count} 条 / {duration} 秒 = {data_count/duration:.1f} Hz")

        # 瓶颈识别
        print(f"\n🔍 瓶颈分析:")

        sensor_times = {
            'ENS160气体传感器': sum(profile_data['ENS']) / len(profile_data['ENS']),
            'VCNL4040光学传感器': sum(profile_data['VCNL']) / len(profile_data['VCNL']),
            'HDC302x温湿度传感器(4个)': hdc_avg * 4,
            'USB传输': sum(profile_data['USB']) / len(profile_data['USB']),
        }

        sorted_sensors = sorted(sensor_times.items(), key=lambda x: x[1], reverse=True)

        for i, (name, time_us) in enumerate(sorted_sensors, 1):
            percentage = (time_us / total_avg) * 100
            print(f"  {i}. {name:25s}: {time_us:6.1f}us ({percentage:5.1f}%)")

        ser.close()

    except serial.SerialException as e:
        print(f"串口错误: {e}")
    except KeyboardInterrupt:
        print("\n用户中断测试")

if __name__ == '__main__':
    import sys

    port = '/dev/ttyACM0'
    if len(sys.argv) > 1:
        port = sys.argv[1]

    analyze_performance(port, duration=15)
