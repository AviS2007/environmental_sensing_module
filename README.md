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
addresses: `0x23` and `0x5C`. It then prints a lux reading once per second.

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

## Next wiring steps

Before adding the remaining sensors, record each sensor's part number and I2C
address. I2C devices can share SDA/SCL when their addresses are unique. The
digital volume sensor will need its part number and signal wiring before a pin
and input mode can be selected.
