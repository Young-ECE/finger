import contextlib
import io
import tempfile
import unittest
import wave
from pathlib import Path
from unittest import mock

import numpy as np

from python_scripts.audio_stream_protocol import AudioPacket
from python_scripts.tests.test_audio_stream_protocol import build_packet


ROOT = Path(__file__).resolve().parents[1]


class AudioStreamHostTest(unittest.TestCase):
    def test_inventory_prefers_new_host_tool(self):
        self.assertTrue((ROOT / "audio_stream_host.py").exists())

        for legacy_name in [
            "analyze_performance.py",
            "sensor_viewer_complete.py",
            "sensor_viewer_no_mic.py",
            "sensor_viewer_single.py",
            "sensor_visualizer.py",
            "temp_humidity_viewer.py",
            "test_refresh_rate.py",
        ]:
            self.assertFalse((ROOT / legacy_name).exists(), legacy_name)

    def test_capture_session_tracks_packets_and_drops(self):
        from python_scripts.audio_stream_host import AudioCaptureSession

        session = AudioCaptureSession()
        packet_a = AudioPacket(
            sequence=4,
            flags=0x00,
            sample_rate=48000,
            channels=2,
            bits_per_sample=16,
            samples_per_channel=2,
            pcm=np.array([[1, -1], [2, -2]], dtype=np.int16),
        )
        packet_b = AudioPacket(
            sequence=6,
            flags=0x80,
            sample_rate=48000,
            channels=2,
            bits_per_sample=16,
            samples_per_channel=1,
            pcm=np.array([[3, -3]], dtype=np.int16),
        )

        session.handle_packet(packet_a)
        session.handle_packet(packet_b)

        self.assertEqual(session.stats.packets_received, 2)
        self.assertEqual(session.stats.dropped_packets, 1)
        self.assertEqual(session.stats.overflow_packets, 1)
        self.assertEqual(session.stereo_pcm().tolist(), [[1, -1], [2, -2], [3, -3]])

    def test_save_pcm_wav_writes_expected_metadata(self):
        from python_scripts.audio_stream_host import save_pcm_wav

        stereo = np.array([[10, -10], [20, -20], [30, -30]], dtype=np.int16)
        with tempfile.TemporaryDirectory() as temp_dir:
            wav_path = Path(temp_dir) / "capture.wav"
            save_pcm_wav(wav_path, stereo, sample_rate=48000)

            with wave.open(str(wav_path), "rb") as wav_file:
                self.assertEqual(wav_file.getnchannels(), 2)
                self.assertEqual(wav_file.getsampwidth(), 2)
                self.assertEqual(wav_file.getframerate(), 48000)
                self.assertEqual(wav_file.getnframes(), 3)
                decoded = np.frombuffer(wav_file.readframes(3), dtype="<i2").reshape(-1, 2)

        np.testing.assert_array_equal(decoded, stereo)

    def test_cli_accepts_pwm_duty_arguments(self):
        from python_scripts.audio_stream_host import build_arg_parser

        args = build_arg_parser().parse_args(
            [
                "--port", "COM5",
                "--lcd1-duty", "10",
                "--led1-duty", "20",
                "--lcd2-duty", "30",
                "--led2-duty", "40",
                "--max-packets", "1",
            ]
        )

        self.assertEqual(args.lcd1_duty, 10)
        self.assertEqual(args.led2_duty, 40)

    def test_capture_stream_sends_pwm_command_before_capture_and_clears_rx_buffer(self):
        from python_scripts.audio_stream_host import capture_stream
        from python_scripts.pwm_control_protocol import build_pwm_packet

        events = []
        expected_packet = build_pwm_packet(sequence=0, lcd1=10, led1=20, lcd2=30, led2=40)

        class FakeSerial:
            def __init__(self, *args, **kwargs):
                self.args = args
                self.kwargs = kwargs

            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc, tb):
                return False

            def write(self, data):
                events.append(("write", bytes(data)))
                return len(data)

            def flush(self):
                events.append(("flush", None))

            def reset_input_buffer(self):
                events.append(("reset_input_buffer", None))

            def read(self, size):
                events.append(("read", size))
                return b""

        with mock.patch("python_scripts.audio_stream_host.serial.Serial", FakeSerial):
            capture_stream("COM5", max_packets=0, quiet=True, pwm_duties=(10, 20, 30, 40))

        self.assertEqual(events[0], ("write", expected_packet))
        self.assertEqual(events[1], ("flush", None))
        self.assertEqual(events[2], ("reset_input_buffer", None))
        self.assertEqual(len([event for event in events if event[0] == "read"]), 0)

    def test_capture_stream_zero_duration_sends_pwm_then_returns_without_reading(self):
        from python_scripts.audio_stream_host import capture_stream
        from python_scripts.pwm_control_protocol import build_pwm_packet

        events = []
        expected_packet = build_pwm_packet(sequence=0, lcd1=11, led1=22, lcd2=33, led2=44)

        class FakeSerial:
            def __init__(self, *args, **kwargs):
                self.args = args
                self.kwargs = kwargs

            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc, tb):
                return False

            def write(self, data):
                events.append(("write", bytes(data)))
                return len(data)

            def flush(self):
                events.append(("flush", None))

            def reset_input_buffer(self):
                events.append(("reset_input_buffer", None))

            def read(self, size):
                events.append(("read", size))
                return b""

        with mock.patch("python_scripts.audio_stream_host.serial.Serial", FakeSerial):
            capture_stream("COM5", duration=0.0, quiet=True, pwm_duties=(11, 22, 33, 44))

        self.assertEqual(events[0], ("write", expected_packet))
        self.assertEqual(events[1], ("flush", None))
        self.assertEqual(events[2], ("reset_input_buffer", None))
        self.assertEqual(len([event for event in events if event[0] == "read"]), 0)

    def test_main_rejects_out_of_range_pwm_duty_as_argument_error(self):
        from python_scripts.audio_stream_host import main

        stderr = io.StringIO()

        class FakeSerial:
            def __init__(self, *args, **kwargs):
                self.args = args
                self.kwargs = kwargs

            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc, tb):
                return False

            def write(self, data):
                return len(data)

            def flush(self):
                return None

            def read(self, size):
                return b""

        with mock.patch("python_scripts.audio_stream_host.serial.Serial", FakeSerial):
            with contextlib.redirect_stderr(stderr):
                exit_code = main(
                    [
                        "--port", "COM5",
                        "--lcd1-duty", "100",
                        "--led1-duty", "20",
                        "--lcd2-duty", "30",
                        "--led2-duty", "40",
                        "--max-packets", "0",
                        "--quiet",
                    ]
                )

        self.assertEqual(exit_code, 2)
        self.assertIn("argument error", stderr.getvalue())

    def test_capture_stream_honors_packet_limit_with_multi_packet_read(self):
        from python_scripts.audio_stream_host import capture_stream

        chunks = [
            build_packet(1, 0x00, 1, [10, -10]) + build_packet(2, 0x00, 1, [20, -20]),
            b"",
        ]

        class FakeSerial:
            def __init__(self, *args, **kwargs):
                self.args = args
                self.kwargs = kwargs

            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc, tb):
                return False

            def read(self, size):
                if chunks:
                    return chunks.pop(0)
                return b""

        with mock.patch("python_scripts.audio_stream_host.serial.Serial", FakeSerial):
            session = capture_stream("COM5", max_packets=1, quiet=True)

        self.assertEqual(session.stats.packets_received, 1)
        self.assertEqual(session.stereo_pcm().tolist(), [[10, -10]])


if __name__ == "__main__":
    unittest.main()
