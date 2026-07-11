import pathlib
import unittest
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import package_firmware as p


class PackageTest(unittest.TestCase):
    def test_round_trip(self):
        data = p.make_header("pico_w_rp2040", "build", b"x" * 256) + b"x" * 256
        self.assertEqual(p.parse_package(data), ("pico_w_rp2040", "build", b"x" * 256))

    def test_rejects_corrupt_and_truncated(self):
        data = p.make_header("pico_w_rp2040", "build", b"x" * 256) + b"x" * 256
        with self.assertRaises(ValueError): p.parse_package(data[:-1])
        with self.assertRaises(ValueError): p.parse_package(bytes([1]) + data[1:])
