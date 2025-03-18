// Written for Grafana Labs to demonstrate how to use the M5Stick CPlus2 with Grafana Cloud
// 2024/02/10
// Willie Engelbrecht - willie.engelbrecht@grafana.com
// Introduction to time series: https://grafana.com/docs/grafana/latest/fundamentals/timeseries/
// M5StickCPlus2: https://docs.m5stack.com/en/core/M5StickC%20PLUS2
// Register for a free Grafana Cloud account including free metrics and logs: https://grafana.com

#include "M5Unified.h"
#include "M5UnitENV.h"          // Temp, Humidity, Pressure
#include "Adafruit_SGP30.h"     // VOC, CO2
#include <M5_DLight.h>          // Light level
#include <Wire.h>

// Include for I2C Multiplexer
#define PAHUB2_ADDRESS 0x70

#include "config.h"
#include "utility.h"
#include "certificates.h"
#include <PromLokiTransport.h>
#include <PrometheusArduino.h>

SHT3X sht30;
QMP6988 qmp6988;
Adafruit_SGP30 sgp;
M5_DLight dlight;

float temp = 0.0, hum = 0.0, pressure = 0.0;
int voc = 0, co2 = 0;
uint16_t lux = 0;

PromLokiTransport transport;
PromClient client(transport);
WriteRequest req(9, 2048);

TimeSeries ts_m5stick_temperature(1, "m5stick_temp", PROM_LABELS);
TimeSeries ts_m5stick_humidity(1, "m5stick_hum", PROM_LABELS);
TimeSeries ts_m5stick_pressure(1, "m5stick_pressure", PROM_LABELS);
TimeSeries ts_m5stick_voc(1, "m5stick_voc", PROM_LABELS);
TimeSeries ts_m5stick_co2(1, "m5stick_co2", PROM_LABELS);
TimeSeries ts_m5stick_lux(1, "m5stick_lux", PROM_LABELS);

// Function to select the I2C channel on the PaHUB2
void selectChannel(uint8_t channel) {
    if (channel > 7) return;
    Wire.beginTransmission(PAHUB2_ADDRESS);
    Wire.write(1 << channel);
    Wire.endTransmission();
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("Booting up!");

    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(3);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 10);
    M5.Display.setTextColor(ORANGE, BLACK);
    M5.Display.printf("== Grafana Labs ==");
    M5.Display.setTextColor(WHITE, BLACK);

    Wire.begin();

    // Initialize sensors via multiplexer
    selectChannel(2);  // Select channel for SGP30
    if (!sgp.begin()) {
        Serial.println("VOC/CO2 Sensor not found!");
        return;
    } else {
        Serial.println("SGP30 initialized successfully.");
    }

    // Set up the transport before sending any data
    transport.setUseTls(true);
    transport.setCerts(grafanaCert, strlen(grafanaCert));
    transport.setWifiSsid(WIFI_SSID);
    transport.setWifiPass(WIFI_PASSWORD);
    if (!transport.begin()) {
        Serial.println(transport.errmsg);
        return;
    }

    // Set the URL and credentials before initializing the client
    client.setUrl(GC_URL);  // Ensure you set the URL first
    client.setPath((char*)GC_PATH);
    client.setPort(GC_PORT);
    client.setUser(GC_USER);
    client.setPass(GC_PASS);

    // Initialize client after URL is set
    if (!client.begin()) {
        Serial.println(client.errmsg);
        return;
    }

    req.addTimeSeries(ts_m5stick_temperature);
    req.addTimeSeries(ts_m5stick_humidity);
    req.addTimeSeries(ts_m5stick_pressure);
    req.addTimeSeries(ts_m5stick_voc);
    req.addTimeSeries(ts_m5stick_co2);
    req.addTimeSeries(ts_m5stick_lux);
}

void loop() {
    int64_t time = transport.getTimeMillis();
    
    selectChannel(2);  // Select channel for SGP30
    if (sgp.IAQmeasure()) {
        voc = sgp.TVOC;
        co2 = sgp.eCO2;
    }
    
    // Optional: Add other sensors if needed here.
    
    ts_m5stick_temperature.addSample(time, temp);
    ts_m5stick_humidity.addSample(time, hum);
    ts_m5stick_pressure.addSample(time, pressure);
    ts_m5stick_voc.addSample(time, voc);
    ts_m5stick_co2.addSample(time, co2);
    ts_m5stick_lux.addSample(time, lux);
    
    client.send(req);
    ts_m5stick_temperature.resetSamples();
    ts_m5stick_humidity.resetSamples();
    ts_m5stick_pressure.resetSamples();
    ts_m5stick_voc.resetSamples();
    ts_m5stick_co2.resetSamples();
    ts_m5stick_lux.resetSamples();
    
    M5.Display.clear();  
    M5.Display.setCursor(10, 10);
    M5.Display.setTextColor(ORANGE, BLACK);
    M5.Display.printf("== Grafana Labs ==");
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setCursor(0, 40);
    M5.Display.printf("VOC: %d  CO2: %d\n", voc, co2);
    
    delay(5000);
}

