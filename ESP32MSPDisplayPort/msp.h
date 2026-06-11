

#ifndef MSP_h
#define MSP_h

#include <Arduino.h>
#include <Stream.h>

#include "msp_protocol.h"

class MSP {
  public:
    void begin(Stream & stream, uint32_t timeout = 500);
    void send(uint8_t messageID, uint8_t * payload, uint8_t size);
    void error(uint8_t messageID, uint8_t * payload, uint8_t size);
    void response(uint8_t messageID, uint8_t * payload, uint8_t size);
    bool recv(uint8_t * messageID, uint8_t * payload, uint8_t maxSize, uint8_t * recvSize);    
    bool waitFor(uint8_t messageID, uint8_t * payload, uint8_t maxSize, uint8_t * recvSize = NULL);
    bool request(uint8_t messageID, uint8_t * payload, uint8_t maxSize, uint8_t * recvSize = NULL);
    bool command(uint8_t messageID, uint8_t * payload, uint8_t size, bool waitACK = true);
    void reset();
    void sendDisplayPortHeartbeat();
    void sendDisplayPortClear();
    void sendDisplayPortReleaseScreen();
    void sendDisplayPortDrawScreen();
    void sendDisplayPortString(uint8_t row, uint8_t col, const char* str);

  private:
    Stream * _stream;
    uint32_t _timeout;
    
};

#endif