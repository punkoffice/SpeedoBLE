
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

// NKolban's original library has a setServiceSolicitation method. As of writing, this is not in the
// Espressif core libs. If you are using NKolban's branch of the library, or if this moves into
// the core libraries eventually, uncomment this.
//#define BLE_LIB_HAS_SERVICE_SOLICITATION 

#include "esp32notifications.h"
#include "ancs_ble_client.h"
#include "ble_security.h"

#include "BLEAddress.h"
#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEClient.h"
#include "BLEUtils.h"
#include "BLE2902.h"

#include <esp_gatts_api.h>

static char LOG_TAG[] = "BLENotifications";

typedef struct {
	BLENotifications *instance;
	BLEAddress *address;
} pinnedTaskClosure_t;

extern const BLEUUID ancsServiceUUID;
Speedo *speedo = Speedo::GetInstance();

#ifndef BLE_LIB_HAS_SERVICE_SOLICITATION
// Use a static function, instead of doing a whole private implementation just for a this one small patch.
static void setServiceSolicitation(class BLEAdvertisementData & advertisementData, BLEUUID uuid);
#endif
void startClientTasks(void * params) {
	pinnedTaskClosure_t *taskData = (pinnedTaskClosure_t *)params;

	BLEClient* pClient  = BLEDevice::createClient();
	BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
	BLEDevice::setSecurityCallbacks(new NotificationSecurityCallbacks()); // @todo memory leak?

	BLESecurity *pSecurity = new BLESecurity();
	pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
	pSecurity->setCapability(ESP_IO_CAP_IO);
	pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
	// Connect to the remove BLE Server.
	Serial.print("Address: ");
	BLEAddress addr = *taskData->address;
	Serial.println(addr.toString().c_str());
	pClient->connect(*taskData->address);
	taskData->instance->clientANCS->setup(pClient);
	speedo->ANCSservice = taskData->instance->clientANCS->remoteService;
	speedo->client = pClient;
	::xTaskCreatePinnedToCore(&ANCSBLEClient::startClientTask, "ClientTask", 10000, pClient, 5, &taskData->instance->clientANCS->clientTaskHandle, 0);
	delete taskData->address;
	delete taskData;
	vTaskDelete(NULL);
}

class MyServerCallbacks: public BLEServerCallbacks {
private:
    struct gatts_connect_evt_param { // @todo include from sdk/include/bt/esp_gatts_api.h
        uint16_t conn_id;               /*!< Connection id */
        esp_bd_addr_t remote_bda;       /*!< Remote bluetooth device address */
    } connect; 
	
public:
	BLENotifications * instance;
	
	MyServerCallbacks(BLENotifications * parent)
	 	: instance(parent) {
        instance->clientANCS = new ANCSBLEClient(); // @todo memory leaks?
	}
	
    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) {
		ESP_LOGI(LOG_TAG, "Device connected");
		Serial.println("Connected");
		gatts_connect_evt_param * connectEventParam = (gatts_connect_evt_param *) param;

		pinnedTaskClosure_t *taskData = new pinnedTaskClosure_t();  // @todo memory leaks?
		BLEAddress *address = new BLEAddress(connectEventParam->remote_bda);
		taskData->address = address;
		taskData->instance = instance;

	    ::xTaskCreatePinnedToCore(&startClientTasks, "ClientStarterTask", 10000, taskData, 5, &instance->clientANCS->clientTaskHandle, 0);
		
		delay(2000);
		
		ESP_LOGI(LOG_TAG, "Set up client");
		
        if (instance->cbStateChanged) {
        	instance->cbStateChanged(BLENotifications::StateConnected, instance->cbStateChangedUserData);
        }
    };

	void onDisconnect(BLEServer* pServer) {
		  ::vTaskDelete(instance->clientANCS->clientTaskHandle);
			Serial.println("Disconnected");
		  instance->clientANCS->clientTaskHandle = nullptr;
			ESP_LOGI(LOG_TAG, "Device disconnected");
	        if (instance->cbStateChanged) {
	        	instance->cbStateChanged(BLENotifications::StateDisconnected, instance->cbStateChangedUserData);
	        }
			delete instance->clientANCS;
			instance->clientANCS = nullptr;
	    }
};


BLENotifications::BLENotifications() {
}

