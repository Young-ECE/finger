import struct
import unittest


MAGIC = b"\xAA\x55"
HEADER = struct.Struct("<2sBBIHBBHH")


def build_packet(sequence, flags, samples_per_channel, interleaved_pcm):
    payload = struct.pack("<" + "h" * len(interleaved_pcm), *interleaved_pcm)
    header = HEADER.pack(
        MAGIC,
        1,
        flags,
        sequence,
        48000,
        2,
        16,
        samples_per_channel,
        len(payload),
    )
    return header + payload


class AudioStreamParserTest(unittest.TestCase):
    def test_parses_a_complete_packet(self):
        from python_scripts.audio_stream_protocol import AudioStreamParser

        parser = AudioStreamParser()
        packet = build_packet(7, 0x01, 2, [1, -1, 2, -2])
        frames = parser.feed(packet)

        self.assertEqual(len(frames), 1)
        self.assertEqual(frames[0].sequence, 7)
        self.assertEqual(frames[0].flags, 0x01)
        self.assertEqual(frames[0].pcm.tolist(), [[1, -1], [2, -2]])

    def test_tracks_sequence_gaps(self):
        from python_scripts.audio_stream_protocol import AudioStreamParser, AudioStreamReconstructor

        parser = AudioStreamParser()
        reconstructor = AudioStreamReconstructor()
        packet_a = build_packet(4, 0x00, 1, [3, -3])
        packet_b = build_packet(6, 0x80, 1, [5, -5])

        for frame in parser.feed(packet_a):
            reconstructor.push(frame)
        for frame in parser.feed(packet_b):
            pcm = reconstructor.push(frame)

        self.assertEqual(reconstructor.dropped_packets, 1)
        self.assertEqual(pcm.tolist(), [[5, -5]])


if __name__ == "__main__":
    unittest.main()
