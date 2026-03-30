import unittest

import numpy as np


class AudioFeaturePipelineTest(unittest.TestCase):
    def test_resample_and_log_mel_shape(self):
        from python_scripts.audio_feature_pipeline import resample_to_16k, compute_log_mel

        t = np.arange(4800, dtype=np.float32) / 48000.0
        stereo = np.stack(
            [
                np.sin(2.0 * np.pi * 440.0 * t),
                np.sin(2.0 * np.pi * 660.0 * t),
            ],
            axis=1,
        )

        mono_16k = resample_to_16k(stereo, input_rate=48000)
        mel = compute_log_mel(mono_16k, sample_rate=16000, n_mels=64, hop_ms=(1000.0 / 60.0))

        self.assertEqual(mono_16k.shape[0], 1600)
        self.assertEqual(mel.shape[0], 64)
        self.assertGreater(mel.shape[1], 0)

    def test_rejects_unsupported_input_rate(self):
        from python_scripts.audio_feature_pipeline import resample_to_16k

        stereo = np.zeros((10, 2), dtype=np.float32)

        with self.assertRaises(ValueError):
            resample_to_16k(stereo, input_rate=32000)


if __name__ == "__main__":
    unittest.main()
