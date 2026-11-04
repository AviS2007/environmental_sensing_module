"""Append 5-second environmental sensor readings to daily CSV files."""

from __future__ import annotations

import argparse
import csv
import os
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

import serial
from serial.tools import list_ports


HEADERS = [
    "timestamp",
    "temp_C",
    "humidity_%",
    "lux",
    "o2_%",
    "co2_ppm",
    "h2_ppm",
    "h2_temp_C",
    "o2_temp_C",
    "aqi",
    "tvoc_ppb",
    "eco2_ppm",
    "sound_raw",
    "raw",
]

_active_path: Optional[Path] = None
_active_file = None
_active_writer = None


def open_append_file(path: Path):
    if sys.platform != "win32":
        return path.open("a", newline="", encoding="utf-8")

    import ctypes
    import msvcrt

    create_file = ctypes.windll.kernel32.CreateFileW
    create_file.argtypes = [
        ctypes.c_wchar_p,
        ctypes.c_uint32,
        ctypes.c_uint32,
        ctypes.c_void_p,
        ctypes.c_uint32,
        ctypes.c_uint32,
        ctypes.c_void_p,
    ]
    create_file.restype = ctypes.c_void_p
    handle = create_file(
        str(path),
        0x00000004,  # FILE_APPEND_DATA
        0x00000001 | 0x00000002 | 0x00000004,  # Share read, write, and delete.
        None,
        0x00000004,  # OPEN_ALWAYS
        0x00000080,  # FILE_ATTRIBUTE_NORMAL
        None,
    )
    if handle == ctypes.c_void_p(-1).value:
        raise PermissionError(ctypes.get_last_error(), os.strerror(ctypes.get_last_error()), str(path))
    descriptor = msvcrt.open_osfhandle(handle, os.O_APPEND | os.O_WRONLY)
    return os.fdopen(descriptor, "a", newline="", encoding="utf-8")


def close_active_file() -> None:
    global _active_path, _active_file, _active_writer
    if _active_file is not None:
        _active_file.close()
    _active_path = None
    _active_file = None
    _active_writer = None


def get_active_writer(path: Path):
    global _active_path, _active_file, _active_writer
    if _active_path != path:
        close_active_file()
        _active_path = path
        is_new_file = not path.exists() or path.stat().st_size == 0
        if not is_new_file:
            with path.open("r", encoding="utf-8", newline="") as existing_file:
                existing_header = next(csv.reader(existing_file), [])
            if existing_header != HEADERS:
                legacy_path = path.with_suffix(".legacy.csv")
                path.replace(legacy_path)
                print(f"Preserved old CSV as {legacy_path}", flush=True)
                is_new_file = True
        _active_file = open_append_file(path)
        _active_writer = csv.writer(_active_file)
        if is_new_file:
            _active_writer.writerow(HEADERS)
            _active_file.flush()
    return _active_file, _active_writer


def parse_value(value: str) -> Optional[float]:
    return float(value) if value else None


def daily_path(output_dir: Path, timestamp: datetime) -> Path:
    return output_dir / f"sensor_data_{timestamp.strftime('%y%m%d')}.csv"


