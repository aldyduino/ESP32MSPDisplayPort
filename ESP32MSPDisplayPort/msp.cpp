#include <Arduino.h>

#include "msp.h"

void MSP::begin(Stream & stream, uint32_t timeout) {
  _stream   = &stream;
  _timeout  = timeout;
}

void MSP::reset() {
  _stream->flush();
  while (_stream->available() > 0)
    _stream->read();
}


void MSP::send(uint8_t messageID, uint8_t * payload, uint8_t size) {
  _stream->write('$');
  _stream->write('M');
  _stream->write('>');
  _stream->write(size);
  _stream->write(messageID);

  uint8_t checksum = size ^ messageID;
  uint8_t * payloadPtr = (uint8_t*)payload;

  for (uint8_t i = 0; i < size; ++i) {
    uint8_t b = *(payloadPtr++);
    checksum ^= b;
    _stream->write(b);
  }
  _stream->write(checksum);
}

void MSP::error(uint8_t messageID, uint8_t * payload, uint8_t size) {
  _stream->write('$');
  _stream->write('M');
  _stream->write('!');
  _stream->write(size);
  _stream->write(messageID);

  uint8_t checksum = size ^ messageID;
  uint8_t * payloadPtr = (uint8_t*)payload;

  for (uint8_t i = 0; i < size; ++i) {
    uint8_t b = *(payloadPtr++);
    checksum ^= b;
    _stream->write(b);
  }
  _stream->write(checksum);
}

void MSP::response(uint8_t messageID, uint8_t * payload, uint8_t size) {
  _stream->write('$');
  _stream->write('M');
  _stream->write('>');
  _stream->write(size);
  _stream->write(messageID);

  uint8_t checksum = size ^ messageID;
  uint8_t * payloadPtr = (uint8_t*)payload;

  for (uint8_t i = 0; i < size; ++i) {
    uint8_t b = *(payloadPtr++);
    checksum ^= b;
    _stream->write(b);
  }
  _stream->write(checksum);
}

// timeout in milliseconds
bool MSP::recv(uint8_t * messageID, uint8_t * payload, uint8_t maxSize, uint8_t * recvSize) {
  uint32_t t0 = millis();

  while (1) {
    
    // read header
    while (_stream->available() < 6)
      if (millis() - t0 >= _timeout)
        return false;
    char header[3];
    _stream->readBytes((char*)header, 3);

    // check header
    if (header[0] == '$' && header[1] == 'M' && header[2] == '<') {
      // header ok, read payload size
      *recvSize = _stream->read();

      // read message ID (type)
      *messageID = _stream->read();

      uint8_t checksumCalc = *recvSize ^ *messageID;

      // read payload
      uint8_t * payloadPtr = (uint8_t*)payload;
      uint8_t idx = 0;
      while (idx < *recvSize) {
        if (millis() - t0 >= _timeout)
          return false;
        if (_stream->available() > 0) {
          uint8_t b = _stream->read();
          checksumCalc ^= b;
          if (idx < maxSize)
            *(payloadPtr++) = b;
          ++idx;
        }
      }
      // zero remaining bytes if *size < maxSize
      for (; idx < maxSize; ++idx)
        *(payloadPtr++) = 0;

      // read and check checksum
      while (_stream->available() == 0)
        if (millis() - t0 >= _timeout)
          return false;
      uint8_t checksum = _stream->read();
      if (checksumCalc == checksum) {
        return true;
      }
      
    }
  }
  
}

// wait for messageID
bool MSP::waitFor(uint8_t messageID, uint8_t * payload, uint8_t maxSize, uint8_t * recvSize) {
  uint8_t recvMessageID;
  uint8_t recvSizeValue;
  uint32_t t0 = millis();

  while (millis() - t0 < _timeout)
    if (recv(&recvMessageID, payload, maxSize, (recvSize ? recvSize : &recvSizeValue)) && messageID == recvMessageID)
      return true;

  // timeout
  return false;  
}

// send a message and wait for the reply
bool MSP::request(uint8_t messageID, uint8_t * payload, uint8_t maxSize, uint8_t * recvSize) {
  send(messageID, NULL, 0);
  return waitFor(messageID, payload, maxSize, recvSize);
}

// send message and wait for ack
bool MSP::command(uint8_t messageID, uint8_t * payload, uint8_t size, bool waitACK) {
  send(messageID, payload, size);

  // ack required
  if (waitACK)
    return waitFor(messageID, NULL, 0);
  
  return true;
}

// MSP display port procedure
// Reference : https://betaflight.com/docs/development/API/DisplayPort

// send displayport heartbeat
void MSP::sendDisplayPortHeartbeat() {
  uint8_t payload[1] = {MSP_DP_HEARTBEAT};
  MSP::send(MSP_DISPLAYPORT, payload, 1);
}

// send displayport draw screen
void MSP::sendDisplayPortDrawScreen() {
  uint8_t payload[1] = {MSP_DP_DRAW_SCREEN};
  MSP::send(MSP_DISPLAYPORT, payload, 1);
}

// send displayport clear screen
void MSP::sendDisplayPortClear() {
  uint8_t payload[1] = {MSP_DP_CLEAR};
  MSP::send(MSP_DISPLAYPORT, payload, 1);
}

// send displayport release screen
void MSP::sendDisplayPortReleaseScreen() {
  uint8_t payload[1] = {MSP_DP_RELEASE};
  MSP::send(MSP_DISPLAYPORT, payload, 1);
}

// send displayport string 
void MSP::sendDisplayPortString(uint8_t row, uint8_t col, const char* str) {
  uint8_t len = strlen(str);
  if (len > 26) len = 26; 

  uint8_t payload[32];
  payload[0] = MSP_DP_WRITE_STRING;
  payload[1] = row;
  payload[2] = col;
  payload[3] = 0; 
  
  for (uint8_t i = 0; i < len; i++) {
    payload[4 + i] = str[i];
  }
  MSP::send(MSP_DISPLAYPORT, payload, 4 + len);
}

