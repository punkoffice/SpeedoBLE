#ifndef WATCHY_DISPLAY_STATE_H__
#define WATCHY_DISPLAY_STATE_H__

#include "watchy_hw.h"
#include "button_interrupt.h"
#include "motion.h"
#include <esp32notifications.h>
#include <stdint.h>

enum watchState {waiting, speedo, distance, notification};

class WatchyDisplayState {
private:
	display_t &display;
	BLENotifications &notifications;
	Speedo *speedo;
	bool firstDisplay = true;
	uint32_t needsUpdate = 1;
	void invalidate() {
	if (!needsUpdate)
		needsUpdate = millis();
	}
	void invalidateImmediate() {
		needsUpdate = millis() - SCREEN_UPDATE_DEBOUNCE_PERIOD_MS - 1;
	}

	bool bleConnected = false;

	// ANCS state
	uint32_t ancsUUID = 0;
	uint32_t ancsShowTimeMS = 0;
	std::string ancsBody;
	std::string ancsTitle;
	ANCS::EventFlags ancsFlags = (ANCS::EventFlags)0;
	bool isShowingANCSNotification() { return ancsShowTimeMS != 0; }

	uint32_t distanceShowTimeMS = 0;
private:
	#define ALIGN_NEAR 0
	#define ALIGN_MIDDLE 1
	#define ALIGN_FAR 2
	#define VIB_MOTOR_PIN 13

	String wordwrapped(const char *text) {
		int16_t x1, y1;
		uint16_t w, h;
		char *mytext = strdup(text);
		char *token;
		token = strtok(mytext, " ");
		String wholeText = "";
		String currentLine = "";
		bool isNewLine = true;
		while (token != NULL) {
			String strWord = String(token);
			if (!isNewLine) currentLine += " ";
			isNewLine = false;
			currentLine += strWord;
			display.getTextBounds(currentLine, 0, 0, &x1, &y1, &w, &h);
			if (w >= 198) {
				currentLine = "";
				wholeText += "\n";
				isNewLine = true;
			} else {
				if (!isNewLine) wholeText += " ";
				wholeText += strWord;
				token = strtok(NULL," ");
			}
		}
		return wholeText;
	}

	uint16_t alignText(const GFXfont *font, const char *text, int halign, int valign) {
		// @todo this is terrible at multiline alignment
		display.setFont(font);
		int16_t x, y;
		int16_t x1, y1;
		uint16_t w, h;
		display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
		x = halign == ALIGN_NEAR ? 0 : (halign == ALIGN_FAR ? WATCHY_DISPLAY_WIDTH - w - 4 : (WATCHY_DISPLAY_WIDTH - w) / 2);
		y = (valign == ALIGN_NEAR ? 0 : (valign == ALIGN_FAR ? WATCHY_DISPLAY_HEIGHT - h - 4 : (WATCHY_DISPLAY_HEIGHT - h) / 2)) - y1;
		display.setCursor(x, y);
		display.println(text);
		return h;
	}

	void drawButtons(const char *button1, const char *button2, const char *button3, const char *button4) {
	if (button1 != nullptr)
		alignText(&FONT_BUTTONS, button1, ALIGN_NEAR, ALIGN_FAR);
	if (button2 != nullptr)
		alignText(&FONT_BUTTONS, button2, ALIGN_NEAR, ALIGN_NEAR);
	if (button3 != nullptr)
		alignText(&FONT_BUTTONS, button3, ALIGN_FAR, ALIGN_NEAR);
	if (button4 != nullptr)
		alignText(&FONT_BUTTONS, button4, ALIGN_FAR, ALIGN_FAR);
	}

	void alignTextTest(const GFXfont *font = &FONT_BIG);

	void drawScreenNoBLE() {
		display.fillScreen(GxEPD_WHITE);
		display.setTextColor(GxEPD_BLACK);
		display.setFont(&FreeSansBold12pt7b);
		speedo->drawString(DISPLAY_WIDTH/2, 90, "Waiting...", CENTER);
		speedo->drawBattery(true);
		display.display(!firstDisplay);
		firstDisplay = false;
	}

	void drawScreenANCSNotification() {
		display.fillScreen(GxEPD_WHITE);
		display.setTextColor(GxEPD_BLACK);
		drawButtons(nullptr, nullptr,
			(ancsFlags & ANCS::EventFlagPositiveAction) != 0 ? "V" : nullptr,
			(ancsFlags & ANCS::EventFlagNegativeAction) != 0 ? "X" : nullptr);
		display.setFont(&FONT_BIG_BOLD);
		display.setCursor(0,15);
		display.println(ancsTitle.c_str());
		display.setFont(&FONT_BIG);
		String wrappedBody = wordwrapped(ancsBody.c_str());
		display.setCursor(0,36);
		display.println(wrappedBody);
		display.display(!firstDisplay);
	}

public:
	watchState currentState = watchState::waiting;

