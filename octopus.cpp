#include "octopus.h"

static const char debugID[]  PROGMEM = "OCTOPUS";  

extern const char *ssid; 
extern const char *pass;

const uint16_t OctopusAPI::port = 443;
const char *   OctopusAPI::host = "api.octopus.energy";
const char *   OctopusAPI::path = "/v1/products/AGILE-23-12-06/electricity-tariffs/E-1R-AGILE-23-12-06-N/standard-unit-rates/";
const char *   OctopusAPI::SubjectGeneral = "Gontroller";
const char *   OctopusAPI::SubjectReport = "Gontroller - Report";
const char *   OctopusAPI::SubjectRestart = "Gontroller - Restart";
const char *   OctopusAPI::SubjectBatteryLost = "Gontroller - Battery lost";
const char *   OctopusAPI::lastReportStatus = "\0               ";

AlertMe alert(EMLSMTP, EMLPORT);

WiFiUDP udp;

GoNetwork octoConn(true, debugID);

bool TimeSlot::parse(const char *value) {
  bool result = true;  //"value_exc_vat":9.14,"value_inc_vat":9.597,"valid_from":"2021-02-28T22:30:00Z","valid_to":"2021-02-28T23:00:00Z" 
  char *nl;
  char *eptr;
  nl = strstr( value, "\"value_exc_vat\":");
  if (nl) { 
    nl = &nl[16];
    float PriceExVAT = strtod(nl, &eptr);
//    Serial.printf("price exc VAT is: %f\n", PriceExVAT);
  } 
  else {
    From = 0;
    Till = 0;
//    PriceExVAT = 0;
    PriceIncVAT = infinity();
    return false;
  }
  
  nl = strstr( eptr, "\"value_inc_vat\":");
  if (nl) { 
    nl = &nl[16];
    PriceIncVAT = strtod(nl, &eptr);
    Serial.printf("price inc VAT is: %6.4f, from ", PriceIncVAT);
  } 
  nl = strstr( eptr, "\"valid_from\":\"");
  if (nl) { 
    nl = &nl[14];
    From = parseISO8601(nl); 
    struct tm timeinfo;                     // debug purpose only
    localtime_r(&this->From, &timeinfo);    // localtime_r or gmtime_r
    Serial.printf("%.2i/%.2i/%.4i, %.2i:%.2i till ", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min);
  }
  nl = strstr( eptr, "\"valid_to\":\"");
  if (nl) { 
    nl = &nl[12];
    Till = parseISO8601(nl);
    struct tm timeinfo;                     // debug purpose only 
    localtime_r(&this->Till, &timeinfo);       // localtime_r or gmtime_r
    Serial.printf("%.2i/%.2i/%.4i, %.2i:%.2i\n", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min);

  }
  return result; 	
}	


OctopusAPI::OctopusAPI () {
  averages = {0, 0, 0};	
  lastPriceFetched = 0;
  highestPriceIncVAT = infinity();
  lowestPriceIncVAT = infinity();
  WiFiSleeping = true; // initialise to allow WiFi to be started
}

