#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// ==================== 配置 ====================
#define TIRE_ID 1
#define INTERVAL_CONNECTED_SEC 30    // 连上WiFi后：30秒发一次
#define INTERVAL_NO_WIFI_MIN 3       // 没连上WiFi：3分钟重试一次

// PS-002 传感器 ADC 配置
#define PRESSURE_ADC_PIN 34          // ADC1_CH6 (GPIO34)
#define ADC_REF_VOLTAGE 3.3          // ESP32 ADC参考电压3.3V
#define ADC_RESOLUTION 4095          // 12位ADC (0-4095)

// 连接 ESP8266 网关
const char* SSID = "TPMS_AP";
const char* PASS = "12345678";
IPAddress TARGET_IP(192, 168, 4, 1);
uint16_t PORT = 1234;
// ==============================================

WiFiUDP udp;

// 从PS-002传感器读取胎压 (ADC模拟电压)
uint16_t getPressure() {
    // 读取ADC原始值 (0-4095)
    int rawValue = analogRead(PRESSURE_ADC_PIN);

    // 转换为电压值
    float voltage = (float)rawValue / ADC_RESOLUTION * ADC_REF_VOLTAGE;

    // PS-002 典型输出: 0.5V = 0bar, 4.5V = 最大量程 (如5bar)
    // 需要根据实际传感器量程校准下面的公式
    // 这里假设量程 0-5bar, 输出 0.5V-4.5V
    float pressureBar = (voltage - 0.5) / (4.5 - 0.5) * 5.0;

    // 限制在有效范围内
    if (pressureBar < 0) pressureBar = 0;
    if (pressureBar > 9.99) pressureBar = 9.99;

    // 转换为 kPa * 10 (例如 250.0kPa -> 2500)
    // 这样保留一位小数，且不超出 uint16_t 范围
    float pressureKpa = pressureBar * 100.0;
    return (uint16_t)(pressureKpa * 10);
}

// 发送UDP数据
void sendData() {
    uint16_t press = getPressure();
    uint8_t frame[6] = {0xAA, 0x55, TIRE_ID, 0, 0, 0};
    frame[3] = (press >> 8) & 0xFF;
    frame[4] = press & 0xFF;
    frame[5] = frame[0] + frame[1] + frame[2] + frame[3] + frame[4];

    // 打印发送的帧数据
    Serial.print("[UDP] 发送帧: ");
    for (int i = 0; i < 6; i++) {
        Serial.print("0x");
        if (frame[i] < 0x10) Serial.print("0");
        Serial.print(frame[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    udp.beginPacket(TARGET_IP, PORT);
    udp.write(frame, 6);
    udp.endPacket();
    delay(10);
}

// 进入深睡眠（根据是否连上WiFi决定睡眠时间）
void goDeepSleep(bool wifiConnected) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // 关闭所有电源域以省电
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);

    uint64_t sleepTime;
    if (wifiConnected) {
        sleepTime = INTERVAL_CONNECTED_SEC * 1000000ULL;  // 30秒
    } else {
        sleepTime = INTERVAL_NO_WIFI_MIN * 60ULL * 1000000ULL;  // 3分钟
    }

    esp_sleep_enable_timer_wakeup(sleepTime);
    esp_deep_sleep_start();
}

void setup() {
    Serial.begin(115200);  // 初始化串口用于调试打印
    btStop();  // 关闭蓝牙

    Serial.println("\n===== ESP32 TPMS 启动 =====");

    // 连接网关
    Serial.print("正在连接 WiFi: ");
    Serial.println(SSID);
    WiFi.begin(SSID, PASS);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
        delay(5);
    }

    bool connected = (WiFi.status() == WL_CONNECTED);

    if (connected) {
        Serial.println("[WiFi] 连接成功!");
        Serial.print("[WiFi] IP地址: ");
        Serial.println(WiFi.localIP());

        // 读取并打印传感器数据
        uint16_t press = getPressure();
        Serial.print("[传感器] 原始ADC值: ");
        Serial.println(analogRead(PRESSURE_ADC_PIN));
        Serial.print("[传感器] 胎压值: ");
        Serial.print(press / 10.0, 1);
        Serial.println(" kPa");

        // 发送数据并判断结果
        Serial.print("[UDP] 正在发送数据到 ");
        Serial.print(TARGET_IP.toString());
        Serial.print(":");
        Serial.print(PORT);
        Serial.print(" ... ");

        sendData();

        // 简单判断发送是否成功（UDP无法保证送达，只能判断本地是否发出）
        Serial.println("发送完成");
    } else {
        Serial.println("[WiFi] 连接失败!");
    }

    Serial.print("[睡眠] 进入深睡眠，时长: ");
    if (connected) {
        Serial.print(INTERVAL_CONNECTED_SEC);
        Serial.println(" 秒");
    } else {
        Serial.print(INTERVAL_NO_WIFI_MIN);
        Serial.println(" 分钟");
    }
    Serial.println("===========================\n");

    // 等待串口数据发送完成
    Serial.flush();

    // 根据连接状态决定下次唤醒时间
    goDeepSleep(connected);
}

void loop() {
    // 永不执行
}
