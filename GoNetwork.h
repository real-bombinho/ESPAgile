#ifndef GONETWORK_H
#define GONETWORK_H

typedef void (*ChunkHandler)(const char *chunk, size_t len, void *userData);

const char cmdGet[]  PROGMEM = "GET ";
const char cmdPost[] PROGMEM = "POST ";
const char cmdHelo[] PROGMEM = "HELO ";
const char cmdEhlo[] PROGMEM = "EHLO ";
const char cmdHttp[] PROGMEM = " HTTP/1.0\r\n";

const char msgConnecting[] PROGMEM = "Trying: ";
const char msgNotConnected[] PROGMEM = "*** Can't connect. ***\n-------\n";
const char msgConnected[] PROGMEM = "Connected!\n-------\n";

struct GoNetwork {
private:
  void printDebugImpl(PGM_P flashStr) {
    uint8_t c;

    if (progmemName) {
      Serial.write('*');
      const char* pmn = progmemName;
      while ((c = pgm_read_byte(pmn++)) != 0) {
        Serial.write(c);
      }
      Serial.write(':');
      Serial.write(' ');
    }

    while ((c = pgm_read_byte(flashStr++)) != 0) {
      Serial.write(c);
    }
  }
public: 
  bool debugOutput = true;
  const char* progmemName = nullptr;
  void printP(Print &client, PGM_P flashStr) {  // helper to print a PROGMEM string
    uint8_t c;
    // pgm_read_byte() reads a single byte from flash
    const char* fs = flashStr;
    while ((c = pgm_read_byte(fs++)) != 0) {
      client.write(c);
      if (debugOutput) Serial.write(c);
    }
  };
  void  printN(Print &client, char *numString) { // helper to print a number string (<16 characters)
  for (uint8_t i = 0; i < 16; i++) {
      uint8_t c = numString[i];
      if (c == 0) break;
      client.write(c);
      if (debugOutput) Serial.write(c);
    }
  };
  
  void printDebug(PGM_P flashStr) {
    printDebugImpl(flashStr);
  }

  void printDebug(const __FlashStringHelper* flashStr) {
    printDebugImpl(reinterpret_cast<PGM_P>(flashStr));
  }
  
  bool connect(BearSSL::WiFiClientSecure *client, const char *host, uint16_t port, const char *certificate = NULL) {
    printDebug(msgConnecting);
    if (certificate != NULL) {
      BearSSL::X509List cert(certificate);
      client->setTrustAnchors(&cert);
    }
    Serial.flush();
    Serial.print(host);
    Serial.print(":");
    Serial.println(port);
    Serial.flush();
    if (!client->connect(host, port)) {
        printDebug(msgNotConnected);
        return false;
    }
    printDebug(msgConnected);
    return (true);
  };

  bool httpRequest(BearSSL::WiFiClientSecure *client, const char *methodPROGMEM, const char *path, const char *host, const char *userAgent)   {
    if (!path) path = "/";
    char method[8];  // Enough for "GET" or "POST"
    strcpy_P(method, methodPROGMEM);  // Copy from PROGMEM to RAM
    int n1 = client->printf("%s %.512s HTTP/1.0\r\n", method, path);
    int n2 = client->printf("Host: %.512s\r\n", host);
    int n3 = client->printf("User-Agent: %s\r\n\r\n", userAgent ? userAgent : "ESPClient");
    if (n1 <= 0 || n2 <= 0 || n3 <= 0) return (false);
    return (true);
  };
  GoNetwork(bool f, const char* strInProgmem)
    : debugOutput(f), progmemName(strInProgmem) {}
  void readHTTPBody(BearSSL::WiFiClientSecure *client, uint32_t timeoutMs, ChunkHandler handler, void *userData) {
    uint32_t deadline = millis() + timeoutMs;
    const int bufSize = 32;
    char buf[bufSize];

    while (millis() < deadline && client->connected()) {
        memset(buf, 0, bufSize);
        int rlen = client->readBytes((uint8_t*)buf, bufSize - 1);
        if (rlen <= 0) break;
        yield();
        for (int i= 0; i<rlen; i++) {Serial.write(buf[i]);}
        if (handler) handler(buf, rlen, userData);
    }
    client->stop();
  };  

};


#endif