import argparse
import sys
import time
import wave
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import serial

try:
    from python_scripts.audio_feature_pipeline import compute_log_mel, resample_to_16k
    from python_scripts.pwm_control_protocol import build_pwm_packet
    from python_scripts.audio_stream_protocol import AudioStreamParser, AudioStreamReconstructor
except ImportError:
    from audio_feature_pipeline import compute_log_mel, resample_to_16k
    from pwm_control_protocol import build_pwm_packet
    from audio_stream_protocol import AudioStreamParser, AudioStreamReconstructor


DEFAULT_BAUDRATE = 115200
OVERFLOW_FLAG = 0x80


@dataclass
class AudioCaptureStats:
    packets_received: int = 0
    dropped_packets: int = 0
    overflow_packets: int = 0
    sample_rate: int = 0
    channels: int = 0
    bits_per_sample: int = 0
    samples_per_channel: int = 0
    total_samples_per_channel: int = 0

    @property
    def duration_seconds(self) -> float:
        if self.sample_rate <= 0:
            return 0.0
        return self.total_samples_per_channel / float(self.sample_rate)


class AudioCaptureSession:
    def __init__(self):
        self.stats = AudioCaptureStats()
        self._reconstructor = AudioStreamReconstructor()
        self._blocks = []

    def handle_packet(self, packet):
        pcm = self._reconstructor.push(packet)

        if self.stats.sample_rate and packet.sample_rate != self.stats.sample_rate:
            raise ValueError(
                f"inconsistent sample rate: got {packet.sample_rate}, expected {self.stats.sample_rate}"
            )
        if self.stats.channels and packet.channels != self.stats.channels:
            raise ValueError(
                f"inconsistent channel count: got {packet.channels}, expected {self.stats.channels}"
            )

        self.stats.packets_received += 1
        self.stats.dropped_packets = self._reconstructor.dropped_packets
        self.stats.sample_rate = packet.sample_rate
        self.stats.channels = packet.channels
        self.stats.bits_per_sample = packet.bits_per_sample
        self.stats.samples_per_channel = packet.samples_per_channel
        self.stats.total_samples_per_channel += int(pcm.shape[0])
        if packet.flags & OVERFLOW_FLAG:
            self.stats.overflow_packets += 1

        self._blocks.append(np.array(pcm, dtype=np.int16, copy=True))
        return pcm

    def stereo_pcm(self) -> np.ndarray:
        if not self._blocks:
            channels = self.stats.channels if self.stats.channels else 2
            return np.zeros((0, channels), dtype=np.int16)
        return np.concatenate(self._blocks, axis=0)

    def mono_16k(self) -> np.ndarray:
        stereo = self.stereo_pcm()
        if stereo.shape[0] == 0:
            return np.zeros((0,), dtype=np.float32)
        return resample_to_16k(stereo, input_rate=self.stats.sample_rate or 48000)

    def log_mel(self, n_mels: int = 64, hop_ms: float = (1000.0 / 60.0)) -> np.ndarray:
        mono = self.mono_16k()
        return compute_log_mel(mono, sample_rate=16000, n_mels=n_mels, hop_ms=hop_ms)

    def status_line(self) -> str:
        return (
            f"packets={self.stats.packets_received} "
            f"dropped={self.stats.dropped_packets} "
            f"overflow={self.stats.overflow_packets} "
            f"samples/ch={self.stats.total_samples_per_channel} "
            f"duration={self.stats.duration_seconds:.3f}s "
            f"rate={self.stats.sample_rate}Hz"
        )


def _as_int16_pcm(pcm: np.ndarray) -> np.ndarray:
    array = np.asarray(pcm)
    if array.ndim not in (1, 2):
        raise ValueError(f"expected mono or stereo PCM array, got shape {array.shape}")
    if np.issubdtype(array.dtype, np.floating):
        array = np.rint(array)
    array = np.clip(array, np.iinfo(np.int16).min, np.iinfo(np.int16).max)
    return array.astype(np.int16, copy=False)


def save_pcm_wav(path, pcm: np.ndarray, sample_rate: int) -> Path:
    wav_path = Path(path)
    wav_path.parent.mkdir(parents=True, exist_ok=True)

    pcm16 = _as_int16_pcm(pcm)
    channels = 1 if pcm16.ndim == 1 else pcm16.shape[1]
    frames = pcm16.reshape(-1, channels)

    with wave.open(str(wav_path), "wb") as wav_file:
        wav_file.setnchannels(channels)
        wav_file.setsampwidth(2)
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(frames.astype("<i2", copy=False).tobytes())

    return wav_path


def save_log_mel(path, mel: np.ndarray) -> Path:
    mel_path = Path(path)
    mel_path.parent.mkdir(parents=True, exist_ok=True)
    with mel_path.open("wb") as output_file:
        np.save(output_file, mel.astype(np.float32, copy=False))
    return mel_path


