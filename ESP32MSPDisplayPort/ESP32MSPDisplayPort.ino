#include "msp_protocol.h"
#include "msp.h"

char *identifier = "Aldyduino";
uint16_t battery_voltage = 0;     // Will be updated from ADC (10 = 1V)
uint16_t battery_capacity = 2000; // mAh
uint16_t battery_drawn = 500;     // mAh drawn
uint16_t amperage = 150;          // 15.0A (10 = 1A)

// --- Battery Voltage ADC Settings ---
const int VBAT_PIN = 3;           // ADC pin for battery voltage (GPIO3)
const float R1 = 10000.0;         // 10k resistor from battery to pin
const float R2 = 1000.0;          // 1k resistor from pin to ground
const float V_REF = 3.3;          // ESP32 reference voltage
const float VBAT_CALIBRATION = 1.0; // Adjust this multiplier if reading is slightly off

int32_t gps_lat = 0;
int32_t gps_lon = 0;
int16_t gps_alt = 0;
int16_t gps_speed = 0;
uint8_t gps_sats = 0;

#include <TinyGPS++.h>
TinyGPSPlus gps;

// On ESP32-C3 we can route Hardware UART to any pin. This is much better and more reliable than SoftwareSerial.
HardwareSerial gpsSerial(1); 
const int GPS_RX_PIN = 4; // Connect to GPS TX
const int GPS_TX_PIN = 5; // Connect to GPS RX
const uint32_t GPS_BAUD = 9600; // Standard GPS baud rate (change if your GPS is 38400 or 115200)

bool is_armed = true; 

HardwareSerial& mspSerial = Serial0;
MSP msp;
void setup() {
  pinMode(9, INPUT_PULLUP);
  mspSerial.begin(115200);
  Serial.begin(115200); 
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  msp.begin(mspSerial);
}

void pack16(uint16_t val, uint8_t* buf, uint8_t& idx) {
  buf[idx++] = val & 0xFF;
  buf[idx++] = (val >> 8) & 0xFF;
}

void pack32(uint32_t val, uint8_t* buf, uint8_t& idx) {
  buf[idx++] = val & 0xFF;
  buf[idx++] = (val >> 8) & 0xFF;
  buf[idx++] = (val >> 16) & 0xFF;
  buf[idx++] = (val >> 24) & 0xFF;
}

void processMSPRequest(uint8_t cmd) {
  uint8_t payload[32];
  uint8_t idx = 0;

  switch (cmd) {
    case MSP_API_VERSION:{
      payload[idx++] = MSP_PROTOCOL_VERSION; 
      payload[idx++] = API_VERSION_MAJOR; 
      payload[idx++] = API_VERSION_MINOR; 
      msp.send(cmd, payload, idx);
      break;
    }
    case MSP_FC_VARIANT: {
      payload[0] = 'B';
      payload[1] = 'T';
      payload[2] = 'F';
      payload[3] = 'L';
      msp.send(cmd, payload, 4);
      break;
    }
    
    case MSP_NAME: {
      const char* name = identifier;
      uint8_t len = strlen(name);
      for(uint8_t i = 0; i < len; i++) payload[i] = name[i];
      msp.send(cmd, payload, len);
      break;
    }
    
    case MSP_STATUS: {
      pack16(2000, payload, idx); // cycle time
      pack16(0, payload, idx);    // i2c errors
      
      // Hardware sensors present (Acc, Baro, Mag, GPS)
      pack16(0x01 | 0x02 | 0x20 | 0x04, payload, idx); 
      
      uint32_t flag = 0;
      if (is_armed) flag |= 0x01; // Bit 0 is the ARM flag
      pack32(flag, payload, idx);
      
      payload[idx++] = 0; 
      msp.send(cmd, payload, idx);
      break;
    }
    
    case MSP_ANALOG: {
      payload[idx++] = (uint8_t)battery_voltage; // vbat (10 = 1V)
      pack16(battery_drawn, payload, idx);       // intPowerMeterSum (mAh)
      pack16(1023, payload, idx);                // rssi (0-1023)
      pack16(amperage, payload, idx);            // amperage (10 = 1A)
      pack16(battery_voltage * 10, payload, idx);// voltage in 0.01V steps
      msp.send(cmd, payload, idx);
      break;
    }
    
    case MSP_BATTERY_STATE: {
      payload[idx++] = 4;                       // cell count (e.g. 4S)
      pack16(battery_capacity, payload, idx);   // battery capacity
      payload[idx++] = (uint8_t)battery_voltage; // voltage (10 = 1V)
      pack16(battery_drawn, payload, idx);      // mAh drawn
      pack16(amperage, payload, idx);           // amperage
      payload[idx++] = 0;                       // battery state (0 = OK)
      pack16(battery_voltage * 10, payload, idx);// voltage in 0.01V steps
      msp.send(cmd, payload, idx);
      break;
    }
    
    case MSP_RAW_GPS: {
      payload[idx++] = 3;                       // fixType (3 = 3D fix)
      payload[idx++] = gps_sats;                // numSat
      pack32(gps_lat, payload, idx);            // lat
      pack32(gps_lon, payload, idx);            // lon
      pack16(gps_alt, payload, idx);            // alt
      pack16(gps_speed, payload, idx);          // speed
      pack16(0, payload, idx);                  // ground course
      msp.send(cmd, payload, idx);
      break;
    }
    
    case MSP_COMP_GPS: {
      pack16(15, payload, idx);                 // distanceToHome (meters)
      pack16(90, payload, idx);                 // directionToHome (degrees)
      payload[idx++] = 1;                       // heartbeat
      msp.send(cmd, payload, idx);
      break;
    }
    
    case MSP_ATTITUDE: {
      pack16(0, payload, idx);                  // roll (10 = 1 deg)
      pack16(0, payload, idx);                  // pitch
      pack16(0, payload, idx);                  // yaw
      msp.send(cmd, payload, idx);
      break;
    }
  }
}

