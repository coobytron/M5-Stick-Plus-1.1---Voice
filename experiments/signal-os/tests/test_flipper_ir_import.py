"""Host-only tests for Flipper IR → Signal OS conversion."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from import_flipper_ir import parse_signals, encode_signal, nec_pulses


def test_raw():
    data = """Filetype: Flipper Infrared signals file
Version: 1
#
name: Lamp
type: raw
frequency: 38000
duty_cycle: 0.330000
data: 9000 4500 560 560 560 1690
#
"""
    signals = list(parse_signals(data))
    assert len(signals) == 1
    assert encode_signal(signals[0]) == "IRRAW|Lamp|38000|9000,4500,560,560,560,1690"


def test_nec():
    assert len(nec_pulses(0, 0)) == 67
    source = """name: Power
type: parsed
protocol: NEC
address: 00 00 00 00
command: 12 00 00 00
#
"""
    cmd = encode_signal(next(parse_signals(source)))
    assert cmd.startswith("IRRAW|Power|38000|9000,4500,")
    assert len(cmd.split("|")[-1].split(",")) == 67


def test_limits_and_unsupported_protocol():
    source = """name: RC5
type: parsed
protocol: RC5
address: 00 00
command: 00 00
#
"""
    try:
        encode_signal(next(parse_signals(source)))
    except ValueError as exc:
        assert "unsupported parsed" in str(exc)
    else:
        raise AssertionError("unsupported protocol accepted")
    raw = {"name": "Oversized", "type": "raw", "frequency": "38000",
           "data": " ".join(["560"] * 193)}
    try:
        encode_signal(raw)
    except ValueError as exc:
        assert "pulse count" in str(exc)
    else:
        raise AssertionError("oversized waveform accepted")


if __name__ == "__main__":
    test_raw()
    test_nec()
    test_limits_and_unsupported_protocol()
    print("Flipper IR importer tests passed")
