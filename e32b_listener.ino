/*
  LoRa E32-TTL-100
  Receive fixed transmission message on channel.
  https://github.com/xreef/LoRa_E32_Series_Library/

  E32-TTL-100----- Arduino UNO or esp8266
  M0         ----- 3.3v (To config) GND (To send) 7 (To dinamically manage)
  M1         ----- 3.3v (To config) GND (To send) 6 (To dinamically manage)
  RX         ----- PIN 2 (PullUP &amp; Voltage divider)
  TX         ----- PIN 3 (PullUP)
  AUX        ----- Not connected (5 if you connect)
  VCC        ----- 3.3v/5v
  GND        ----- GND

*/
#define VERSION "V1.3"
#define BANNER "LoRA Channel Monitor"
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#include <EEPROM.h>
#include <string.h>

#define EEPROMSIZE 512

#include <HardwareSerial.h>
HardwareSerial e32(1);

#define FREQUENCY_915
#define E32_TTL_1W
#include "LoRa_E32.h"
LoRa_E32 e32ttl(13, 14, &e32, 25, 27, 26, UART_BPS_RATE_9600, SERIAL_8N1); //RX, TX, AUX, M0, M1

int cluster;
unsigned count=0;


void show_cmd() {
	Serial.printf("\nCommand: ");
	Serial.printf("\n  channel=15 (example)");
}

void console_update() {
	if (Serial.available()) {
		char buf[80];
		int c = Serial.readBytesUntil('\n', buf, 80);
		buf[c] = 0;
		if (buf[c-1] == '\r') {
			//Serial.printf("\n removed cr");
			buf[c-1] = 0;
		}
		configure(buf, "console");
	}
}

void configure(char* buf, String from) {
	if (!buf[0]) {
		show_cmd();
		return;
	}

	String cmd = String(buf);
	//Serial.printf("\n%s ", buf);
	if (cmd.startsWith("channel=")) {
		int c = sscanf(buf, "channel=%d", &cluster);

		if (c != 1) {
			Serial.printf("\nFormat failure(%d): [%s] ", c, buf);
			show_cmd();
			return;
		}
		Serial.printf("\nMonitoring channel=%d", cluster);

		if (from == "console") {
			EEPROM.begin(EEPROMSIZE);
			int i;
			Serial.printf("\n");
			for (i=0; buf[i]; i++) { EEPROM.write(i, buf[i]); Serial.printf("%c", buf[i]); }
			Serial.printf("\n");
			EEPROM.commit();
			Serial.printf("\nRestarting in 3 second...");
			delay(3000);
			ESP.restart();
		}
	} else
	if (cmd.startsWith("reboot")) ESP.restart();
	else show_cmd();
}

void setup()
{
	Serial.begin(115200);
	Serial.printf("\n\n%s %s", BANNER, VERSION);
	Serial.printf("\nBuild time: %s  %s ", __DATE__, __TIME__); 
	Serial.printf("\n%s", __FILENAME__);
	
	{
		EEPROM.begin(EEPROMSIZE);
		char buf[EEPROMSIZE];
		for (int i=0; i<EEPROMSIZE; i++) buf[i] = EEPROM.read(i);
		String b = String(buf);
		if (b.startsWith("channel=")) configure(buf, "setup");
		else configure("channel=20", "console");
	}

	if (cluster>0) {
		Serial.printf("\n Listening channel(%d)", cluster);
		on(22);
		e32ttl.begin();

		ResponseStructContainer c;
		c = e32ttl.getConfiguration();
		Configuration configuration = *(Configuration*) c.data;
		configuration.ADDH = 7;
		configuration.ADDL = 3; 
		configuration.CHAN = cluster;
		configuration.OPTION.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;
		configuration.SPED.airDataRate = AIR_DATA_RATE_010_24;
		configuration.OPTION.wirelessWakeupTime = WAKE_UP_250;
		configuration.SPED.uartBaudRate = UART_BPS_9600;
		configuration.SPED.uartParity = MODE_00_8N1;
		configuration.OPTION.fec = FEC_1_ON;
		configuration.OPTION.ioDriveMode = IO_D_MODE_PUSH_PULLS_PULL_UPS;
		configuration.OPTION.transmissionPower = POWER_21;
		e32ttl.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
		off(22);
	} else {
		Serial.printf("\ncluster=%d: no LoRA channel info. no operation", cluster);
		show_cmd();
	}
	if (e32ttl.available()>0) {
		Serial.printf("\n flushing incoming message");
		ResponseContainer rs = e32ttl.receiveMessage();
	}
	Serial.println();
}

bool activated[35];
void on(int pin) {
	if (!activated[pin]) {
		activated[pin] = true;
		pinMode(pin, OUTPUT);
		digitalWrite(pin, LOW);
	}
	digitalWrite(pin, HIGH);
}

void off(int pin) {
	if (!activated[pin]) {
		activated[pin] = true;
		pinMode(pin, OUTPUT);
		digitalWrite(pin, LOW);
	}
	digitalWrite(pin, LOW);
}

unsigned long mark=0;
bool once = true;

void loop()
{
	int avail = e32ttl.available();
	if (cluster>0 && avail>0) {
		on(22);
		ResponseContainer rs = e32ttl.receiveMessage();
		off(22);
		avail = rs.data.length();
		Serial.printf("\n%5d ms %4d(%2dB)", millis()-mark, count++, avail);

		Serial.printf(" (channel %d) %2d<-%2d %d", rs.data.charAt(0), rs.data.charAt(1), rs.data.charAt(2), rs.data.charAt(3));
		Serial.printf(" data=");
		for (int i=0; i<avail; i++) Serial.printf(" %d", rs.data.charAt(i));
		mark = millis();
		once = true;
	}
	if ((millis()-mark)>4000 && once) {
		Serial.printf("\n");
		once = false;
	}
	console_update();
}
