/*
  Copyright (c) 2014-2020 Electronic Cats SAPI de CV.  All right reserved.
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <esp32notifications.h>
#include "button_interrupt.h"
#include "watchy_hw.h"
#include "ble_notification.h"
#include "watchy_display_state.h"

// typedef GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display_t;
display_t display(GxEPD2_154_D67(WATCHY_DISPLAY_CS_PIN, WATCHY_DISPLAY_DC_PIN, WATCHY_DISPLAY_RESET_PIN, WATCHY_DISPLAY_BUSY_PIN));
BLENotifications notifications;
Speedo *mainSpeedo = Speedo::GetInstance();
WatchyDisplayState displayState(display, notifications);

void onBLEStateChanged(BLENotifications::State state, const void *userData)
{
  switch (state)
  {
  case BLENotifications::StateConnected:
    Serial.println("StateConnected - connected to a phone or tablet");
	mainSpeedo->alreadyPaired = true;
    break;

  case BLENotifications::StateDisconnected:
    Serial.println("StateDisconnected - disconnected from a phone or tablet");
    //notifications.startAdvertising();
    ESP.restart(); // not sure why advertising restart doesnt work
    break;
  }
  displayState.setConnected(state == BLENotifications::StateConnected);
}

void onNotificationArrived(const Notification *rawData, const void *userData) {
	displayState.setANCSNotification(rawData);
}

void onNotificationRemoved(const Notification *rawData, const void *userData) {
  	displayState.clearANCSNotification(rawData->uuid);
}

void setup() {
	mainSpeedo->display = &display;	

	Serial.begin(115200);
	Serial.println("Starting BLE work!");
	notifications.begin(WATCHY_BLE_NAME);
	notifications.setConnectionStateChangedCallback(onBLEStateChanged);
	notifications.setNotificationCallback(onNotificationArrived);
	notifications.setRemovedCallback(onNotificationRemoved);

	buttonSetup(WATCHY_BUTTON_1_PIN, 0);
	buttonSetup(WATCHY_BUTTON_2_PIN, 1);
	buttonSetup(WATCHY_BUTTON_3_PIN, 2);
	buttonSetup(WATCHY_BUTTON_4_PIN, 3);
	mainSpeedo->init();
	displayState.updateDisplay();
}

void loop() {
	uint8_t presses = buttonGetPressMask();
	for (int i = 0; i < 4; i++)
	if (buttonWasPressed(presses, i)) {
		ESP_LOGI(LOG_TAG, "Button %d press", i + 1);
		switch(i) {
			case 1:
				if (displayState.currentState == watchState::speedo) {
					displayState.startDistanceTimer();
					displayState.currentState = watchState::distance;
				} else if (displayState.currentState == watchState::distance) {
					displayState.endDistanceTimer();
				}
				break;
			case 2:
				if (displayState.currentState == watchState::speedo) {
					mainSpeedo->resetMaxSpeed();
				} else if (displayState.currentState == watchState::distance) {
					mainSpeedo->resetDistance();
				}
				break;
		}
	}
	displayState.handleButtonPress(presses);
	displayState.updateDisplay();
	if (displayState.currentState == watchState::waiting) {
		delay(1000);
		mainSpeedo->tick++;
		if (mainSpeedo->tick >= SECONDS_TILL_SLEEP) mainSpeedo->sleep();
	} else {
		delay(50);
	}
}
