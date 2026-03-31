import unittest


class PwmControlProtocolTest(unittest.TestCase):
    def test_build_pwm_packet_matches_wire_format(self):
        from python_scripts.pwm_control_protocol import build_pwm_packet

        packet = build_pwm_packet(sequence=0x12, lcd1=10, led1=20, lcd2=30, led2=40)

        self.assertEqual(packet[:6], bytes([0x43, 0x54, 0x01, 0x01, 0x04, 0x12]))
        self.assertEqual(packet[6:10], bytes([10, 20, 30, 40]))
        self.assertEqual(packet[10], 0x52)
        self.assertEqual(len(packet), 11)

    def test_build_pwm_packet_rejects_out_of_range_values(self):
        from python_scripts.pwm_control_protocol import build_pwm_packet

        with self.assertRaises(ValueError):
            build_pwm_packet(sequence=0, lcd1=100, led1=0, lcd2=0, led2=0)


if __name__ == "__main__":
    unittest.main()
