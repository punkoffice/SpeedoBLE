#ifndef SPEEDO_H
#define SPEEDO_H

#include <config.h>
#include <BLEService.h>
#include <BLERemoteService.h>
#include <BLEClient.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include "fonts/Montserrat_Bold_72.h"

//pins
#define PIN_TOP_RIGHT 32

#define WATCHY_DISPLAY_CS_PIN     5
#define WATCHY_DISPLAY_DC_PIN    10
#define WATCHY_DISPLAY_RESET_PIN  9
#define WATCHY_DISPLAY_BUSY_PIN  19

#define DISPLAY_WIDTH 200
#define SECONDS_TILL_SLEEP 20

#define MENU_BTN_MASK GPIO_SEL_26
#define BACK_BTN_MASK GPIO_SEL_25
#define UP_BTN_MASK GPIO_SEL_32
#define DOWN_BTN_MASK GPIO_SEL_4

#define BTN_PIN_MASK MENU_BTN_MASK|BACK_BTN_MASK|UP_BTN_MASK|DOWN_BTN_MASK

//BLE services
#define SpeedoService_UUID        			"4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define SpeedoCharacteristic_Speed_UUID 	"beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define SpeedoCharacteristic_Time_UUID 		"ad0794ba-f0dc-11eb-9a03-0242ac130003"

enum alignment {LEFT, RIGHT, CENTER};

typedef GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display_t;

class Speedo {
	protected:
		static Speedo* singleton;
	private:
		void printMaxSpeed();
		void printTime();
		float getBatteryVoltage();
		BLEServer *server;
   	public:	
		void drawBattery(bool isWaiting);
		void drawString(int x, int y, String text, alignment align);
		void setup(BLEServer *pServer);
		void resetMaxSpeed();
		void disconnect();
		void sleep();
		void showSpeed();
		void showDistance();
		void init();
		int tick = 0;
		bool alreadyPaired = false;
		BLEClient *client;
		BLERemoteService *ANCSservice;
		display_t *display;
		static Speedo *GetInstance();
};

#endif