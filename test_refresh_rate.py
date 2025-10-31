#!/usr/bin/env python3
"""
快速测试传感器数据刷新率
"""
import serial
import time

def test_refresh_rate(port='/dev/ttyACM0', baudrate=115200, duration=5):
    """
    测试指定时间内的数据刷新率

    Args:
        port: 串口设备路径
        baudrate: 波特率
        duration: 测试持续时间（秒）
    """
    print(f"正在连接到 {port}...")

    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"已连接! 正在测试 {duration} 秒...")
        print("=" * 60)

        start_time = time.time()
        count = 0
        last_print = start_time

        while time.time() - start_time < duration:
            line = ser.readline()
            if line:
                try:
                    decoded = line.decode('utf-8').strip()
                    if decoded:
                        count += 1

                        # 每秒打印一次统计
                        current_time = time.time()
                        if current_time - last_print >= 1.0:
                            elapsed = current_time - start_time
                            rate = count / elapsed
                            print(f"[{elapsed:.1f}s] 收到 {count} 条数据, 当前速率: {rate:.1f} Hz")
                            last_print = current_time

                        # 显示最近一条数据
                        if count <= 5 or count % 100 == 0:
                            print(f"  数据 #{count}: {decoded[:80]}")

                except UnicodeDecodeError:
                    pass

        # 最终统计
        total_time = time.time() - start_time
        avg_rate = count / total_time

        print("=" * 60)
        print(f"\n测试完成!")
        print(f"总时间: {total_time:.2f} 秒")
        print(f"总数据量: {count} 条")
        print(f"平均刷新率: {avg_rate:.2f} Hz")
        print(f"理论最大刷新率: {count/duration:.2f} Hz (假设持续)")

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

    test_refresh_rate(port, duration=10)