	void updateDisplay() {
    	needsUpdate = 0;

		switch(currentState) {
			case watchState::waiting:
				drawScreenNoBLE();
				break;
			case watchState::speedo:
				if (didShake()) {
					if (ancsTitle.length() > 0) {
						ANCSVibrate();
						currentState = watchState::notification;
						ancsShowTimeMS = millis() | 1;
					} else {
						speedo->showSpeed();
					}
				} else {
					speedo->showSpeed();
				}
				break;
			case watchState::notification:
				drawScreenANCSNotification();
				checkANCSNotificationTimeout();
				break;
			case watchState::distance:
				speedo->showDistance();
				checkDistanceTimeout();
				break;
		}
	};

	WatchyDisplayState(display_t &display, BLENotifications &notifications) : display(display), notifications(notifications) {
		speedo = Speedo::GetInstance();
		ancsBody = "";
		ancsTitle = "";
		setupMotion();
	};
  	~WatchyDisplayState(){};

	void setConnected(const bool isConnected) {
		if (bleConnected == isConnected)
		return;
		currentState = watchState::speedo;
		bleConnected = isConnected;
		invalidate();
	}

	void clearANCSNotification(uint32_t uuid) {
		if (uuid != ancsUUID)
		return;
		ancsUUID = 0;
		ancsShowTimeMS = 0;
		ancsFlags = (ANCS::EventFlags)0;
		currentState = watchState::speedo;
		// Need to get the screen updated fast because the buttons
		// won't reflect the right action on screen until it does,
		// but their behaviour is immediately changed
		invalidateImmediate();
	}

	void clearANCSNotification() { clearANCSNotification(ancsUUID); }

	void startDistanceTimer() {
		distanceShowTimeMS = millis() | 1;
	}
	
	void setANCSNotification(const Notification *notification) {
    	if ((notification->eventFlags & (ANCS::EventFlagPreExisting | ANCS::EventFlagSilent)) != 0)
    		return;
		currentState = watchState::notification;
		ancsUUID = notification->uuid;
		ancsShowTimeMS = millis() | 1;
		ancsFlags = (ANCS::EventFlags)notification->eventFlags;
		std::string notifType = notification->type;
		if (notifType.length() > 0) {
			uint32_t lastDotIndex = notifType.find_last_of('.', notifType.length() - 1);
			if (lastDotIndex != std::string::npos)
				notifType = notifType.substr(lastDotIndex + 1);
		}
		ancsTitle = notification->title;
		ancsBody = notification->message + "\n(" + notifType + ")";
		ANCSVibrate();
		invalidateImmediate();
	}

	void checkANCSNotificationTimeout() {
		if (!isShowingANCSNotification())
		return;
		uint32_t now = millis();
		if ((now - ancsShowTimeMS) > ANCS_NOTIFICATION_DELAY_MS || (now < ancsShowTimeMS))
			clearANCSNotification();
	}

	void checkDistanceTimeout() {
		if (currentState != watchState::distance) return;
		uint32_t now = millis();
		if ((now - distanceShowTimeMS) > DISTANCE_NOTIFICATION_DELAY_MS || (now < ancsShowTimeMS))
			currentState = watchState::speedo;
	}

	void updateIfNeeded() {
		checkANCSNotificationTimeout();
		if (!needsUpdate)
			return;
		uint32_t now = millis();
		if ((now - needsUpdate) > SCREEN_UPDATE_DEBOUNCE_PERIOD_MS || (needsUpdate > now))
			updateDisplay();
	}

	void handleButtonPress(const uint8_t presses) {
		if (!bleConnected)
			return;
		if (!presses)
			return;
		if (isShowingANCSNotification()) {
			if (buttonWasPressed(presses, 0) || buttonWasPressed(presses, 1))
			clearANCSNotification();
			if (buttonWasPressed(presses, 2) && ((ancsFlags & ANCS::EventFlagPositiveAction) != 0))
			notifications.actionPositive(ancsUUID);
			if (buttonWasPressed(presses, 3) && ((ancsFlags & ANCS::EventFlagNegativeAction) != 0))
			notifications.actionNegative(ancsUUID);
		}
  	}

	void ANCSVibrate() {
		pinMode(VIB_MOTOR_PIN, OUTPUT);
		digitalWrite(VIB_MOTOR_PIN, true);
		delay(400);
		digitalWrite(VIB_MOTOR_PIN, false);
		delay(400);
		digitalWrite(VIB_MOTOR_PIN, true);
		delay(400);
		digitalWrite(VIB_MOTOR_PIN, false);
	}
};

#endif // WATCHY_DISPLAY_STATE_H__