def save_reading(output_dir: Path, values: list[str]) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    timestamp = datetime.now(timezone.utc)
    timestamp_text = timestamp.isoformat(timespec="microseconds")
    temperature = parse_value(values[0])
    humidity = parse_value(values[1])
    lux = parse_value(values[2])
    oxygen = parse_value(values[3])
    co2 = parse_value(values[4])
    hydrogen = parse_value(values[5])
    hydrogen_temperature = parse_value(values[6])
    oxygen_temperature = parse_value(values[7])
    aqi = parse_value(values[8])
    tvoc = parse_value(values[9])
    eco2 = parse_value(values[10])
    sound = parse_value(values[11])
    raw = (
        f"Temp: {temperature:.2f} C | Humidity: {humidity:.2f} % | "
        f"Light: {lux:.1f} lx | O2: {oxygen:.2f} %vol | CO2: {co2:.0f} ppm | "
        f"H2: {hydrogen:.2f} ppm | H2 temp: {hydrogen_temperature:.2f} C | "
        f"O2 temp: {oxygen_temperature:.2f} C | AQI: {aqi:.0f} | TVOC: {tvoc:.0f} ppb | "
        f"eCO2: {eco2:.0f} ppm | Sound: {sound:.0f}"
        if all(value is not None for value in (
            temperature, humidity, lux, oxygen, co2, hydrogen,
            hydrogen_temperature, oxygen_temperature, aqi, tvoc, eco2, sound
        ))
        else " | ".join(
            [
                f"Temp: {temperature:.2f} C" if temperature is not None else "Temp: N/A",
                f"Humidity: {humidity:.2f} %" if humidity is not None else "Humidity: N/A",
                f"Light: {lux:.1f} lx" if lux is not None else "Light: N/A",
                f"O2: {oxygen:.2f} %vol" if oxygen is not None else "O2: N/A",
                f"CO2: {co2:.0f} ppm" if co2 is not None else "CO2: N/A",
                f"H2: {hydrogen:.2f} ppm" if hydrogen is not None else "H2: N/A",
                f"H2 temp: {hydrogen_temperature:.2f} C" if hydrogen_temperature is not None else "H2 temp: N/A",
                f"O2 temp: {oxygen_temperature:.2f} C" if oxygen_temperature is not None else "O2 temp: N/A",
                f"AQI: {aqi:.0f}" if aqi is not None else "AQI: N/A",
                f"TVOC: {tvoc:.0f} ppb" if tvoc is not None else "TVOC: N/A",
                f"eCO2: {eco2:.0f} ppm" if eco2 is not None else "eCO2: N/A",
                f"Sound: {sound:.0f}" if sound is not None else "Sound: N/A",
            ]
        )
    )
    path = daily_path(output_dir, timestamp)
    row = [
        timestamp_text, temperature, humidity, lux, oxygen, co2, hydrogen,
        hydrogen_temperature, oxygen_temperature, aqi, tvoc, eco2, sound, raw
    ]
    warned = False
    while True:
        try:
            output_file, writer = get_active_writer(path)
            writer.writerow(row)
            output_file.flush()
            break
        except PermissionError:
            if not warned:
                print(
                    f"{path.name} is locked by Excel. Close it or use a separate "
                    "read-only copy; logging will retry.",
                    flush=True,
                )
                warned = True
            time.sleep(1)
    print(f"Saved {timestamp_text} to {path}", flush=True)


def run(port: str, baud: int, output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    try:
        with serial.Serial(port, baudrate=baud, timeout=1) as connection:
            print(f"Listening on {port} at {baud} baud", flush=True)
            while True:
                line = connection.readline().decode("utf-8", errors="replace").strip()
                if not line.startswith("SENSOR_CSV,"):
                    if line and "I2C device found" in line:
                        print(f"Board: {line}", flush=True)
                    continue
                values = next(csv.reader([line[len("SENSOR_CSV,") :]]))
                if len(values) != 12:
                    print(f"Ignoring malformed sensor row: {line}", flush=True)
                    continue
                save_reading(output_dir, values)
    except KeyboardInterrupt:
        print("\nLogger stopped.", flush=True)
    except serial.SerialException as error:
        available_ports = [entry.device for entry in list_ports.comports()]
        available = ", ".join(available_ports) if available_ports else "none"
        raise SystemExit(
            f"Could not open {port}: {error}\n"
            f"Available serial ports: {available}\n"
            "Close PlatformIO Serial Monitor and use the port shown by "
            "'python -m platformio device list'."
        ) from error
    finally:
        close_active_file()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="USB serial port, e.g. COM4")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "raw_ambient_sensor_data",
    )
    args = parser.parse_args()
    run(args.port, args.baud, args.output_dir)


if __name__ == "__main__":
    main()
