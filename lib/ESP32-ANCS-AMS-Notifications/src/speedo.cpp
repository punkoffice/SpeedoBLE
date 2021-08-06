#include "speedo.h"
#include "fonts/Montserrat_Bold_45.h"
#include "fonts/DSEG7_Classic_Bold_25.h"
#include <BLECharacteristic.h>
#include <BLEDevice.h>
#include <DS3232RTC.h>

#define YEAR_OFFSET 1970
#define MUTEX_WAIT 50

// battery
const uint8_t BATTERY_SEGMENT_WIDTH = 7;
const uint8_t BATTERY_SEGMENT_HEIGHT = 11;
const uint8_t BATTERY_SEGMENT_SPACING = 9;

// 'battery', 37x21px
const unsigned char battery [] PROGMEM = {
	0x3f, 0xff, 0xff, 0xff, 0x80, 0x7f, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xe0, 
	0x00, 0x00, 0x00, 0xe0, 0xe0, 0x00, 0x00, 0x00, 0xe0, 0xe0, 0x00, 0x00, 0x00, 0xf8, 0xe0, 0x00, 
	0x00, 0x00, 0xf8, 0xe0, 0x00, 0x00, 0x00, 0x38, 0xe0, 0x00, 0x00, 0x00, 0x38, 0xe0, 0x00, 0x00, 
	0x00, 0x38, 0xe0, 0x00, 0x00, 0x00, 0x38, 0xe0, 0x00, 0x00, 0x00, 0x38, 0xe0, 0x00, 0x00, 0x00, 
	0x38, 0xe0, 0x00, 0x00, 0x00, 0x38, 0xe0, 0x00, 0x00, 0x00, 0xf8, 0xe0, 0x00, 0x00, 0x00, 0xf8, 
	0xe0, 0x00, 0x00, 0x00, 0xe0, 0xe0, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x7f, 
	0xff, 0xff, 0xff, 0xc0, 0x3f, 0xff, 0xff, 0xff, 0x80
};

#define ADC_PIN 33

// Singleton setup
Speedo* Speedo::singleton = nullptr;
Speedo *Speedo::GetInstance() {
	if (singleton == nullptr) {
		singleton = new Speedo();		
	}
	return singleton;
}

int currentSpeed = 0;
int maxSpeed = 0;
bool isTimeSet = false;
DS3232RTC RTC; 

String getValue(String data, char separator, int index) {
	int found = 0;
	int strIndex[] = {0, -1};
	int maxIndex = data.length()-1;

	for(int i=0; i<=maxIndex && found<=index; i++) {
		if (data.charAt(i)==separator || i==maxIndex){
			found++;
			strIndex[0] = strIndex[1]+1;
			strIndex[1] = (i == maxIndex) ? i+1 : i;
		}
	}
	return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

class callbackSpeed: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
		std::string value = pCharacteristic->getValue();
		if (value.length() > 0) {
			String strValue = value.c_str();
			int intValue = strValue.toInt();
			if (intValue > maxSpeed) {
				maxSpeed = intValue;
			}
			currentSpeed = intValue;
		}
    }
};

class callbackTime: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
		std::string value = pCharacteristic->getValue();
		if (value.length() > 0) {
			String strValue = value.c_str();
			if (strValue.length() > 0) {
				Serial.print("Time: ");
				if (strValue.compareTo("0") == 0) {
					Speedo *singleton = Speedo::GetInstance();
					singleton->disconnect();
				}
				for (int i = 0; i < strValue.length(); i++) {
					Serial.print(value[i]);
				}
				Serial.println();
				tmElements_t tm;
				tm.Year = getValue(strValue, ':', 0).toInt() - YEAR_OFFSET;//offset from 1970, since year is stored in uint8_t        
				tm.Month = getValue(strValue, ':', 1).toInt();
				tm.Day = getValue(strValue, ':', 2).toInt();
				tm.Hour = getValue(strValue, ':', 3).toInt();
				tm.Minute = getValue(strValue, ':', 4).toInt();
				tm.Second = getValue(strValue, ':', 5).toInt();

				time_t t = makeTime(tm);
				RTC.begin();
				delay(1000);
				RTC.set(t);
				isTimeSet = true;
			}
		}
    }
};

