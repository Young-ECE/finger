from dataclasses import dataclass
import struct

import numpy as np


MAGIC = b"\xAA\x55"
PROTOCOL_VERSION = 1
HEADER = struct.Struct("<2sBBIHBBHH")


@dataclass
class AudioPacket:
    sequence: int
    flags: int
    sample_rate: int
    channels: int
    bits_per_sample: int
    samples_per_channel: int
    pcm: np.ndarray


class AudioStreamParser:
    def __init__(self):
        self._buffer = bytearray()

    def feed(self, chunk: bytes):
        self._buffer.extend(chunk)
        packets = []

        while True:
            if len(self._buffer) < HEADER.size:
                return packets

            magic_index = self._buffer.find(MAGIC)
            if magic_index < 0:
                self._buffer.clear()
                return packets
            if magic_index > 0:
                del self._buffer[:magic_index]
            if len(self._buffer) < HEADER.size:
                return packets

            magic, version, flags, sequence, sample_rate, channels, bits_per_sample, samples_per_channel, payload_bytes = HEADER.unpack_from(self._buffer)
            if magic != MAGIC:
                del self._buffer[:2]
                continue
            if version != PROTOCOL_VERSION:
                raise ValueError(f"unsupported audio protocol version: {version}")
            if channels <= 0:
                raise ValueError(f"invalid channel count: {channels}")
            if bits_per_sample != 16:
                raise ValueError(f"unsupported bits_per_sample: {bits_per_sample}")

            expected_payload_bytes = samples_per_channel * channels * (bits_per_sample // 8)
            if payload_bytes != expected_payload_bytes:
                raise ValueError(
                    f"payload size mismatch: header={payload_bytes}, expected={expected_payload_bytes}"
                )

            packet_bytes = HEADER.size + payload_bytes
            if len(self._buffer) < packet_bytes:
                return packets

            payload = bytes(self._buffer[HEADER.size:packet_bytes])
            del self._buffer[:packet_bytes]

            pcm = np.frombuffer(payload, dtype="<i2").reshape(samples_per_channel, channels)
            packets.append(
                AudioPacket(
                    sequence=sequence,
                    flags=flags,
                    sample_rate=sample_rate,
                    channels=channels,
                    bits_per_sample=bits_per_sample,
                    samples_per_channel=samples_per_channel,
                    pcm=pcm,
                )
            )


class AudioStreamReconstructor:
    def __init__(self):
        self.expected_sequence = None
        self.dropped_packets = 0

    def push(self, packet: AudioPacket):
        if self.expected_sequence is not None and packet.sequence != self.expected_sequence:
            if packet.sequence > self.expected_sequence:
                self.dropped_packets += packet.sequence - self.expected_sequence
            else:
                raise ValueError(
                    f"out-of-order packet sequence: got {packet.sequence}, expected {self.expected_sequence}"
                )

        self.expected_sequence = packet.sequence + 1
        return packet.pcm
