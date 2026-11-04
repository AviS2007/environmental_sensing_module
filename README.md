# Environmental sensing module

This project targets the SparkFun RedBoard Plus as an Arduino Uno-compatible
ATmega328P board and reads a BH1750 light sensor through the Qwiic I2C
connector.

## VS Code setup

1. Install the PlatformIO extension.
2. Open this folder in VS Code.
3. Connect the RedBoard Plus over USB.
4. Select the detected serial port in PlatformIO.
5. Upload the project.
6. Open the PlatformIO serial monitor at `115200` baud.

The firmware scans the I2C bus at startup and checks both common BH1750
addresses: `0x23` and `0x5C`. It samples every five seconds and emits a
machine-readable `SENSOR_CSV` line alongside the diagnostic output.

The current firmware also initializes and reads the SCD-30, SEN0514/ENS160,
and both SEN0465 gas sensors. The SCD-30 can take several seconds to produce
its first measurement, and the SEN0465 probes require at least five minutes of
initial warm-up for useful readings.

The BMV080 is detected at its I2C address, but particulate readings are not
yet enabled. Its SparkFun driver requires Bosch BMV080 SDK files in addition to
the PlatformIO library, and those files must be added before enabling that
driver on the ATmega328P.

The RedBoard Plus uses the Arduino Uno target. Its Qwiic connector is connected
to the standard I2C pins: SDA/A4 and SCL/A5.

## Daily Excel-compatible logging

Install the logger dependency once:

```powershell
py -m pip install -r requirements.txt
```

With the board connected, upload the latest firmware first, then start the
logger. For the currently detected board on `COM9`:

```powershell
& "$HOME\.platformio\penv\Scripts\platformio.exe" run --target upload --upload-port COM9
py tools\serial_to_excel.py --port COM9
```

Replace both port examples with the board's actual serial port. Stop any
PlatformIO serial monitor before uploading or starting the logger because only
one program can open the serial port at a time.

The logger writes append-only daily CSV files. Start the logger first, then
open the active CSV in Excel as read-only. The logger opens the CSV with
Windows shared read/write access, so Excel can view the same file while new
rows are appended. Excel may require Refresh or reopening the file to display
rows added after it was opened.

The logger writes `sensor_data_YYMMDD.csv` files to
`raw_ambient_sensor_data` with the same columns as the existing sensor export:
`timestamp`, `temp_C`, `humidity_%`, `lux`, `o2_%`, `co2_ppm`, `h2_ppm`,
`h2_temp_C`, `o2_temp_C`, `aqi`, `tvoc_ppb`, `eco2_ppm`, `sound_raw`, and
`raw`. A new daily CSV is selected automatically after UTC midnight. If an
old-schema CSV already exists for the day, it is preserved as
`sensor_data_YYMMDD.legacy.csv` and a new CSV with the expanded schema is
started.

The SCD-30 supplies the CSV `temp_C`, `humidity_%`, and `co2_ppm` values. It is
queried once per five-second sample. The SCD-30 does not measure pressure, so
there is no pressure column.

## Next wiring steps

Before adding the remaining sensors, record each sensor's part number and I2C
address. I2C devices can share SDA/SCL when their addresses are unique. The
digital volume sensor will need its part number and signal wiring before a pin
and input mode can be selected.
