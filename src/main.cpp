#include <Arduino.h>
#include <Wire.h>
#include <DFRobot_ENS160.h>
#include <DFRobot_MultiGasSensor.h>
#include <SparkFun_SCD30_Arduino_Library.h>

namespace {
constexpr uint8_t Bh1750AddressLow = 0x23;
constexpr uint8_t Bh1750AddressHigh = 0x5C;
constexpr uint8_t Bh1750PowerOn = 0x01;
constexpr uint8_t Bh1750ContinuousHighResolution = 0x10;
constexpr uint8_t SoundSensorPin = A0;
constexpr unsigned long MeasurementWaitMs = 180;
constexpr unsigned long SampleIntervalMs = 2000;

uint8_t bh1750Address = 0;
unsigned long lastSampleTime = 0;
bool scd30Ready = false;
bool ens160Ready = false;
bool hydrogenReady = false;
bool oxygenReady = false;

SCD30 scd30;
DFRobot_ENS160_I2C ens160(&Wire, 0x52);
DFRobot_GAS_I2C hydrogenSensor(&Wire, 0x74);
DFRobot_GAS_I2C oxygenSensor(&Wire, 0x77);

struct ExpectedI2cDevice {
  uint8_t address;
  const char *name;
};

const ExpectedI2cDevice expectedDevices[] = {
    {0x52, "SEN0514 / ENS160"},
    {0x54, "BMV080 (address option)"},
    {0x55, "BMV080 (address option)"},
    {0x56, "BMV080 (address option)"},
    {0x57, "BMV080 (default address)"},
    {0x61, "SCD-30"},
    {0x74, "SEN0465 H2"},
    {0x77, "SEN0465 O2"},
};

bool deviceResponds(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void scanI2cBus() {
  Serial.println(F("Scanning I2C bus..."));
  uint8_t deviceCount = 0;

  for (uint8_t address = 1; address < 127; ++address) {
    if (deviceResponds(address)) {
      Serial.print(F("I2C device found at 0x"));
      if (address < 16) {
        Serial.print('0');
      }
      Serial.println(address, HEX);
      ++deviceCount;
    }
  }

  if (deviceCount == 0) {
    Serial.println(F("No I2C devices found."));
  } else {
    Serial.print(F("I2C devices found: "));
    Serial.println(deviceCount);
  }
}

void reportExpectedDevices() {
  Serial.println(F("Expected sensor check:"));

  const size_t deviceCount = sizeof(expectedDevices) / sizeof(expectedDevices[0]);
  for (size_t index = 0; index < deviceCount; ++index) {
    const ExpectedI2cDevice &device = expectedDevices[index];
    Serial.print(F("  "));
    Serial.print(device.name);
    Serial.print(F(" at 0x"));
    if (device.address < 16) {
      Serial.print('0');
    }
    Serial.print(device.address, HEX);
    Serial.print(F(": "));
    Serial.println(deviceResponds(device.address) ? F("FOUND") : F("NOT FOUND"));
  }

  const bool bh1750Found =
      deviceResponds(Bh1750AddressLow) || deviceResponds(Bh1750AddressHigh);
  Serial.print(F("  BH1750 at 0x23 or 0x5C: "));
  Serial.println(bh1750Found ? F("FOUND") : F("NOT FOUND"));
}

bool configureBh1750() {
  if (deviceResponds(Bh1750AddressLow)) {
    bh1750Address = Bh1750AddressLow;
  } else if (deviceResponds(Bh1750AddressHigh)) {
    bh1750Address = Bh1750AddressHigh;
  } else {
    return false;
  }

  Wire.beginTransmission(bh1750Address);
  Wire.write(Bh1750PowerOn);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  Wire.beginTransmission(bh1750Address);
  Wire.write(Bh1750ContinuousHighResolution);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  return true;
}

bool readBh1750(float &lux) {
  Wire.requestFrom(bh1750Address, static_cast<uint8_t>(2));
  if (Wire.available() != 2) {
    return false;
  }

  const uint16_t rawReading =
      (static_cast<uint16_t>(Wire.read()) << 8) | Wire.read();
  lux = static_cast<float>(rawReading) / 1.2F;
  return true;
}

void initializeAdvancedSensors() {
  scd30Ready = scd30.begin(false);
  Serial.print(F("SCD-30 driver: "));
  Serial.println(scd30Ready ? F("READY") : F("FAILED"));

  const int ens160Status = ens160.begin();
  ens160Ready = ens160Status == 0;
  Serial.print(F("SEN0514 driver: "));
  Serial.println(ens160Ready ? F("READY") : F("FAILED"));
  if (ens160Ready) {
    ens160.setPWRMode(ENS160_STANDARD_MODE);
  }

  hydrogenReady = hydrogenSensor.begin();
  Serial.print(F("SEN0465 H2 driver: "));
  Serial.println(hydrogenReady ? F("READY") : F("FAILED"));
  if (hydrogenReady) {
    hydrogenSensor.changeAcquireMode(hydrogenSensor.PASSIVITY);
    hydrogenSensor.setTempCompensation(hydrogenSensor.ON);
  }

  oxygenReady = oxygenSensor.begin();
  Serial.print(F("SEN0465 O2 driver: "));
  Serial.println(oxygenReady ? F("READY") : F("FAILED"));
  if (oxygenReady) {
    oxygenSensor.changeAcquireMode(oxygenSensor.PASSIVITY);
    oxygenSensor.setTempCompensation(oxygenSensor.ON);
  }
}

void printAdvancedSensorReadings() {
  if (scd30Ready) {
    if (scd30.dataAvailable() && scd30.readMeasurement()) {
      Serial.print(F("SCD-30 CO2 ppm: "));
      Serial.print(scd30.getCO2());
      Serial.print(F(", temperature C: "));
      Serial.print(scd30.getTemperature(), 2);
      Serial.print(F(", humidity %: "));
      Serial.println(scd30.getHumidity(), 2);
    } else {
      Serial.println(F("SCD-30: waiting for measurement"));
    }
  }

  if (ens160Ready) {
    Serial.print(F("SEN0514 AQI: "));
    Serial.print(ens160.getAQI());
    Serial.print(F(", TVOC ppb: "));
    Serial.print(ens160.getTVOC());
    Serial.print(F(", eCO2 ppm: "));
    Serial.println(ens160.getECO2());
  }

  if (hydrogenReady) {
    Serial.print(F("SEN0465 H2 ppm: "));
    Serial.print(hydrogenSensor.readGasConcentrationPPM(), 2);
    Serial.print(F(", temperature C: "));
    Serial.println(hydrogenSensor.readTempC(), 2);
  }

  if (oxygenReady) {
    Serial.print(F("SEN0465 O2: "));
    Serial.print(oxygenSensor.readGasConcentrationPPM(), 2);
    Serial.print(F(" percent, temperature C: "));
    Serial.println(oxygenSensor.readTempC(), 2);
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  Wire.begin();

  delay(100);
  Serial.println(F("Environmental sensing module"));
  Serial.println(F("RedBoard Plus / BH1750 I2C test"));
  scanI2cBus();
  reportExpectedDevices();

  if (configureBh1750()) {
    Serial.print(F("BH1750 configured at 0x"));
    Serial.println(bh1750Address, HEX);
  } else {
    Serial.println(F("BH1750 not detected at 0x23 or 0x5C."));
    Serial.println(F("Check Qwiic connection, power, and I2C voltage."));
  }

  pinMode(SoundSensorPin, INPUT);
  Serial.print(F("SEN0232 analog sound sensor on A0: "));
  Serial.println(analogRead(SoundSensorPin));
  initializeAdvancedSensors();
}

void loop() {
  if (bh1750Address == 0) {
    delay(2000);
    scanI2cBus();
    configureBh1750();
    return;
  }

  if (millis() - lastSampleTime >= SampleIntervalMs) {
    lastSampleTime = millis();
    delay(MeasurementWaitMs);
    float lux = 0.0F;
    if (readBh1750(lux)) {
      Serial.print(F("BH1750 lux: "));
      Serial.println(lux, 2);
    } else {
      Serial.println(F("BH1750 read failed."));
    }

    Serial.print(F("SEN0232 A0: "));
    Serial.println(analogRead(SoundSensorPin));
    printAdvancedSensorReadings();
  }
}