const char * BLENotifications::getNotificationCategoryDescription(NotificationCategory category) const {
	switch(category) { 
    case CategoryIDOther: return "other";
	case CategoryIDIncomingCall: return "incoming call";
	case CategoryIDMissedCall: return "missed call";
	case CategoryIDVoicemail: return "voicemail";
	case CategoryIDSocial: return "social";
	case CategoryIDSchedule: return "schedule";
	case CategoryIDEmail: return "email";
	case CategoryIDNews: return "news";
	case CategoryIDHealthAndFitness: return "health and fitness";
	case CategoryIDBusinessAndFinance: return "business and finance";
	case CategoryIDLocation: return "location";
	case CategoryIDEntertainment: return "entertainment";
	default: return "unknown";
}
}

void BLENotifications::begin(const char * name) {
	ESP_LOGI(LOG_TAG, "begin()");
    BLEDevice::init(name);
    server = BLEDevice::createServer();
    server->setCallbacks(new MyServerCallbacks(this));
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
	BLEDevice::setSecurityCallbacks(new NotificationSecurityCallbacks()); // @todo memory leak?
	Serial.println("Server");
	speedo->setup(server);
	startAdvertising();
}

void BLENotifications::stop() {
	ESP_LOGI(LOG_TAG, "stop()");
	BLEDevice::deinit(false);	
}

void BLENotifications::setConnectionStateChangedCallback(ble_notifications_state_changed_t callback, const void *userData) {
	cbStateChanged = callback;
	cbStateChangedUserData = userData;
}

void BLENotifications::setNotificationCallback(ble_notification_arrived_t callback, const void *userData) {
	clientANCS->setNotificationArrivedCallback(callback, userData);
}

void BLENotifications::setRemovedCallback(ble_notification_removed_t callback, const void *userData) {
	clientANCS->setNotificationRemovedCallback(callback, userData);
}

void BLENotifications::actionPositive(uint32_t uuid) {
	ESP_LOGI(LOG_TAG, "actionPositive()");
	clientANCS->performAction(uuid, uint8_t(ANCS::NotificationActionPositive));
}

void BLENotifications::actionNegative(uint32_t uuid) {
	ESP_LOGI(LOG_TAG, "actionNegative()");
	clientANCS->performAction(uuid, uint8_t(ANCS::NotificationActionNegative));
}

void BLENotifications::startAdvertising() {
	ESP_LOGI(LOG_TAG, "startAdvertising()");
	Serial.println("Advertising");
    // Start soliciting the Apple ANCS service and make the device visible to searches on iOS (from Apple ANCS documentation)
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
	
	if (isAdvertising == true) {
		// Stopping it without checking first seems to cause failures to advertise without debugging on.
		// There is no way to query the BLEAdvertising object to see if it is advertising, so we keep a variable.
		pAdvertising->stop();
	}

    BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
    oAdvertisementData.setFlags(0x01);
	
#ifdef BLE_LIB_HAS_SERVICE_SOLICITATION
    oAdvertisementData.setServiceSolicitation(ANCSBLEClient::getAncsServiceUUID()); 
#else
	setServiceSolicitation(oAdvertisementData, ANCSBLEClient::getAncsServiceUUID());
#endif
	
    pAdvertising->setAdvertisementData(oAdvertisementData);        

    // Set security
    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
    pSecurity->setCapability(ESP_IO_CAP_OUT);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    //Start advertising
    pAdvertising->start();
	isAdvertising = true;
}


#ifndef BLE_LIB_HAS_SERVICE_SOLICITATION
void setServiceSolicitation(BLEAdvertisementData & advertisementData, BLEUUID uuid)
{
	char cdata[2];
	switch(uuid.bitSize()) {
		case 16: {
			// [Len] [0x14] [UUID16] data
			cdata[0] = 3;
			cdata[1] = ESP_BLE_AD_TYPE_SOL_SRV_UUID;  // 0x14
			advertisementData.addData(std::string(cdata, 2) + std::string((char *)&uuid.getNative()->uuid.uuid16,2));
			break;
		}

		case 128: {
			// [Len] [0x15] [UUID128] data
			cdata[0] = 17;
			cdata[1] = ESP_BLE_AD_TYPE_128SOL_SRV_UUID;  // 0x15
			advertisementData.addData(std::string(cdata, 2) + std::string((char *)uuid.getNative()->uuid.uuid128,16));
			break;
		}

		default:
			return;
	}
}
#endif