bool OctopusAPI::setClock() {
  if (ntpServerCount == 0) {
    if (octoConn.debugOutput) printDebug(PSTR("No NTP servers provided.\n"));
    return (false);
  }
  size_t idx = random(ntpServerCount);
  PGM_P ptr = (PGM_P)pgm_read_ptr(&ntpServers[idx]);
  char serverBuffer[32];

  uint16_t udpPort = 49152 + rand() % 65534 - 49152 + 1;
  udp.begin(udpPort); // pick a free local port
 
  strncpy_P(serverBuffer, ptr, 31);
  if (octoConn.debugOutput) Serial.printf("NTP server: %s\n", serverBuffer );
  serverBuffer[31] = '\0';

  // Prepare NTP request packet
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011; // LI, Version, Mode

  if (!udp.beginPacket(serverBuffer, 123)) {
    if (octoConn.debugOutput) Serial.println(F("beginPacket failed"));
    udp.stop();
    return false;
  }
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  if (!udp.endPacket()) {
    if (octoConn.debugOutput) Serial.println(F("endPacket failed"));
    udp.stop();
    return false;
  }
  
  // Poll until timeout
  unsigned long start = millis();
  unsigned long lastDot = 0;
  while (millis() - start < 5000) { // timeout 5s
    yield();
    int size = udp.parsePacket();
    if (size > 0) {
      if (octoConn.debugOutput) Serial.printf("QueryNTP: Received packet of size %d", size);
    } else {
      // Serial.print(".");
      if (millis() - lastDot >= 500) {
      Serial.print(".");
      lastDot = millis();
    }
    }
    
    if (size >= NTP_PACKET_SIZE) {
      udp.read(packetBuffer, NTP_PACKET_SIZE);

      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord  = word(packetBuffer[42], packetBuffer[43]);
      unsigned long secsSince1900 = (highWord << 16) | lowWord;
      const unsigned long seventyYears = 2208988800UL;
      struct timeval tv;
      tv.tv_sec = secsSince1900 - seventyYears;
      tv.tv_usec = 0;
      settimeofday(&tv, NULL); // no TZ change here
      udp.stop();
      return true; // Success
    }
   
    delay(10); // yield
  } 
  if (octoConn.debugOutput) Serial.println();
  udp.stop();
  return false; // Timed out  
}

void OctopusAPI::sendMail(const char* subject, const char* message, void (*writeFn)(Client& client)) {
  if (!isWifiOn()) WiFiOn ();
  // alert.debug(true);
  ESP.resetFreeContStack();
  lastReportStatus = "ATTEMPT";
  lastReportStatus = alert.send(subject, message, EMLTO, writeFn);
  if (octoConn.debugOutput) Serial.println(lastReportStatus);  
  time_t now = time(nullptr);
  if (octoConn.debugOutput) Serial.printf("Report sent %s\n", ctime(&now));
}

void OctopusAPI::setPortalEnabled(const bool enable) {
  // wm.setEnableConfigPortal(enable);
}

bool OctopusAPI::sendReportSuccessful() {
  return(lastReportStatus == "SENT");
}

bool OctopusAPI::Holiday(const uint16_t fday, const uint16_t fmonth, const uint16_t fyear, const uint16_t tday, const uint16_t tmonth, const uint16_t tyear){
  struct tm holiday = {0};
  holiday.tm_year = fyear - 1900;   // tm_year is years since 1900
  holiday.tm_mon  = fmonth - 1;     // tm_mon is 0-based (0 = January)
  holiday.tm_mday = fday;
  holiday.tm_hour = 0;
  holiday.tm_min  = 0;
  holiday.tm_sec  = 0;
  holiday.tm_isdst = -1;  // Let mktime() determine whether DST is in effect

  // Convert the tm structure to an epoch time
  time_t startEpoch = mktime(&holiday);

  holiday.tm_year = tyear - 1900;   // tm_year is years since 1900
  holiday.tm_mon  = tmonth - 1;     // tm_mon is 0-based (0 = January)
  holiday.tm_mday = tday;

  time_t endEpoch = mktime(&holiday);

  time_t now = time(nullptr);

  // Return true if the current time is within the power hour period
  return (now >= startEpoch && now < endEpoch);

};

bool OctopusAPI::PowerHour(const uint16_t day, const uint16_t month, const uint16_t year,
    const uint16_t hourFrom, const uint16_t minuteFrom) {

  // Build a tm structure for the start of the power hour
  struct tm powerStart = {0};
  powerStart.tm_year = year - 1900;  // tm_year is years since 1900
  powerStart.tm_mon  = month - 1;     // tm_mon is 0-based (0 = January)
  powerStart.tm_mday = day;
  powerStart.tm_hour = hourFrom;
  powerStart.tm_min  = minuteFrom;
  powerStart.tm_sec  = 0;
  powerStart.tm_isdst = -1;  // Let mktime() determine whether DST is in effect

  // Convert the tm structure to an epoch time
  time_t startEpoch = mktime(&powerStart);

  // Define end time as one hour later than the start
  time_t endEpoch = startEpoch + 3600;  // 3600 seconds = 1 hour

  // Get the current time
  time_t now = time(nullptr);

  // Return true if the current time is within the power hour period
  return (now >= startEpoch && now < endEpoch);
}

