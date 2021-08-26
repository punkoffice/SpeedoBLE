#include "motion.h"
#include <Wire.h>
#include "bma.h"

#define MOTION_TRIGGER 1200
#define SHAKE_DISTANCE 500

RTC_DATA_ATTR BMA423 sensor;

uint16_t wristFlipCount = 0;
int16_t previousAccZ;
bool isSetup = false;

uint16_t _readRegister(uint8_t address, uint8_t reg, uint8_t *data, uint16_t len)
{
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)address, (uint8_t)len);
    uint8_t i = 0;
    while (Wire.available()) {
        data[i++] = Wire.read();
    }
    return 0;
}

uint16_t _writeRegister(uint8_t address, uint8_t reg, uint8_t *data, uint16_t len)
{
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(data, len);
    return (0 !=  Wire.endTransmission());
}

void setupMotion() {
	if (isSetup) return;
	if (sensor.begin(_readRegister, _writeRegister, delay) == false) {
		//fail to init BMA
		return;
	}

	// Accel parameter structure
	Acfg cfg;
	cfg.odr = BMA4_OUTPUT_DATA_RATE_100HZ;
	cfg.range = BMA4_ACCEL_RANGE_2G;
	cfg.bandwidth = BMA4_ACCEL_NORMAL_AVG4;
	cfg.perf_mode = BMA4_CONTINUOUS_MODE;

	// Configure the BMA423 accelerometer
	sensor.setAccelConfig(cfg);

	// Enable BMA423 accelerometer
	// Warning : Need to use feature, you must first enable the accelerometer
	sensor.enableAccel();
	isSetup = true;
	struct bma4_int_pin_config config ;
	config.edge_ctrl = BMA4_LEVEL_TRIGGER;
	config.lvl = BMA4_ACTIVE_HIGH;
	config.od = BMA4_PUSH_PULL;
	config.output_en = BMA4_OUTPUT_ENABLE;
	config.input_en = BMA4_INPUT_DISABLE;
	// The correct trigger interrupt needs to be configured as needed
	sensor.setINTPinConfig(config, BMA4_INTR1_MAP);

	struct bma423_axes_remap remap_data;
	remap_data.x_axis = 1;
	remap_data.x_axis_sign = 0xFF;
	remap_data.y_axis = 0;
	remap_data.y_axis_sign = 0xFF;
	remap_data.z_axis = 2;
	remap_data.z_axis_sign = 0xFF;

	// Need to raise the wrist function, need to set the correct axis
	sensor.setRemapAxes(&remap_data);
	Accel acc;
	bool res = sensor.getAccel(acc);
	if (!res) {
		previousAccZ = acc.z;
	}
}

bool didShake() {
	Accel acc;
	bool res = sensor.getAccel(acc);
	bool didShake;
	if (!res) {
		Serial.println("getAccel FAIL");
		return false;
	} else {
		int16_t currentAccZ = acc.z;
		didShake = false;
		int16_t diff = abs(currentAccZ - previousAccZ);
		switch(wristFlipCount) {
			case 0:
				if (diff > SHAKE_DISTANCE) wristFlipCount++;
				break;
			case 1:
				if ((currentAccZ < 0) && (diff > SHAKE_DISTANCE)) {
					wristFlipCount++;
					didShake = true;
				}
				wristFlipCount = 0;				
				break;
		}
		previousAccZ = currentAccZ;
		return (didShake);
	}
}