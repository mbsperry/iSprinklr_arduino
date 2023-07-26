#include <Arduino.h>
#include "HunterRoam.h"

// Serial communication protocol:
// Incoming packets are 8 bytes, response is 7 bytes

// Handshake:
// [BEGIN][Conn ID][SYN][EMPTY][EMPTY][Checksum 2 byte][END]
// Response - 7 bytes
// [BEGIN][Conn ID][SYN][ACK][Checksum 2 bytes][END]
// [BEGIN][Conn ID]][ACK][EMPTY][EMPTY][Checksum 2 bytes][END]

// Command sequence
// [BEGIN][Conn ID][CMD][DATA 2 bytes][Checksum 2 bytes][END]
// [BEGIN][Conn ID][ACK][Empty byte][Checksum 2 bytes][END] or [BEGIN][Conn ID][ERR][ERR type][Checksum 2 bytes][END]

// Bad packets, improperly formated packet, data underun, etc, drop, timeout after 1s
// Server can resend packet if did not receive ACK response

#define BEGIN 255           // 0xFF
#define END 175             // 0xAF
#define START_SPRINKLER 101 // e 0x65
#define STOP_SPRINKLER 114  // r 0x72
#define SYN 238             // 0xEE
#define ACK 174             // 0xAE
#define EMPTY 0             // 0
#define ERR 221             // 0xDD
#define BAD_CMD 105         // i 0x69
#define BAD_SPRINKLER 111   // o 0x6F
#define BAD_DURATION 112    // p 0x70

#define ARRAY_MAX 8

void setup()
{
  // Setup code
  Serial.begin(9600);
  Serial.setTimeout(100);
}

// Fletcher-16 checksum function
// x-y range, x inclusive y exclusive
uint16_t fletcher16(const uint8_t *data, int x, int y)
{
  uint16_t sum1 = 0;
  uint16_t sum2 = 0;

  for (int i = x; i < y; ++i)
  {
    sum1 = (sum1 + data[i]) % 255;
    sum2 = (sum2 + sum1) % 255;
  }

  return (sum2 << 8) | sum1;
}

// Compare the checksum in the packet to the calculated checksum
bool checkData(const uint8_t *data)
{
  uint16_t rec_chk = (data[5] << 8) | data[6];
  uint16_t checksum = fletcher16(data, 1, 5);
  if (checksum != rec_chk)
  {
    return false;
  }

  return true;
}

void sendResponse(uint8_t r1, uint8_t r2, uint8_t CONN_ID, uint16_t checkInt)
{
  uint8_t header[4] = {BEGIN, CONN_ID, r1, r2};
  Serial.write(header, 4);
  Serial.write((char *)&checkInt, 2);
  Serial.write(END);
}

void loop()
{
  HunterRoam smartPort(LED_BUILTIN);
  uint8_t data[ARRAY_MAX];
  bool new_data = false;
  static uint8_t CONN_ID = 0;
  static bool connected = false;

  // Check to see if there is new data
  if (Serial.available() > 0)
  {
    // Read ARRAY_MAX bytes or until timeout
    size_t len = Serial.readBytes(data, ARRAY_MAX);
    if (len < ARRAY_MAX)
    {
      // Data underun, clear buffer
      memset(data, '\0', ARRAY_MAX - 1);
      return;
    }
    new_data = true;
  }

  if (new_data == true)
  {
    new_data = false;
    // Make sure the BEGIN and END bytes are correct
    if (data[0] != BEGIN || data[ARRAY_MAX - 1] != END)
    {
      return;
    }

    if (checkData(data) == true)
    {
      uint16_t checksum = fletcher16(data, 0, ARRAY_MAX);
      if (data[2] == SYN)
      {
        CONN_ID = data[1];
        connected = false;
        sendResponse(SYN, ACK, CONN_ID, checksum);
      }
      if (data[2] == ACK && data[1] == CONN_ID && connected == false)
      {
        connected = true;
        return;
      }
      if (connected == true && data[1] == CONN_ID)
      {
        if (data[2] == START_SPRINKLER)
        {
          if (data[3] < 1 || data[3] > 8)
          {
            sendResponse(ERR, BAD_SPRINKLER, CONN_ID, checksum);
            return;
          }
          if (data[4] < 1 || data[4] > 60)
          {
            sendResponse(ERR, BAD_DURATION, CONN_ID, checksum);
            return;
          }
          smartPort.startZone(data[3], data[4]);
        }
        else if (data[2] == STOP_SPRINKLER)
        {
          if (data[3] < 1 || data[3] > 8)
          {
            sendResponse(ERR, BAD_SPRINKLER, CONN_ID, checksum);
            return;
          }
          smartPort.stopZone(data[3]);
        }
        else
        {
          // Bad command, send err
          sendResponse(ERR, BAD_CMD, CONN_ID, checksum);
          return;
        }
        // Send acknowledge
        sendResponse(ACK, EMPTY, CONN_ID, checksum);
      }
    }
  }
}
