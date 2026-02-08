#include "WiFiConnection.h"

WiFiConnection::WiFiConnection(bool verbose) { // submit false if no Serial output required
  beVerbose = verbose; 
  network[0].init(SSID1, PSK1, "HOME");
  // strncpy(network[0].HSSID, SSID1, 32);
  // strncpy(network[0].PSKencr, PSK1, 16); 
  // strncpy(network[0].name, "HOME", 4);
  // strncpy(network[1].HSSID, SSID2, 32); //(weissnich)
  // network[1].PSK = STAPSK;
  // strncpy(network[1].name, "HOME", 4);
  currentNetwork_ = 0;
  isOff = (WiFi.status() != WL_CONNECTED);
}

bool WiFiConnection::isAvailable() { // implement
  int n = WiFi.scanNetworks(false, true);
  return (true); 
}

bool WiFiConnection::on() {
  if (!validNetworkNumber(currentNetwork_)) {
    return (false);
  }
  if (!isOff) {
    Serial.printf("%s seems connected\n", network[currentNetwork_].name); // no change
    return (false);
  }
  // if (WiFi.status() == WL_CONNECTED) return (false);
  // Bring up the WiFi connection
  WiFi.persistent(false); 
  WiFi.setAutoConnect(false); 
  WiFi.setAutoReconnect(false);
  WiFi.mode( WIFI_STA );

  int networkNo = -1;
  int n = WiFi.scanNetworks(false, true);
  uint8_t attempts = 0;
  do {
    int n = WiFi.scanNetworks(false, true);
    Serial.printf("%i networks found\n", n);
    // String sssid;
    // sssid = String(network[currentNetwork].SSID);
    // Serial.printf("Search for %s\n", sssid);
    for (int i = 0; i<n; i++) {
      if (network[currentNetwork_].isMatching(WiFi.SSID(i).c_str())) { // no change
        Serial.printf("Hash correct at %i\n", i);
        networkNo = i;
        network[currentNetwork_].decryptPW(reinterpret_cast<unsigned char*>(network[currentNetwork_].PSKencr), WiFi.SSID(i).c_str());
        // network[currentNetwork_].encryptPW(PSK, WiFi.SSID(networkNo).c_str());
        break; 
      }
      else
         Serial.printf("Hash not correct at %i\n", i);
      // if (WiFi.SSID(i).compareTo(sssid) == 0) { //(WiFi.SSID(i) == network[currentNetwork].SSID) {
      //   Serial.printf("%s found (%i).\n", WiFi.SSID(i), i);
      //   networkNo = i;
      //   break;
      // }
    }
    attempts++;
  } while ((networkNo == -1) || (attempts > 5) );
  uint8_t status;
  attempts = 0;
  uint8_t timeout = 0;
  if (networkNo != -1) {
    
    String pw = String(network[currentNetwork_].PSK); 
    // pw = String(network[currentNetwork_].PSK); // no change
    // Serial.printf("%s %s\n", WiFi.SSID(networkNo), pw);
    WiFi.begin(WiFi.SSID(networkNo), pw);
    do {
      // WiFi.setTxPower(WIFI_POWER_8_5dBm); 
      
      Serial.printf("\nConnecting to WiFi Network %s ..", WiFi.SSID(networkNo));
      do {
        status = WiFi.status();
        delay(100);
        timeout++;
        Serial.print(".");
      }  
      while ((status != WL_CONNECTED) && (timeout < 50));
      delay(500);
      WiFi.reconnect();
      Serial.println();
      attempts++; 
      timeout = 0;
    }
    while ((status != WL_CONNECTED) && (attempts < 4));  
  }
  
  if (status != WL_CONNECTED) {
    Serial.print("not ");
    isOff = true;
  }
  // else {
  //   WiFi.setAutoReconnect(true);
  //   isOff = false;
  // }
  Serial.printf("connected to %s WiFi\n", network[currentNetwork_].name); //no change
  Serial.print("Local ESP8266 IP: ");
  attempts = 100;
  while (!WiFi.localIP().isSet() && (attempts > 0)) {
    delay(100);
    attempts--;
  }
  if (attempts == 0) Serial.println(F("time-out"));
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  return (!isOff);
}


