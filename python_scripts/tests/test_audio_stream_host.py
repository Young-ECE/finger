import tempfile
import unittest
import wave
from pathlib import Path

import numpy as np

from python_scripts.audio_stream_protocol import AudioPacket


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


if __name__ == "__main__":
    unittest.main()
