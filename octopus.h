#ifndef OCTOPUS_H
#define OCTOPUS_H

#include <time.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
// #include <WiFiManager.h>
#include "AlertMe.h" 
#include "Credentials.h"
#include <sntp.h>
#include <WiFiUdp.h>
#include "GoNetwork.h"
#include <parseISO8601.h>
#include "dudas.h"


//#include <base64.hpp>

#define Agile_Slots 96 

#define MyTZ "GMT0BST,M3.5.0/1,M10.5.0" // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

struct ParseState;

typedef void (*ParseCallback)(ParseState &ps);

const char ntpServer1[] PROGMEM = "uk.pool.ntp.org";
const char ntpServer2[] PROGMEM = "time.nist.gov";
const char ntpServer3[] PROGMEM = "europe.pool.ntp.org";

// Array of pointers to NTP servers in PROGMEM
static const char* const ntpServers[] PROGMEM = {
    ntpServer1,
    ntpServer2,
    ntpServer3
};
// Automatically get the count at compile time
constexpr size_t ntpServerCount = sizeof(ntpServers) / sizeof(ntpServers[0]);

static char ntpServerBuffer[24];

static const int NTP_PACKET_SIZE = 48;
static byte packetBuffer[NTP_PACKET_SIZE];

const char WiFiSetup[] PROGMEM = "Setup-GTRL";

// const uint8_t CosySlots[] = {
  // 8, 9, 10, 11, 12, 13, 26, 27, 28, 29, 30, 31, 44, 45, 46, 47};

const uint8_t CosySlots_pm[] PROGMEM= {
  8, 9, 10, 11, 12, 13, 26, 27, 28, 29, 30, 31, 44, 45, 46, 47};
  
const uint8_t CosySlotsAtNight_pm[] PROGMEM = {
  8, 9, 10, 11, 12, 13, 44, 45, 46, 47};  

const uint8_t CosySlotsWinter_pm[] PROGMEM = {
  8, 9, 10, 11, 12, 13, 30, 31, 44, 45, 46, 47};    

static const char octocert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)EOF";

struct ParseState;

const char reason_0[] PROGMEM = "Unknown";
const char reason_1[] PROGMEM = "Power-on reset";
const char reason_2[] PROGMEM = "External reset";
const char reason_3[] PROGMEM = "Software reset";
const char reason_4[] PROGMEM = "Hardware watchdog reset";
const char reason_5[] PROGMEM = "Deep sleep wake-up";
const char reason_6[] PROGMEM = "Software watchdog reset";
const char reason_7[] PROGMEM = "Exception reset";

static const char* const reset_reasons[] PROGMEM = {
  reason_0, reason_1, reason_2, reason_3,
  reason_4, reason_5, reason_6, reason_7
};

struct Averages {
  float Avg24h, Avg30d, Avg365d;
};

struct TimeSlot {
  float PriceIncVAT;
  time_t From;
  time_t Till;
  bool parse(const char *value);
};

struct ParseState;

static void priceSlotHandler(const char *chunk, size_t len, void *userData);

struct OctopusAPI {
private:
  friend struct ParseState;
  friend void priceSlotHandler(const char *chunk, size_t len, void *userData);
  bool fetchURL(BearSSL::WiFiClientSecure *client, const char *host, const uint16_t port, const char *path, ParseCallback parser);
  void readAndParseStream(BearSSL::WiFiClientSecure *client, ParseState &ps, ParseCallback parser);
  void parseSlot(ParseState &ps);
  void parseFree(ParseState &ps);
  static const char * lastReportStatus;
  time_t lastPriceFetched;
  float lowestPriceIncVAT;
  float highestPriceIncVAT;
  bool WiFiSleeping;
  bool connect(BearSSL::WiFiClientSecure *Client, const char *server, const uint16_t port, const char *certificate = NULL);

public:
  static const char * SubjectReport;
  static const char * SubjectGeneral;
  static const char * SubjectRestart;
  static const char * SubjectBatteryLost;
  static const uint16_t port;
  static const char *   host;
  static const char *   path;
  struct Averages averages;
  struct TimeSlot priceData[Agile_Slots]; // 2 days of half hourly slots
  
  OctopusAPI();	
  bool WiFiOn ();
  void WiFiOff ();
  void WiFiReset ();
  bool isWifiOn () { return (!WiFiSleeping); };
  bool setClock(); 
  void initTime (); 
  void sendMail(const char* subject,  const char* message, void (*writeFn)(Client& client) = NULL); 
  void setPortalEnabled(const bool enable);
  bool sendReportSuccessful(); 
//  void sendLoss();
  bool CalculateSwitching();
  bool fetch(const bool Insecure = false);
  bool fetchCertAuthority();
  bool fetchInsecure(); 
  bool PowerHour(const uint16_t day, const uint16_t month, const uint16_t year, const uint16_t hourFrom, const uint16_t minuteFrom = 0);
  bool Holiday(const uint16_t fday, const uint16_t fmonth, const uint16_t fyear, const uint16_t tday, const uint16_t tmonth, const uint16_t tyear);
  float CurrentPrice();
  float CalculatedTemperature (const float HighTemperature, const float LowTemperature, const float lowPrice = infinity());
  int CurrentTimeSlot ();
  int RemainingSlots ();
  bool IsCheapestToday ();
  bool IsInCheapestSlotsToday(const uint8_t slotCount);
  template <size_t N>
  bool IsInSlots(const uint8_t (&slots_pm)[N], uint8_t slotValue);
  bool IsInCosySlotsToday(const uint8_t slotValue);
  // bool IsInCosySlotsTonight(const uint8_t slotValue);
  bool IsInCheapestConsecutiveSlotsToday(const uint8_t slotCount);
  void printDebug(PGM_P flashStr);
  int strcmp_RAM_P(const char *ram, PGM_P prog);
  // void sendMyData(Client& c);
};

template <size_t N>
bool OctopusAPI::IsInSlots(const uint8_t (&slots_pm)[N], uint8_t slotValue) {
    for (size_t i = 0; i < N; i++) {
        yield();
        uint8_t val = pgm_read_byte(&slots_pm[i]);
        if (slotValue == val) return true;
    }
    return false;
}



#endif