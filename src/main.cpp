#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>

WiFiManager	wifiManager;

WiFiClient espClient;
PubSubClient client(espClient);

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>

typedef struct {
	uint8_t magic = 42;
	char mqtt_server[50];
	char mqtt_topic[50];
} t_data;

char hostname[50]  = "LEDs-matrix";

t_data data;

WiFiManagerParameter param_MQTT_server("MQTT_server", "MQTT server", "?", 50);
WiFiManagerParameter param_MQTT_topic("MQTT_topic", "MQTT Topic name", "?", 50);


const uint32_t pinCS = 2; // Attach CS to this pin, DIN to MOSI and CLK to SCK
uint32_t numberOfHorizontalDisplays = 8;
uint32_t numberOfVerticalDisplays = 1;

Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);

uint32_t wait = 25; // In milliseconds

uint32_t spacer = 1;
uint32_t width = 5 + spacer; // The font width is 5 pixels
uint32 timer = 0;

void callback(char* topic, uint8_t* message, unsigned int length) {
	Serial.printf("Topic: %s, [", topic);
	Serial.write(message, length);
	Serial.printf("], len: %d\n", length);
	// Serial.println(text_size);
	uint32 text_size = (width * length) + matrix.width() - 1 - spacer;
	for (uint32_t i = 0; i < text_size; i++) {
		matrix.fillScreen(LOW);
		int letter = i / width;
		int x = (matrix.width() - 1) - i % width;
		int y = (matrix.height() - 8) / 2; // center the text vertically

		while (x + width - spacer >= 0 && letter >= 0) {
			if (letter < (int)length) {
				matrix.drawChar(x, y, message[letter], HIGH, LOW, 1);
			}
			letter--;
			x -= width;
		}
		matrix.write();
		delay(wait);
	}

	matrix.fillScreen(LOW);
	matrix.write();
}


void reconnect() {
	if (timer < millis()) {
		Serial.print("Attempting MQTT connection...");
		// Attempt to connect
		if (client.connect(hostname)) {
			Serial.println("connected");
			client.setCallback(callback);
			client.subscribe(data.mqtt_topic);
		}
		else {
			Serial.print("failed, rc=");
			Serial.print(client.state());
			Serial.println(" try again in 10 seconds");
			timer = millis() + 10000;
		}
	}
}

void setSaveParamsCallback() {
	Serial.println("Saving config");
	data.magic = 42;
	memcpy(data.mqtt_server, param_MQTT_server.getValue(), 50);
	memcpy(data.mqtt_topic, param_MQTT_topic.getValue(), 50);
	uint8 *ptr = (uint8*)&data;
	for (uint32_t i = 0; i < sizeof(t_data); i++)
		EEPROM.write(i, ptr[i]);
	if (EEPROM.commit())
		Serial.println("EEPROM successfully committed");
	else
		Serial.println("ERROR! EEPROM commit failed");
	reconnect();
}

void setup() {
	Serial.begin(115200);
	Serial.printf("\n\n\nStart\n");
	WiFi.mode(WIFI_STA);
	EEPROM.begin(512);
	// pinMode(LED_BUILTIN, OUTPUT);

	matrix.setIntensity(15); // Use a value between 0 and 15 for brightness
	for (uint32_t i = 0; i < 8; i++) {
		matrix.setPosition(i, 7 - i, 0);
		matrix.setRotation(i, 3);
	}
	matrix.fillScreen(LOW);
	matrix.write();
	
	Serial.printf("Start Wifi manager\n");
	wifiManager.setHostname(hostname);
	wifiManager.setDebugOutput(true);
	wifiManager.setTimeout(180);
	wifiManager.setConfigPortalTimeout(180); // try for 3 minute
	wifiManager.setMinimumSignalQuality(15);
	wifiManager.setRemoveDuplicateAPs(true);
	wifiManager.setSaveParamsCallback(setSaveParamsCallback);
	wifiManager.setClass("invert"); // dark theme

	uint8* ptr = (uint8 *)&data;
	for (uint32_t i=0; i<sizeof(t_data); i++)
		ptr[i] = EEPROM.read(i);

	Serial.println(data.magic);
	Serial.println(data.mqtt_server);
	Serial.println(data.mqtt_topic);

	if (data.magic == 42) {
		param_MQTT_server.setValue(data.mqtt_server, 50);
		param_MQTT_topic.setValue(data.mqtt_topic, 50);
	} else {
		param_MQTT_server.setValue("?", 50);
		param_MQTT_topic.setValue("?", 50);
	}

	wifiManager.addParameter(&param_MQTT_server);
	wifiManager.addParameter(&param_MQTT_topic);

	std::vector<const char*> menu = {"wifi", "info", "close", "sep", "erase", "update", "restart", "exit" };
	wifiManager.setMenu(menu); // custom menu, pass vector


	bool rest = wifiManager.autoConnect(hostname);
	if (rest) {
		Serial.println("Wifi connected");
		wifiManager.startWebPortal();
	}
	else
		ESP.restart();

	Serial.print("IP: "); Serial.println(WiFi.localIP());

	Serial.print("Connect MQTT: "); Serial.println(data.mqtt_server);
	client.setServer(data.mqtt_server, 1883);
}

void loop() {

	if (!client.connected())
		reconnect();
	else
		client.loop();
	wifiManager.process();
}