bool OctopusAPI::connect(BearSSL::WiFiClientSecure *client, const char *server, const uint16_t port, const char *certificate) {
  if (certificate == NULL) {
    client->setInsecure();
  }
  else {
    BearSSL::X509List cert(certificate);
    client->setTrustAnchors(&cert);
  }
  if (octoConn.debugOutput) Serial.printf("Connecting to : %s:%i\n", server, port);
  client->connect(server, port); 
  if (client->connected()) {
    // Client = client;
    if (octoConn.debugOutput) Serial.println("connected.");
  }   
  // else 
    // Client = NULL;
  return (client->connected());   
}

bool OctopusAPI::fetch(const bool Insecure) {
  BearSSL::WiFiClientSecure client;
  if (Insecure) {
    connect(&client, host, port);
  }
  else {
    connect(&client, host, port, octocert);
  }
  // return fetchURL(&client, host, port, path); 
  if (client.connected() )
    client.stop();
  return (true);
}

bool OctopusAPI::fetchInsecure() {
  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  return fetchURL(&client, host, port, path, nullptr);
}

bool OctopusAPI::fetchCertAuthority() {
  BearSSL::WiFiClientSecure client;
  BearSSL::X509List cert(octocert);
  client.setTrustAnchors(&cert);   
  //RefreshClock();     // if clock is not set correctly, it will fail
  return(fetchURL(&client, host, port, path, nullptr)); 
}

void OctopusAPI::readAndParseStream(BearSSL::WiFiClientSecure *client, ParseState &ps, ParseCallback parser) {
// void OctopusAPI::readAndParseStream(BearSSL::WiFiClientSecure *client, ParseState &ps) {
  const int bufSize = 32;
  char tmp[bufSize];
  uint32_t to = millis() + 15000;

  do {
    memset(tmp, 0, bufSize);
    int rlen = client->readBytes((uint8_t*)tmp, bufSize - 1);
    yield();
    if (rlen < 0) break;

    if (ps.stringSuspected) {
      strncat(ps.searchStr, tmp, bufSize);
      char *endString = strchr(ps.searchStr, '}');

      if ((strlen(ps.searchStr) > (bufSize * 5)) || endString) {
        ps.stringSuspected = false;
        if (endString) *endString = 0;

        if (octoConn.debugOutput) Serial.printf("String No.%i is: %s\n", ps.count, ps.searchStr);
        if (parser == nullptr) {
           parseSlot(ps);  // 🔄 Delegated slot parsing
        }         
        else
          parser(ps);
        memset(ps.searchStr, 0, bufSize * 5);
      }
    }

    char *nl = strchr(tmp, '{');
    if (nl) {
      ps.stringSuspected = true;
      memset(ps.searchStr, 0, bufSize * 4);
      strcpy(ps.searchStr, nl);
      *nl = 0;
    }

  } while (millis() < to);
}

void OctopusAPI::parseSlot(ParseState &ps) {
  if (ps.count >= Agile_Slots) return;

  TimeSlot *ts = &priceData[ps.count];
  ts->parse(ps.searchStr);

  if (ts->From != 0 && ps.count < 65) {
    if (lowestPriceIncVAT > ts->PriceIncVAT) {
      lowestPriceIncVAT = ts->PriceIncVAT;
    }
    if ((highestPriceIncVAT == infinity()) || (highestPriceIncVAT < ts->PriceIncVAT)) {
      highestPriceIncVAT = ts->PriceIncVAT;
    }
    ps.count++;
  }
}