void updateOSD() {
  // Send heartbeat
  msp.sendDisplayPortHeartbeat();
  // Clear the canvas
  msp.sendDisplayPortClear();

  char buf[32];
  snprintf(buf, sizeof(buf), "\x90 %d.%d\x06", battery_voltage / 10, battery_voltage % 10);
  msp.sendDisplayPortString(14, 1, buf);

  // Amperage (\x9A = Ampere)
  snprintf(buf, sizeof(buf), "\x9A %d.%dA", amperage / 10, amperage % 10);
  msp.sendDisplayPortString(15, 1, buf);


  // GPS Satellites (\x1E = Sat)
  snprintf(buf, sizeof(buf), "\x1E %d", gps_sats);
  msp.sendDisplayPortString(13, 38, buf);

  // GPS Speed
  snprintf(buf, sizeof(buf), "SPD: %d KM/H", (gps_speed * 36) / 1000);
  msp.sendDisplayPortString(14, 38, buf);

  // GPS Lat / Lon
  snprintf(buf, sizeof(buf), "LAT: %ld.%07ld", gps_lat / 10000000, abs(gps_lat) % 10000000);
  msp.sendDisplayPortString(15, 33, buf);
  
  snprintf(buf, sizeof(buf), "LON: %ld.%07ld", gps_lon / 10000000, abs(gps_lon) % 10000000);
  msp.sendDisplayPortString(16, 33, buf);

  msp.sendDisplayPortString(8, 20, "RACUN FPV");

  if (is_armed) {
    msp.sendDisplayPortString(10, 20, "* ARMED *");
  } else {
    msp.sendDisplayPortString(10, 19, "DISARMED");
  }

  // Draw the screen 
  msp.sendDisplayPortDrawScreen();
}

msp_packet_t packet;

void loop() {
  // Read incoming bytes from DJI
  if (msp.recv(&packet.recvMessageID, packet.payload, packet.maxSize, &packet.recvSize)) {
    processMSPRequest(packet.recvMessageID);
  }
  
  // Parse incoming GPS data
  while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read())) {
      if (gps.location.isValid()) {
        gps_lat = gps.location.lat() * 10000000;
        gps_lon = gps.location.lng() * 10000000;
      }
      if (gps.altitude.isValid()) {
        gps_alt = gps.altitude.meters();
      }
      if (gps.speed.isValid()) {
        gps_speed = gps.speed.mps() * 100; // m/s to cm/s
      }
      if (gps.satellites.isValid()) {
        gps_sats = gps.satellites.value();
      }
    }
  }

  static unsigned long lastOsdUpdate = 0;
  if (millis() - lastOsdUpdate > 200) {
    lastOsdUpdate = millis();
    
    // Read battery voltage via ADC
    int adc_val = analogRead(VBAT_PIN);
    float v_pin = (adc_val / 4095.0) * V_REF;
    float v_bat = v_pin * ((R1 + R2) / R2) * VBAT_CALIBRATION;
    battery_voltage = (uint16_t)(v_bat * 10.0); // 10 = 1V

    updateOSD();
  }

  // read pin 9 and set arm
  is_armed = (digitalRead(9) == LOW);
}
