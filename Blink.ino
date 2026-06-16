// ESP8266_TPMS_Gateway.ino
// 数据汇聚 - 只转发胎压数据

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

const char* apSSID = "TPMS_AP";
const char* apPassword = "12345678";
const int localPort = 1234;

WiFiUDP udp;
char packetBuffer[256];

void setup() {
    Serial.begin(115200);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    
    udp.begin(localPort);
}

void loop() {
    int packetSize = udp.parsePacket();
    if (packetSize) {
        int len = udp.read(packetBuffer, 255);
        if (len == 6) {  // 只转发6字节的胎压帧
            Serial.write(packetBuffer, 6);  // 直接转发给STM32
        }
    }
}