void Speedo::drawString(int x, int y, String text, alignment align) {
	int16_t x1, y1; //the bounds of x,y and w and h of the variable ‘text’ in pixels.
	uint16_t w, h;
	display->setTextWrap(false);
	display->getTextBounds(text, x, y, &x1, &y1, &w, &h);
	if (align == RIGHT) x = x - w;
	if (align == CENTER) x = x - w / 2;
	display->setCursor(x, y + h);
	display->print(text);
}

void Speedo::disconnect() {
	Serial.println("Forcing disconnection");
	//client->disconnect();
}

void Speedo::draw() {
	String strCurrentSpeed = String(currentSpeed);
	int n = strCurrentSpeed.length();  
	char char_array[n + 1];
	strcpy(char_array, strCurrentSpeed.c_str());
	display->fillScreen(GxEPD_WHITE);
	display->setTextColor(GxEPD_BLACK);
	display->setFont(&Montserrat_Bold72pt7b);
	drawString(DISPLAY_WIDTH/2, 10, char_array, CENTER);
	printMaxSpeed();
	printTime();
	drawBattery(false);
	display->display(true);
}

void Speedo::printMaxSpeed() {
	String strMaxSpeed = String(maxSpeed);
	int n = strMaxSpeed.length();  
	char char_array[n + 1];
	strcpy(char_array, strMaxSpeed.c_str());
	display->setFont(&Montserrat_Bold45pt7b);
	drawString(194, 132, char_array, RIGHT);
}

void Speedo::printTime() {
	if (isTimeSet) {
		time_t t = RTC.get();
		struct tm* p_tm = localtime(&t);
		display->setFont(&DSEG7_Classic_Bold_25);
		display->setCursor(0, 195);	
		int hour = p_tm->tm_hour;
		if (hour > 12) hour -= 12;
		display->print(hour);
		display->print(":");
		if (p_tm->tm_min < 10){
			display->print("0");
		}  
		display->println(p_tm->tm_min);    
	}
}

float Speedo::getBatteryVoltage() {
    return analogRead(ADC_PIN) / 4096.0 * 7.23;
}

void Speedo::drawBattery(bool isWaiting) {
	int16_t offset = 0;
	if (isWaiting) offset = 50;
    display->drawBitmap(34 + offset, 134, battery, 37, 21, GxEPD_BLACK);
    display->fillRect(39 + offset, 139, 27, BATTERY_SEGMENT_HEIGHT, GxEPD_WHITE); //clear battery segments
    int8_t batteryLevel = 0;
    float VBAT = getBatteryVoltage();
    if (VBAT > 4.1){
        batteryLevel = 3;
    }
    else if (VBAT > 3.95 && VBAT <= 4.1){
        batteryLevel = 2;
    }
    else if (VBAT > 3.80 && VBAT <= 3.95){
        batteryLevel = 1;
    }    
    else if (VBAT <= 3.80){
        batteryLevel = 0;
    }

    for (int8_t batterySegments = 0; batterySegments < batteryLevel; batterySegments++) {
        display->fillRect(39 + offset + (batterySegments * BATTERY_SEGMENT_SPACING), 139, BATTERY_SEGMENT_WIDTH, BATTERY_SEGMENT_HEIGHT, GxEPD_BLACK);
    }
}

void Speedo::init() {
	isTimeSet = false;
	display->init(0, true);
	display->setFullWindow();
}

void Speedo::setup(BLEServer *pServer) {
	server = pServer;
	BLEService *pService = pServer->createService(SpeedoService_UUID );

	BLECharacteristic *pCharacteristicSpeed = pService->createCharacteristic(
											SpeedoCharacteristic_Speed_UUID,
											BLECharacteristic::PROPERTY_WRITE
										);
	pCharacteristicSpeed->setCallbacks(new callbackSpeed());

	BLECharacteristic *pCharacteristicTime = pService->createCharacteristic(
											SpeedoCharacteristic_Time_UUID,
											BLECharacteristic::PROPERTY_WRITE
										);										
	pCharacteristicTime->setCallbacks(new callbackTime());
	pService->start();
}

void Speedo::sleep() {
	display->fillScreen(GxEPD_WHITE);
	display->setTextColor(GxEPD_BLACK);
	display->setFont(&FreeSansBold12pt7b);
	drawString(DISPLAY_WIDTH/2, 90, "Sleeping...", CENTER);
	drawBattery(true);
	display->display(true);	
	esp_sleep_enable_ext1_wakeup(BTN_PIN_MASK, ESP_EXT1_WAKEUP_ANY_HIGH); //enable deep sleep wake on button press
  	esp_deep_sleep_start();	
}