bool OctopusAPI::fetchURL(BearSSL::WiFiClientSecure *client, const char *host, const uint16_t port, const char *path, ParseCallback parser) {

// bool OctopusAPI::fetchURL(BearSSL::WiFiClientSecure *client, const char *host, const uint16_t port, const char *path) {
  if (!isWifiOn()) WiFiOn ();
  if (!path) {
    path = "/";
  }
  lastPriceFetched = time(nullptr);   
  ESP.resetFreeContStack();
  uint32_t freeStackStart = ESP.getFreeContStack();
  if (!octoConn.connect(client, host, port)) return (false);
  if (!octoConn.httpRequest(client, cmdGet, path, host, "CosyTroller")) return (false);
  uint32_t to = millis() + 15000; // timeout up from 5000 (serial print at lower speed?
  
  
  if (client->connected()) {
    highestPriceIncVAT = infinity();
    lowestPriceIncVAT = infinity();

    ParseState ps;
     
    readAndParseStream(client, ps, parser);  // 🔄 Delegated parsing
  }

  client->stop();
  uint32_t freeStackEnd = ESP.getFreeContStack();
  if (octoConn.debugOutput) Serial.printf("\nHighest price %4.2f; lowest price %4.2f\n", highestPriceIncVAT, lowestPriceIncVAT);
  if (octoConn.debugOutput) Serial.printf("\nCONT stack used: %d\n", freeStackStart - freeStackEnd);
  if (octoConn.debugOutput) Serial.printf("BSSL stack used: %d\n-------\n\n", stack_thunk_get_max_usage());
  return(true); 
}

bool OctopusAPI::CalculateSwitching() {
  bool result = true;
  time_t priorTime = priceData[0].From;
  for (int i = 1; i < 64; i++) {
    if (priorTime != priceData[i].From + (30 * 60) ) {
      //clk.SetStatus(TFT_BLUE);
	  result = false;
    }
    priorTime = priceData[i].From;
  }
  float avgSum = priceData[0].PriceIncVAT;
  for (int i = 1; i < 48; i++) {
    avgSum = avgSum + priceData[i].PriceIncVAT;
  }
//    double/float Avg24h, Avg30d, Avg365d;
  averages.Avg24h = avgSum/48;
  if (averages.Avg30d == 0) {
    averages.Avg30d = averages.Avg24h;
  }
  else {
    averages.Avg30d = ((averages.Avg30d * 30) + averages.Avg24h) / 31;
  }
  return result;
}

void OctopusAPI::initTime () {
  struct tm timeinfo;

  printDebug(PSTR("Setting up time\n"));
  // Serial.println("Setting up time");
  strncpy_P(ntpServerBuffer, (PGM_P)pgm_read_ptr(&ntpServers[random(ntpServerCount)]), sizeof(ntpServerBuffer) - 1);
  ntpServerBuffer[sizeof(ntpServerBuffer) - 1] = '\0';
  configTime(MyTZ, ntpServerBuffer);
  //configTime(0, 0, timeServer1, timeServer2);    // First connect to NTP server, with 0 TZ offset
  
  uint32_t start = millis();
  bool gotLocalTime = false;
  time_t now;
  while(!gotLocalTime && (millis()-start) <= 5000) {
    time(&now);
    localtime_r(&now, &timeinfo);
    if(timeinfo.tm_year > (2016 - 1900)) {
      gotLocalTime = true;
    }
    delay(10);
  }
  if(!gotLocalTime){
    if (octoConn.debugOutput) printDebug(PSTR("  Failed to obtain time\n"));
    // Serial.println("  Failed to obtain time");
    return;
  }
  if (octoConn.debugOutput) printDebug(PSTR("  Got the time from NTP\n"));

  if (octoConn.debugOutput) Serial.printf("  Timezone set to %s\n", MyTZ);
  // setenv("TZ", MyTZ, 1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  // setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0", 1);
  // tzset();
  now = time(nullptr);
  localtime_r(&now, &timeinfo);
  if (octoConn.debugOutput) Serial.printf("Local time %i:%02i\n", timeinfo.tm_hour, timeinfo.tm_min);
  gmtime_r(&now, &timeinfo);
  if (octoConn.debugOutput) Serial.printf("UTC time %i:%02i\n", timeinfo.tm_hour, timeinfo.tm_min); 
}

void OctopusAPI::WiFiOff () {
  if (WiFiSleeping) return;
  sntp_stop();
  WiFi.setAutoReconnect(false); 
  WiFi.mode( WIFI_OFF );
  delay(50);
  WiFi.forceSleepBegin();
  delay( 10 );
  printDebug(PSTR("WiFi off\n"));
  WiFiSleeping = true;
}

