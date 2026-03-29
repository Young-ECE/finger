from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

i2s_c = (ROOT / "Core/Src/i2s.c").read_text(encoding="utf-8", errors="ignore")
main_c = (ROOT / "Core/Src/main.c").read_text(encoding="utf-8", errors="ignore")
mic_h = (ROOT / "Core/Inc/microphone_sensor.h").read_text(encoding="utf-8", errors="ignore")
ioc = (ROOT / "finger.ioc").read_text(encoding="utf-8", errors="ignore")

assert "hi2s1.Init.AudioFreq = I2S_AUDIOFREQ_48K;" in i2s_c, "hi2s1 is not 48 kHz"
assert "hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_48K;" in i2s_c, "hi2s2 is not 48 kHz"
assert "PeriphClkInitStruct.PLLI2S.PLLI2SN = 192;" in main_c, "PLLI2SN is not 192"
assert "PeriphClkInitStruct.PLLI2S.PLLI2SR = 5;" in main_c, "PLLI2SR is not 5"
assert "#define MIC_BUFFER_SIZE AUDIO_STREAM_DMA_BUFFER_WORDS" in mic_h, "MIC buffer does not use stream constants"
assert "I2S1.AudioFreq=I2S_AUDIOFREQ_48K" in ioc, "CubeMX I2S1 config is not 48 kHz"
assert "I2S2.AudioFreq=I2S_AUDIOFREQ_48K" in ioc, "CubeMX I2S2 config is not 48 kHz"

print("audio config checks passed")
