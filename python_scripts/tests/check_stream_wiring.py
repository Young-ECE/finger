from pathlib import Path


WORKTREE = Path(r"D:\technical documents\SSR\GJH\SHARP\new\finger\.worktrees\audio-stream-alignment")
MY_APPLICATION = WORKTREE / "Core" / "Src" / "my_application.c"
IRQ_FILE = WORKTREE / "Core" / "Src" / "stm32f4xx_it.c"


def main() -> None:
    my_application = MY_APPLICATION.read_text(encoding="utf-8")
    irq_file = IRQ_FILE.read_text(encoding="utf-8")
    errors = []

    if "AudioStream_InterleavePcmBlock(" not in my_application:
        errors.append("my_application.c does not interleave PCM blocks")
    if "AudioStream_BuildPacket(" not in my_application:
        errors.append("my_application.c does not build audio packets")
    if "CDC_Transmit_FS((uint8_t*)msg, len);" in my_application:
        errors.append("my_application.c still transmits CSV text packets")
    if "mic.audio_result_left =" in irq_file:
        errors.append("stm32f4xx_it.c still writes single-sample mic.audio_result_left")
    if "mic_2.audio_result_left =" in irq_file:
        errors.append("stm32f4xx_it.c still writes single-sample mic_2.audio_result_left")

    if errors:
        raise SystemExit("\n".join(errors))

    print("stream wiring checks passed")


if __name__ == "__main__":
    main()