def _requested_pwm_values(args) -> tuple[int, int, int, int] | None:
    values = (args.lcd1_duty, args.led1_duty, args.lcd2_duty, args.led2_duty)
    if all(value is None for value in values):
        return None
    if any(value is None for value in values):
        raise ValueError("provide all four PWM duty values together")
    return tuple(int(value) for value in values)


def send_pwm_command(serial_port, duties: tuple[int, int, int, int], sequence: int = 0) -> None:
    packet = build_pwm_packet(
        sequence=sequence,
        lcd1=duties[0],
        led1=duties[1],
        lcd2=duties[2],
        led2=duties[3],
    )
    serial_port.write(packet)
    serial_port.flush()


def capture_stream(
    port: str,
    baudrate: int = DEFAULT_BAUDRATE,
    duration: float | None = None,
    max_packets: int | None = None,
    quiet: bool = False,
    pwm_duties: tuple[int, int, int, int] | None = None,
) -> AudioCaptureSession:
    parser = AudioStreamParser()
    session = AudioCaptureSession()
    start_time = time.monotonic()
    last_status_time = start_time

    with serial.Serial(port, baudrate, timeout=0.1) as serial_port:
        if pwm_duties is not None:
            send_pwm_command(serial_port, pwm_duties)

        while True:
            chunk = serial_port.read(4096)
            if chunk:
                for packet in parser.feed(chunk):
                    session.handle_packet(packet)

            now = time.monotonic()
            if not quiet and now - last_status_time >= 1.0:
                print(session.status_line())
                last_status_time = now

            if duration is not None and now - start_time >= duration:
                break
            if max_packets is not None and session.stats.packets_received >= max_packets:
                break

    return session


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Capture the binary dual-channel audio stream, save WAV, export 16 kHz log-mel features, and optionally set PWM duty values."
    )
    parser.add_argument("--port", required=True, help="Serial port, for example COM5")
    parser.add_argument("--baudrate", type=int, default=DEFAULT_BAUDRATE, help="Serial baud rate")
    parser.add_argument("--duration", type=float, default=None, help="Capture duration in seconds")
    parser.add_argument("--max-packets", type=int, default=None, help="Stop after N packets")
    parser.add_argument("--wav-out", type=Path, default=None, help="Optional stereo WAV output path")
    parser.add_argument("--mono16k-out", type=Path, default=None, help="Optional 16 kHz mono WAV output path")
    parser.add_argument("--log-mel-out", type=Path, default=None, help="Optional .npy output path for log-mel features")
    parser.add_argument("--n-mels", type=int, default=64, help="Mel bin count when exporting log-mel")
    parser.add_argument(
        "--hop-ms",
        type=float,
        default=(1000.0 / 60.0),
        help="Hop size in milliseconds when exporting log-mel",
    )
    parser.add_argument("--lcd1-duty", type=int, default=None, help="Optional LCD1 PWM duty in range 0..99")
    parser.add_argument("--led1-duty", type=int, default=None, help="Optional LED1 PWM duty in range 0..99")
    parser.add_argument("--lcd2-duty", type=int, default=None, help="Optional LCD2 PWM duty in range 0..99")
    parser.add_argument("--led2-duty", type=int, default=None, help="Optional LED2 PWM duty in range 0..99")
    parser.add_argument("--quiet", action="store_true", help="Suppress periodic status prints")
    return parser


def main(argv=None) -> int:
    args = build_arg_parser().parse_args(argv)
    try:
        pwm_duties = _requested_pwm_values(args)
    except ValueError as exc:
        print(f"argument error: {exc}", file=sys.stderr)
        return 2

    try:
        session = capture_stream(
            port=args.port,
            baudrate=args.baudrate,
            duration=args.duration,
            max_packets=args.max_packets,
            quiet=args.quiet,
            pwm_duties=pwm_duties,
        )
    except KeyboardInterrupt:
        print("capture interrupted by user", file=sys.stderr)
        return 130
    except serial.SerialException as exc:
        print(f"serial error: {exc}", file=sys.stderr)
        return 1

    stereo = session.stereo_pcm()
    print(session.status_line())

    if args.wav_out is not None:
        stereo_wav = save_pcm_wav(args.wav_out, stereo, sample_rate=session.stats.sample_rate or 48000)
        print(f"stereo wav saved to {stereo_wav}")

    if args.mono16k_out is not None:
        mono = session.mono_16k()
        mono_wav = save_pcm_wav(args.mono16k_out, mono, sample_rate=16000)
        print(f"mono 16 kHz wav saved to {mono_wav}")

    if args.log_mel_out is not None:
        mel = session.log_mel(n_mels=args.n_mels, hop_ms=args.hop_ms)
        mel_path = save_log_mel(args.log_mel_out, mel)
        print(f"log-mel saved to {mel_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
