import numpy as np


def stereo_to_mono(stereo: np.ndarray) -> np.ndarray:
    stereo = np.asarray(stereo, dtype=np.float32)
    if stereo.ndim != 2 or stereo.shape[1] <= 0:
        raise ValueError(f"expected stereo audio with shape [samples, channels], got {stereo.shape}")
    return stereo.mean(axis=1, dtype=np.float32)


def resample_to_16k(stereo: np.ndarray, input_rate: int = 48000) -> np.ndarray:
    mono = stereo_to_mono(stereo)
    if input_rate == 16000:
        return mono
    if input_rate != 48000:
        raise ValueError(f"unsupported input_rate: {input_rate}")

    remainder = mono.shape[0] % 3
    if remainder:
        pad_width = 3 - remainder
        mono = np.pad(mono, (0, pad_width), mode="edge")

    return mono.reshape(-1, 3).mean(axis=1, dtype=np.float32)


def hz_to_mel(hz: np.ndarray) -> np.ndarray:
    hz = np.asarray(hz, dtype=np.float32)
    return 2595.0 * np.log10(1.0 + hz / 700.0)


def mel_to_hz(mel: np.ndarray) -> np.ndarray:
    mel = np.asarray(mel, dtype=np.float32)
    return 700.0 * (10.0 ** (mel / 2595.0) - 1.0)


def build_mel_filterbank(sample_rate: int, n_fft: int, n_mels: int) -> np.ndarray:
    mel_edges = np.linspace(
        hz_to_mel(np.array([0.0], dtype=np.float32))[0],
        hz_to_mel(np.array([sample_rate / 2.0], dtype=np.float32))[0],
        n_mels + 2,
        dtype=np.float32,
    )
    hz_edges = mel_to_hz(mel_edges)
    bins = np.floor((n_fft + 1) * hz_edges / sample_rate).astype(np.int32)
    filters = np.zeros((n_mels, n_fft // 2 + 1), dtype=np.float32)

    for index in range(n_mels):
        left = bins[index]
        center = bins[index + 1]
        right = bins[index + 2]

        if center > left:
            filters[index, left:center] = np.linspace(0.0, 1.0, center - left, endpoint=False, dtype=np.float32)
        if right > center:
            filters[index, center:right] = np.linspace(1.0, 0.0, right - center, endpoint=False, dtype=np.float32)

    return filters


def compute_log_mel(
    audio: np.ndarray,
    sample_rate: int = 16000,
    n_mels: int = 64,
    window_ms: float = 25.0,
    hop_ms: float = (1000.0 / 60.0),
) -> np.ndarray:
    audio = np.asarray(audio, dtype=np.float32)
    if audio.ndim != 1:
        raise ValueError(f"expected mono audio, got shape {audio.shape}")

    n_fft = max(int(sample_rate * window_ms / 1000.0), 1)
    hop_length = max(int(sample_rate * hop_ms / 1000.0), 1)
    if audio.shape[0] < n_fft:
        return np.zeros((n_mels, 0), dtype=np.float32)

    window = np.hanning(n_fft).astype(np.float32)
    frame_count = 1 + (audio.shape[0] - n_fft) // hop_length
    frames = np.empty((frame_count, n_fft), dtype=np.float32)

    for index in range(frame_count):
        start = index * hop_length
        frames[index] = audio[start:start + n_fft] * window

    spec = np.fft.rfft(frames, axis=1)
    power = (np.abs(spec) ** 2).T.astype(np.float32)
    mel_filters = build_mel_filterbank(sample_rate, n_fft, n_mels)
    mel = mel_filters @ power
    return np.log10(np.maximum(mel, 1e-10)).astype(np.float32)