bool WiFiConnection::off() {
  if (isOff) return (false);
  
  WiFi.mode(WIFI_OFF);
  WiFi.setAutoReconnect(false);

  WiFi.setSleep(true);
  uint32_t br = Serial.baudRate();

  Serial.updateBaudRate(br);
  delay(100);
  Serial.printf("%s - WiFi off\n", network[currentNetwork_].name); // no change
  isOff = true;
  return (isOff);
}

bool WiFiConnection::setCurrentNetwork(const uint8_t value) {
  Serial.printf("New network ID: %i\n", value);
  bool result = (currentNetwork_ != value);
  currentNetwork_ = value;
  return (result);
}

uint8_t WiFiConnection::getCurrentNetwork() {
  return (currentNetwork_);
}

bool WiFiConnection::switchTo(uint8_t newNetwork) {
   if (!validNetworkNumber(newNetwork)) {
    return (false);
  }
  Serial.println(network[newNetwork].name);
  if (1==2) { //(getCurrentNetwork() == newNetwork) {
    Serial.println("No change of network" );
    return (false);
  }  
  else {
    off();
    setCurrentNetwork(newNetwork);
    on();
    return (true);
  }  
}

bool WiFiConnection::WiFiSleeping() {
  return (isOff);
}

bool WiFiConnection::validNetworkNumber(const uint8_t value) {
 if (value >= maxNetworks) {
    Serial.println("invalid network index");
    return (false);
  }
  return (true);
}

bool WiFiConnection::isConnected() {
  int status = WiFi.status();
  if (beVerbose) {
    Serial.printf("WiFi ");
    switch (status) {
      case WL_CONNECTED: Serial.println(F("successful connection")); break;
      case  WL_NO_SSID_AVAIL: Serial.println(F("missing SSID")); break;
      case  WL_NO_SHIELD: Serial.println(F("no shield")); break;  // for compatibility with WiFi Shield library
      // case  WL_STOPPED: Serial.println(F("stopped")); break;
      case  WL_IDLE_STATUS: Serial.println(F("idle status")); break;
      case  WL_SCAN_COMPLETED: Serial.println(F("scan complete")); break;
      case  WL_CONNECT_FAILED: Serial.println(F("connection failed")); break;
      case  WL_CONNECTION_LOST: Serial.println(F("connection lost")); break;
      case  WL_DISCONNECTED: Serial.println(F("disconnected")); break;
      default: Serial.printf(("Unknown result %i\n"), status); break;
    }
  }
  if (status == WL_CONNECTED) {
    IPAddress ip = WiFi.localIP();
    if (beVerbose) {
      Serial.print(F("IP assigned: "));
      Serial.println(ip);
    }
    return (ip[0] != 0);
  };
  return (false);
}

// Network ////////////////////////////////////////////////////

void WiFiNetwork::init(const char *pSSID, const char *pPSKenc, const char *pName) {
  strncpy(HSSID, pSSID, sizeof(HSSID));
  
  strncpy(PSKencr, pPSKenc, sizeof(PSKencr));

  strncpy(name, pName, sizeof(name)-1);
  name[sizeof(name)-1] = '\0';
}

void WiFiNetwork::init(const __FlashStringHelper *pSSID, const __FlashStringHelper *pPSKenc, const char *pName) {
  strncpy_P(HSSID, (PGM_P)pSSID, sizeof(HSSID));

  strncpy_P(PSKencr, (PGM_P)pPSKenc, sizeof(PSKencr)-1);
  PSKencr[sizeof(PSKencr)-1] = '\0';

  strncpy(name, pName, sizeof(name)-1);
  name[sizeof(name)-1] = '\0';
}