bool OctopusAPI::WiFiOn() {
  if (!WiFiSleeping) return (true);
  // Bring up the WiFi connection
  WiFi.persistent(false); 
  WiFi.setAutoConnect(false); 
  WiFi.setAutoReconnect(false);
  WiFi.mode( WIFI_STA );

  printDebug(PSTR("MAC Address:  "));
  // Serial.print("MAC Address:  ");
  if (octoConn.debugOutput) Serial.println(WiFi.macAddress());
  WiFi.begin(ssid, pass);
  int attempts = 20;
  while ((WiFi.status() != WL_CONNECTED) && (attempts > 0)) {
    delay(500);
    attempts--;
  }  

  while (!WiFi.localIP().isSet() && (attempts > 0)) {
    delay(100);
    attempts--;
  }

  WiFiSleeping = !WiFi.isConnected();

  if (!WiFiSleeping) {
    printDebug(PSTR("\nWiFi connected\nIP address: "));
    // Serial.println("\nWiFi connected");
    // Serial.println("IP address: ");
    if (octoConn.debugOutput) Serial.println(WiFi.localIP());
    sntp_stop();
    sntp_init();
  }
  else {
    printDebug(PSTR("\nWiFi connection failed.\n"));
    // Serial.println("\nWiFi connection failed.");
    WiFiSleeping = false;
    WiFiOff();
  }
  return (!WiFiSleeping);
}

void OctopusAPI::WiFiReset() {
  // wm.resetSettings();
}

int OctopusAPI::RemainingSlots () {
  int result = 0;
  time_t now = time(nullptr);
  for (int i = 0; i < 60; i++) {
    if (now < priceData[i].Till) {
      result++;  
    }
  }
  return (result);
}

float OctopusAPI::CurrentPrice() {
  int i = CurrentTimeSlot ();
  if (i == -1) {
    return (infinity());
  }
  return (priceData[i].PriceIncVAT);
}

float OctopusAPI::CalculatedTemperature (const float HighTemperature, const float LowTemperature, const float lowPrice) {
  float lp;
  if (lowPrice == infinity()) 
    lp = lowestPriceIncVAT;
  else
    lp = lowPrice;  
  float percentage = (CurrentPrice() - lp) / 
    (highestPriceIncVAT - lowestPriceIncVAT);
  return (((HighTemperature - LowTemperature) * percentage) + LowTemperature);  
}

int OctopusAPI::CurrentTimeSlot () {
  time_t now = time(nullptr);
  for (int i = 0; i < Agile_Slots; i++) {
    if ((now >= priceData[i].From) && (now < priceData[i].Till)) {
      return (i);  
    }
  }
  return (-1);     
}

bool OctopusAPI::IsCheapestToday () {
  time_t now = time(nullptr);
  time_t today = now;
  struct tm tm1;
  localtime_r(&today, &tm1);
  tm1.tm_min = 0;
  tm1.tm_sec = 0;
  tm1.tm_hour = 0;
  today = mktime(&tm1);
  tm1.tm_hour = 24;
  time_t tomorrow = mktime(&tm1); 
  int cheapestIndex;  
  float cheapest = infinity();  
  for (int i = 0; i < Agile_Slots; i++) {
    if ((priceData[i].From >= today) && (priceData[i].From < tomorrow)) {
      if (cheapest > priceData[i].PriceIncVAT) { 
        cheapest = priceData[i].PriceIncVAT;
        cheapestIndex = i;
      }
    }
  }
  return ((now >= priceData[cheapestIndex].From) && (now < priceData[cheapestIndex].Till));
}

bool OctopusAPI::IsInCosySlotsToday(const uint8_t slotValue) {
  int n = sizeof(CosySlots_pm) / sizeof(CosySlots_pm[0]);
  for (int i = 0; i < n; i++) {
    yield();
    uint8_t val = pgm_read_byte(&CosySlots_pm[i]); // read from flash
    if (slotValue == val) return (true);
    // if (slotValue == CosySlots[i]) return (true);
  }  
  return(false);
}

// bool OctopusAPI::IsInCosySlotsTonight(const uint8_t slotValue) {
//   int n = sizeof(CosySlotsAtNight) / sizeof(CosySlotsAtNight[0]);
//   for (int i = 0; i < n; i++) {
//     yield();
//     if (slotValue == CosySlotsAtNight[i]) return (true);
//   }  
//   return(false);
// }


