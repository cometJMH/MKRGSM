#include "Modem.h"

ModemClass::ModemClass(Uart& uart, unsigned long baud) :
  _uart(&uart),
  _baud(baud),
  _atCommandState(AT_COMMAND_IDLE),
  _ready(1),
  _responseDataStorage(NULL),
  _ucrHandler(NULL)
{
  _buffer.reserve(64);
}

int ModemClass::begin(bool restart)
{
  _uart->begin(_baud);

  if (!autosense()) {
    return 0;
  }

  if (restart) {
    if (!reset()) {
      return 0;
    }

    if (!autosense()) {
      return 0;
    }
  }

  return 1;
}

void ModemClass::end()
{
  _uart->end();
}

int ModemClass::autosense(int timeout)
{
  for (unsigned long start = millis(); millis() < (start + timeout);) {
    if (noop() == 1) {
      return 1;
    }

    delay(100);
  }

  return 0;
}

int ModemClass::noop()
{
  send("AT");

  return (waitForResponse() == 1);
}

int ModemClass::reset()
{
  send("AT+CFUN=16");

  return (waitForResponse(1000) == 1);
}

size_t ModemClass::write(uint8_t c)
{
  return _uart->write(c);
}

void ModemClass::send(const char* command)
{
  _uart->println(command);
  _uart->flush();
  _atCommandState = AT_COMMAND_IDLE;
  _ready = 0;
}

int ModemClass::waitForResponse(unsigned long timeout, String* responseDataStorage)
{
  _responseDataStorage = responseDataStorage;
  for (unsigned long start = millis(); millis() < (start + timeout);) {
    int r = ready();

    if (r != 0) {
      _responseDataStorage = NULL;
      return r;
    }
  }

  _responseDataStorage = NULL;
  _buffer = "";
  return -1;
}

int ModemClass::ready()
{
  poll();

  return _ready;
}

void ModemClass::poll()
{
  while (_uart->available()) {
    char c = _uart->read();

Serial.write(c);

    _buffer += c;

    switch (_atCommandState) {
      case AT_COMMAND_IDLE:
      default: {
        
        if (_buffer.startsWith("AT") && _buffer.endsWith("\r\n")) {
          _atCommandState = AT_RECEIVING_RESPONSE;
          _buffer = "";
        }  else if (_buffer.endsWith("\r\n")) {
          _buffer.trim();

          if (_buffer.length()) {
            if (_ucrHandler != NULL) {
              _ucrHandler->handleUcr(_buffer);
            }
          }          

          _buffer = "";
        }

        break;
      }

      case AT_RECEIVING_RESPONSE: {
        if (c == '\n') {
          int responseResultIndex = _buffer.lastIndexOf("OK\r\n");
          if (responseResultIndex != -1) {
            _ready = 1;         
          } else {
            responseResultIndex = _buffer.lastIndexOf("ERROR\r\n");
            if (responseResultIndex != -1) {
              _ready = 2;
            } else {
              responseResultIndex = _buffer.lastIndexOf("NO CARRIER\r\n");
              if (responseResultIndex != -1) {
                _ready = 3;
              }
            }
          }

          if (_ready != 0) {
            if (_responseDataStorage != NULL) {
              _buffer.remove(responseResultIndex);
              _buffer.trim();

              *_responseDataStorage = _buffer;

              _responseDataStorage = NULL;
            }

            _atCommandState = AT_COMMAND_IDLE;
            _buffer = "";
            return;
          }
        }
        break;
      }
    }
  }
}

void ModemClass::setResponseDataStorage(String* responseDataStorage)
{
  _responseDataStorage = responseDataStorage;
}

void ModemClass::setUcrHandler(ModemUcrHandler* handler)
{
  _ucrHandler = handler;
}

ModemClass MODEM(SerialGSM, 115200);
