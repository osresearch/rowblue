#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino.h>

#define __le16(x) (x) // hack since esp32 is little endian
#define __le32(x) (x) // hack since esp32 is little endian

/*
static uint16_t __le16(uint16_t x)
{
	return 0
	| ((x >>  0) & 0xFF) << 8
	| ((x >>  8) & 0xFF) << 0
	;
}
static uint32_t __le32(uint32_t x)
{
	return 0
	| ((x >>  0) & 0xFF) << 24
	| ((x >>  8) & 0xFF) << 16
	| ((x >> 16) & 0xFF) <<  8
	| ((x >> 24) & 0xFF) <<  0
	;
}
*/

BLEServer * server;
BLEService * indoorBike;
BLEService * cadence;

// fitness machine service uuid, as defined in gatt specifications
#define fitnessMachineServiceUUID BLEUUID((uint16_t)0x1826)
BLECharacteristic indoorBikeDataCharacteristic(BLEUUID((uint16_t)0x2AD2), BLECharacteristic::PROPERTY_NOTIFY);

// cadence sensor
#define cadenceServiceUUID BLEUUID((uint16_t)0x1816)
BLECharacteristic cadenceMeasurementCharacteristic(BLEUUID((uint16_t)0x2A5B), BLECharacteristic::PROPERTY_NOTIFY);
BLECharacteristic cadenceFeatureCharacteristic(BLEUUID((uint16_t)0x2A5C), BLECharacteristic::PROPERTY_READ);

static const uint16_t indoorBikeFlags = 0
	| (1 << 0) // instaneous speed
	| (1 << 2) // instaneous cadence
	| (1 << 6) // instaneous power
	;

typedef struct
{
	uint16_t flags_le16;
	uint16_t speed_le16; // km/h / 1000
	uint16_t cadence_le16; // 0.5 rev/min
	int16_t power_le16; // watts?
} __attribute__((__packed__)) indoorBikeData_t;

static indoorBikeData_t indoorBikeData;

static const uint16_t cadence_feature = 0
	| (1 << 0) // wheel revolution data
	| (1 << 1) // crank revolution data
	;

typedef struct {
	uint8_t flags;
	uint32_t wheel_count_le32;
	uint16_t wheel_time_le16;
	uint16_t crank_count_le16;
	uint16_t crank_time_le16;
} __attribute__((__packed__)) cadenceMeasurement_t;

bool deviceConnected;

class MyServerCallbacks : public BLEServerCallbacks
{
private:
    void onConnect(BLEServer *pServer) {
	Serial.print("connected!");
	deviceConnected=true;
	}
    void onDisconnect(BLEServer *pServer) {
	Serial.print("disconnected");
	delay(500);
        server->startAdvertising();
    }
};

BLEDescriptor * BLE2901(const char * name)
{
	BLEDescriptor *descriptor = new BLEDescriptor("2901");
	descriptor->setValue((uint8_t*) name, strlen(name));
	return descriptor;
}

void indoorBike_setup()
{
        BLEDevice::init("BlueRow-143");
	server = BLEDevice::createServer();
        server->setCallbacks(new MyServerCallbacks());

	cadence = server->createService(cadenceServiceUUID);

	// create a notify characteristic for the measurement 
	// 2902 is required for any notify things. not sure what it means
	cadence->addCharacteristic(&cadenceMeasurementCharacteristic);
	cadenceMeasurementCharacteristic.addDescriptor(new BLE2902());
	cadenceMeasurementCharacteristic.addDescriptor(BLE2901("CSC Measurement"));

	// create a named characteristic with the name "CSC Feature" 
	// and the value of the configuration flag
	cadence->addCharacteristic(&cadenceFeatureCharacteristic);
	cadenceFeatureCharacteristic.addDescriptor(BLE2901("CSC Feature"));
	static uint8_t cadence_feature_le16[2];
	cadence_feature_le16[0] = (cadence_feature >> 0) & 0xFF;
	cadence_feature_le16[1] = (cadence_feature >> 8) & 0xFF;
	cadenceFeatureCharacteristic.setValue((uint8_t*) cadence_feature_le16, sizeof(cadence_feature_le16));
	cadence->start();


        // Start advertising
        BLEAdvertising *advertising = server->getAdvertising();
        //advertising->addServiceUUID(fitnessMachineServiceUUID);
        advertising->addServiceUUID(cadenceServiceUUID);
        advertising->setScanResponse(false);
        advertising->setMinPreferred(0x0); // set value to 0x00 to not advertise this parameter
        server->startAdvertising();
}

void indoorBike_publish(float speed, float cadence, float power)
{
	// ensure things are within limits
	if (speed < 0)
		speed = 0;
	if (cadence < 0)
		cadence = 0;

	// convert m/s to km/hour / 100
	uint16_t speed_16 = speed * 3600.0 / 1000.0 / 100.0;
	// convert to 0.5 rev/min
	uint16_t cadence_16 = cadence * 2;

	// convert across?
	int16_t power_16 = power;

	indoorBikeData.speed_le16 = __le16(speed_16);
	indoorBikeData.cadence_le16 = __le16(cadence_16);
	indoorBikeData.power_le16 = __le16(power_16);

	indoorBikeDataCharacteristic.setValue((uint8_t*) &indoorBikeData, sizeof(indoorBikeData));
	indoorBikeDataCharacteristic.notify();
}

void cadence_publish(unsigned stroke_count, unsigned last_stroke, unsigned wheel_count, unsigned last_wheel)
{
	static cadenceMeasurement_t data;
	data.flags = cadence_feature;
	data.wheel_count_le32 = __le32(wheel_count);
	data.wheel_time_le16 = __le16(last_wheel * (1024.0 / 1e6)); // usec to 1/1024 of a second
	data.crank_count_le16 = __le16(stroke_count);
	data.crank_time_le16 = __le16(last_stroke * (1024.0 / 1e6)); // usec to 1/1024 of a second

	cadenceMeasurementCharacteristic.setValue((uint8_t*) &data, sizeof(data));
	cadenceMeasurementCharacteristic.notify();
}
