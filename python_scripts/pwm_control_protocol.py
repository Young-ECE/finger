from dataclasses import dataclass


MAGIC = b"\x43\x54"
PROTOCOL_VERSION = 1
PACKET_TYPE_SET_PWM_DUTY = 1
PAYLOAD_LENGTH = 4
PACKET_LENGTH = 11


def crc8(data: bytes) -> int:
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def _validate_sequence(value: int) -> int:
    if type(value) is not int:
        raise TypeError(f"sequence must be an integer, got {value!r}")
    if not 0 <= value <= 255:
        raise ValueError(f"sequence must be in range 0..255, got {value}")
    return value


def _validate_duty(name: str, value: int) -> int:
    if type(value) is not int:
        raise TypeError(f"{name} must be an integer, got {value!r}")
    if not 0 <= value <= 99:
        raise ValueError(f"{name} must be in range 0..99, got {value}")
    return value


@dataclass(frozen=True)
class PwmDutyCommand:
    sequence: int
    lcd1: int
    led1: int
    lcd2: int
    led2: int

    def to_bytes(self) -> bytes:
        body = bytes(
            [
                PROTOCOL_VERSION,
                PACKET_TYPE_SET_PWM_DUTY,
                PAYLOAD_LENGTH,
                _validate_sequence(self.sequence),
                _validate_duty("lcd1", self.lcd1),
                _validate_duty("led1", self.led1),
                _validate_duty("lcd2", self.lcd2),
                _validate_duty("led2", self.led2),
            ]
        )
        return MAGIC + body + bytes([crc8(body)])


def build_pwm_packet(sequence: int, lcd1: int, led1: int, lcd2: int, led2: int) -> bytes:
    return PwmDutyCommand(
        sequence=sequence,
        lcd1=lcd1,
        led1=led1,
        lcd2=lcd2,
        led2=led2,
    ).to_bytes()
