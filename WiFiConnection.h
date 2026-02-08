#ifndef WIFICONNECTION_H
#define WIFICONNECTION_H

#include <ESP8266WiFi.h>
#include <BearSSLHelpers.h>
#include <GoNaes.h>
// #include <mbedtls/md.h>
// #include "mbedtls/aes.h"
#include "Credentials.h"

#define maxNetworks 1

static const char WIFICONNECTION[] PROGMEM = "WiFiConnection V0.1"; 

struct WiFiNetwork {
  bool beVerbose = true;
  char name[16];
  char output[32];
  char HSSID[32];
  char PSKencr[16];
  char PSK[17];
  bool isActive;
  bool isMatching(const char *isSSID);
  bool encryptPW(const char *input, const char *ssid );
  bool decryptPW(unsigned char *encrypted, const char *ssid );
  void init(const char *pSSID, const char *pPSKenc, const char *pName);
  void init(const __FlashStringHelper *pSSID, const __FlashStringHelper *pPSKenc, const char *pName);
};

struct WiFiConnection {
  WiFiConnection(bool verbose = true);
  bool beVerbose;
  bool validNetworkNumber(const uint8_t value);
  bool isOff;
  WiFiNetwork network[maxNetworks];
  uint8_t currentNetwork_;
  bool setCurrentNetwork(const uint8_t value);
  uint8_t getCurrentNetwork();
  bool on();
  bool off();
  bool switchTo(uint8_t newNetwork);
  bool WiFiSleeping();
  bool isConnected();
  bool isAvailable();
 
} static WiFiConn;



#endif