bool WiFiNetwork::isMatching(const char *isSSID) {
  // Combine SSIDSalt and isSSID into buf safely
  char buf[32 + sizeof(SSIDSalt)];
  strncpy(buf, SSIDSalt, sizeof(buf) - 1);
  strncat(buf, isSSID, sizeof(buf) - strlen(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  // Prepare SHA256 context
  br_sha256_context ctx;
  br_sha256_init(&ctx);
  br_sha256_update(&ctx, buf, strlen(buf));
  br_sha256_out(&ctx, output);  // output must be a 32-byte buffer

  if (beVerbose) {
  // Print hash for debugging
    Serial.print(isSSID);
    Serial.print(" , SHA256 hash: ");
    for (int i = 0; i < 32; i++) {
      Serial.printf("%02x", output[i]);
    }
    Serial.printf("\n");  
  }
  
  // Compare with stored hash
  bool result = (memcmp(HSSID, output, 32) == 0);
  // for (int i = 0; i < 32; i++) {
  //   Serial.printf("%02x", HSSID[i]);
  // }
  // Serial.printf("\n");
  memset(output, '\0', sizeof(output));

  return result;
}

bool WiFiNetwork::encryptPW(const char *input, const char *ssid ) {
  if (!input || !ssid) return(false);
  size_t len = strlen(input);
  const size_t bufSize = 16;
  if (len > bufSize) {
    // input too long for 16-byte buffer
    return false;
  }
  uint8_t* buffer = new uint8_t[bufSize]; //reserve space on heap
  if (!buffer) return (false);            //signal if no space
  
  if (len == bufSize) {
    // Case 3: exactly 16 chars, no infill, no count
    memcpy(buffer, input, bufSize);
  } 
  else {
    size_t freeSpace = bufSize - len - 1;  // bytes before input

    // Only fill random chars if there’s more than one free slot
    for (size_t i = 0; i < freeSpace; i++) {
      // uint32_t r = RANDOM_REG32;
      // buffer[i] = '0' + (r % ('z' - '0' + 1));
      buffer[i] = (char)('0' + random('z' - '0' + 1));
    }

    // Always store the count (0 if freeSpace == 1)
    buffer[freeSpace] = static_cast<uint8_t>(freeSpace);

    // Copy the input after the count byte
    memcpy(buffer + freeSpace + 1, input, len);
  }

  Serial.printf("Original: ");
  for (int i = 0; i < bufSize; i++) {
    Serial.printf("%02x ", buffer[i]);
  }
  Serial.printf("\n");

  // Prepare AES key (128-bit)
  uint8_t key[16];
  memcpy(key, &HSSID[16], 16);  // Use second half of HSSID
  int n = strlen(ssid);
  if (n > 16) n = 16;
  memcpy(key, ssid, n);

  // Encrypt in-place using Tiny AES ECB
  AES_ctx ctx;
  AES_init_ctx(&ctx, key);

  uint8_t encrypted[16];
  memcpy(encrypted, buffer, 16);  // Copy input to encrypted buffer
  AES_ECB_encrypt(&ctx, encrypted);  // Encrypt in-place

  Serial.printf("Encrypted: ");
  for (int i = 0; i < 16; i++) {
    Serial.printf("%02x ", encrypted[i]);
  }
  Serial.printf("\n");

  // Optional: verify by decrypting
  decryptPW(encrypted, ssid);

  delete[] buffer;

  return true;
}


bool WiFiNetwork::decryptPW(unsigned char *encrypted, const char *ssid ) {
  // Copy encrypted block into a local buffer
  uint8_t decrypted[16];
  memcpy(decrypted, encrypted, 16);

  // Prepare AES key (128-bit = 16 bytes)
  uint8_t key[16];
  memcpy(key, &HSSID[16], 16);  // Use second half of HSSID as key
  int n = strlen(ssid);
  if (n > 16) n = 16;
  memcpy(key, ssid, n);

  // Initialize AES context
  AES_ctx ctx;
  AES_init_ctx(&ctx, key);

  // Decrypt in-place
  AES_ECB_decrypt(&ctx, decrypted);

  // Debug output
  // Serial.printf("Decrypted: ");
  // for (int i = 0; i < 16; i++) {
  //   Serial.printf("%02x ", decrypted[i]);
  // }
  // Serial.printf(" -> ");

  // Extract embedded length marker
  size_t le = 0;
  for (int i = 0; i < 16; i++) {
    if (decrypted[i] < 32 && i == decrypted[i]) {
      le = i + 1;
      break;
    }
  }
  size_t bufSize = 16 - le;
 
  memcpy(PSK, decrypted + le, bufSize);
  PSK[bufSize] = 0;
  // Serial.printf("%s\n", PSK);
  // Serial.printf("%02x : %02x", decrypted[le-1], le-1);

  return (decrypted[le-1] == le-1);
}