bool OctopusAPI::IsInCheapestSlotsToday(const uint8_t slotCount){
  if (slotCount > 47) return (true);
  if (slotCount < 1) return (false);
  time_t now = time(nullptr);
  time_t today = now;
  struct tm tm1;
  localtime_r(&today, &tm1);
  tm1.tm_min = 0;
  tm1.tm_sec = 0;
  tm1.tm_hour = 0;
  today = mktime(&tm1);
  tm1.tm_hour = 24;
  time_t tomorrow = mktime(&tm1); 
  int8_t todaySlots[50];
  int8_t index = 0;
  for (int i = 0; i < Agile_Slots; i++) {
    if ((priceData[i].From >= today) && (priceData[i].From < tomorrow)) {  
      todaySlots[index] = i;
      index++;
    }
  }  
  while (index < 50) {
    todaySlots[index] = -1;
    index++;
  } 
  bool change;
  do {
    change = false; 
    for (int i = 1; i < 50; i++) {
      if ((todaySlots[i] != -1) && (priceData[todaySlots[i]].PriceIncVAT < priceData[todaySlots[i - 1]].PriceIncVAT)) {
        change = true;
        int8_t t = todaySlots[i];  // swap index if smaller value
        todaySlots[i] = todaySlots[i - 1];
        todaySlots[i - 1] = t;
      }
    }  
    index--;
    yield();
  } while ((change) && (index >= 0));

  if (octoConn.debugOutput) Serial.printf("Cheapest slot %1.2fp\nSecond cheapest %1.2fp\n", priceData[todaySlots[0]].PriceIncVAT, priceData[todaySlots[1]].PriceIncVAT);

  bool result = false;
  for (int i = 0; i < slotCount; i++) {
    if ((now >= priceData[todaySlots[i]].From) && (now < priceData[todaySlots[i]].Till)) {
      result = true;
      if (octoConn.debugOutput) Serial.printf("%i. cheapest slot today\n", i);
    }  
  }
  return (result);

}

bool OctopusAPI::IsInCheapestConsecutiveSlotsToday(const uint8_t slotCount) { // from 1900 to 1900
  if (slotCount > 47) return (true);
  time_t now = time(nullptr);
  time_t today = now;
  struct tm tm1;
  localtime_r(&today, &tm1);
  
// how much available data have I got?
// what time is it? Am I at the beginning of the 1900 period?

  tm1.tm_min = 0;
  tm1.tm_sec = 0;
  tm1.tm_hour = 0;
  today = mktime(&tm1);
  tm1.tm_hour = 24;
  time_t tomorrow = mktime(&tm1); 
  int8_t todaySlots[50]; 
  int8_t index = 0;
  for (int i = 0; i < Agile_Slots; i++) {
    if ((priceData[i].From >= today) && (priceData[i].From < tomorrow)) {  
      todaySlots[index] = i;
      index++;
    }
  }  
  while (index < 50) {
    todaySlots[index] = -1;
    index++;
  } 

  bool change;
  do {
    change = false; 
    for (int i = 1; i < 50; i++) {
      if ((todaySlots[i] != -1) && (priceData[todaySlots[i]].From < priceData[todaySlots[i - 1]].From)) {
        change = true;
        int8_t t = todaySlots[i];  // swap index if smaller value
        todaySlots[i] = todaySlots[i - 1];
        todaySlots[i - 1] = t;
      }
    }  
    index--;
    yield();
  } while ((change) && (index >= 0)); 
  return(false);
}

void OctopusAPI::printDebug(PGM_P flashStr) {  // helper to print a PROGMEM string
  uint8_t c;
  while ((c = pgm_read_byte(flashStr++)) != 0) {
    Serial.write(c);
  }
}

// compare a RAM string to a PROGMEM string
// returns 0 if equal, <0 if ram<pgm, >0 if ram>pgm
int OctopusAPI::strcmp_RAM_P(const char *ram, PGM_P prog) {
  while (true) {
    char  c1 = *ram++;
    char  c2 = pgm_read_byte(prog++);
    if (c1 != c2) {
      // mismatch: return difference (as unsigned)
      return (uint8_t)c1 - (uint8_t)c2;
    }
    if (c1 == '\0') {
      // both ended together
      return 0;
    }
  }
}
