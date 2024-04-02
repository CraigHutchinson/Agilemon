#include <WiFi.h>
#include "time.h"
#include <esp_sntp.h>
#include <WiFiClientSecure.h>
#include <esp_crt_bundle.h>
#include <ssl_client.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <limits>
#include <algorithm> //< std::clamp

//https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include "Adafruit_4_01_ColourEPaper.h"

const char firmwareDate[] = "07/02/2024";

/** User-provided configuration that contains SSID, WiFi wifiPassword & Octopus personal authorisation code
@code
  const char* wifiSsid = "WiFi SSID";
  const char* wifiPassword = "WiFi Password";
  const char* auth_string = "API Authorisation Code from Octopus Energy Account";
@endcode
*/
#include "secrets.h"

// SPI
const int DIN_PIN = 14;	//SPI MOSI pin, data input
const int SCLK_PIN = 13;	//SPI CLK pin, clock signal input
const int CS_PIN = 15;	//Chip selection, low active

const int DC_PIN = 27;	//Data/command, low for commands, high for data
const int RST_PIN = 26;	//Reset, low active
const int BUSY_PIN = 25;	//Busy status output pin (means busy)

const int LED_PIN = 2; //< GPIO2 has LED

/**********************************
Color Index
**********************************/
const int EPD_4IN01F_BLACK = 0x0;	/// 000
const int EPD_4IN01F_WHITE = 0x1;	///	001
const int EPD_4IN01F_GREEN = 0x2;	///	010
const int EPD_4IN01F_BLUE = 0x3;	///	011
const int EPD_4IN01F_RED = 0x4;	///	100
const int EPD_4IN01F_YELLOW = 0x5;	///	101
const int EPD_4IN01F_ORANGE = 0x6;	///	110
const int EPD_4IN01F_CLEAN = 0x7;	///	111   unavailable  Afterimage

const int EPD_4IN01F_WIDTH = 640;
const int EPD_4IN01F_HEIGHT = 480;


const int SCREEN_WIDTH = EPD_4IN01F_WIDTH;     // OLED display width, in pixels
const int SCREEN_HEIGHT = EPD_4IN01F_HEIGHT;     // OLED display height, in pixels

const int SCREEN_BLACK = EPD_4IN01F_BLACK;
const int SCREEN_WHITE = EPD_4IN01F_WHITE;
const int SCREEN_GREEN = EPD_4IN01F_GREEN;
const int SCREEN_BLUE = EPD_4IN01F_BLUE;
const int SCREEN_RED = EPD_4IN01F_RED;
const int SCREEN_YELLOW = EPD_4IN01F_YELLOW;
const int SCREEN_ORANGE = EPD_4IN01F_ORANGE;
const int SCREEN_CLEAN = EPD_4IN01F_CLEAN;

const bool headless = false; //< Run without display
// Display Scehmatic https://files.waveshare.com/upload/b/bb/4.01inch_e-Paper_HAT_%28F%29.pdf
// Reference design https://files.waveshare.com/upload/f/f0/4.01inch-ePaper-F-Reference-Design.pdf
Adafruit_4_01_ColourEPaper display(
    SCREEN_WIDTH
  , SCREEN_HEIGHT
  , RST_PIN
  , DC_PIN
  , BUSY_PIN
  , false);

//
// Declare Functions
//
void printTime(time_t timeToPrint);  // Print time to serial monitor
void Get_Octopus_Data();             // Get Octopus Data
void timeavailable(struct timeval* t);  // Callback function (get's called when time adjusts via NTP)
void drawStats();                      // Routine Refresh of Display
void drawGraph();                       // Draw tariff graph

const char* server = "api.octopus.energy";  // Server URL
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// @todo Othwr time zones can be defined from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
const char* posixTimeZone = "GMT0BST,M3.5.0/1,M10.5.0";// Time Zone as "Europe/London"	

// Cert for Octopus
const char* octopus =
  "-----BEGIN CERTIFICATE-----\n"
  "MIIEdTCCA12gAwIBAgIJAKcOSkw0grd/MA0GCSqGSIb3DQEBCwUAMGgxCzAJBgNV\n"
  "BAYTAlVTMSUwIwYDVQQKExxTdGFyZmllbGQgVGVjaG5vbG9naWVzLCBJbmMuMTIw\n"
  "MAYDVQQLEylTdGFyZmllbGQgQ2xhc3MgMiBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0\n"
  "eTAeFw0wOTA5MDIwMDAwMDBaFw0zNDA2MjgxNzM5MTZaMIGYMQswCQYDVQQGEwJV\n"
  "UzEQMA4GA1UECBMHQXJpem9uYTETMBEGA1UEBxMKU2NvdHRzZGFsZTElMCMGA1UE\n"
  "ChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjE7MDkGA1UEAxMyU3RhcmZp\n"
  "ZWxkIFNlcnZpY2VzIFJvb3QgQ2VydGlmaWNhdGUgQXV0aG9yaXR5IC0gRzIwggEi\n"
  "MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDVDDrEKvlO4vW+GZdfjohTsR8/\n"
  "y8+fIBNtKTrID30892t2OGPZNmCom15cAICyL1l/9of5JUOG52kbUpqQ4XHj2C0N\n"
  "Tm/2yEnZtvMaVq4rtnQU68/7JuMauh2WLmo7WJSJR1b/JaCTcFOD2oR0FMNnngRo\n"
  "Ot+OQFodSk7PQ5E751bWAHDLUu57fa4657wx+UX2wmDPE1kCK4DMNEffud6QZW0C\n"
  "zyyRpqbn3oUYSXxmTqM6bam17jQuug0DuDPfR+uxa40l2ZvOgdFFRjKWcIfeAg5J\n"
  "Q4W2bHO7ZOphQazJ1FTfhy/HIrImzJ9ZVGif/L4qL8RVHHVAYBeFAlU5i38FAgMB\n"
  "AAGjgfAwge0wDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0O\n"
  "BBYEFJxfAN+qAdcwKziIorhtSpzyEZGDMB8GA1UdIwQYMBaAFL9ft9HO3R+G9FtV\n"
  "rNzXEMIOqYjnME8GCCsGAQUFBwEBBEMwQTAcBggrBgEFBQcwAYYQaHR0cDovL28u\n"
  "c3MyLnVzLzAhBggrBgEFBQcwAoYVaHR0cDovL3guc3MyLnVzL3guY2VyMCYGA1Ud\n"
  "HwQfMB0wG6AZoBeGFWh0dHA6Ly9zLnNzMi51cy9yLmNybDARBgNVHSAECjAIMAYG\n"
  "BFUdIAAwDQYJKoZIhvcNAQELBQADggEBACMd44pXyn3pF3lM8R5V/cxTbj5HD9/G\n"
  "VfKyBDbtgB9TxF00KGu+x1X8Z+rLP3+QsjPNG1gQggL4+C/1E2DUBc7xgQjB3ad1\n"
  "l08YuW3e95ORCLp+QCztweq7dp4zBncdDQh/U90bZKuCJ/Fp1U1ervShw3WnWEQt\n"
  "8jxwmKy6abaVd38PMV4s/KCHOkdp8Hlf9BRUpJVeEXgSYCfOn8J3/yNTd126/+pZ\n"
  "59vPr5KW7ySaNRB6nJHGDn2Z9j8Z3/VyVOEVqQdZe4O/Ui5GjLIAZHYcSNPYeehu\n"
  "VsyuLAOQ1xk4meTKCRlb/weWsKh/NEnfVqn3sF/tM+2MR7cwA130A4w=\n"
  "-----END CERTIFICATE-----\n";

/** Price stored as Float s8p8
*/
class Price
{
public:

    static constexpr int16_t Scale = 200;

    constexpr Price() : value_() {}
    constexpr Price(float f) : value_( static_cast<int16_t>(std::round(f * Scale)) ) {}
    constexpr Price(const Price& rhs) = default;

    constexpr operator float() const
    {
      return value_ * (1.0F/Scale); 
    }

private:
    int16_t value_;
};

static_assert(Price(1.1) == 1.1F);


/** 30-minute time resolution 
*/
class Time
{
public:
  static constexpr uint32_t Minute = 60; //Hour in seconds
  static constexpr uint32_t Hour = Minute * 60; //Hour in seconds
  static constexpr uint32_t HalfHour = Hour/2; //half Hour in seconds (=1800)
  static constexpr uint32_t Day = Hour*24; //half Hour in seconds (=1800)
  static constexpr time_t EpochOffset = 1577836800LL; // time_t offset from 00:00 1-1-1970 to 00:00 1-1-2020

  struct Internal{};

  static Time nowUTC() 
  {
    time_t t;
    time(&t); // Returns the time as the number of seconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC)
    return {t};
  }

  constexpr Time() : value_() {}
  constexpr Time(time_t t) : value_( t - EpochOffset ) {}
  constexpr Time(const Time& rhs) = default;
  constexpr Time(uint32_t value, Internal ) : value_(value) {}

  constexpr time_t toPosix() const
  {
      return static_cast<time_t>(value_) + EpochOffset;
  }
  
  constexpr operator uint32_t() const
  {
      return value_;
  }
 
  Time roundUp( uint32_t toMul = HalfHour ) const  { return { ((value_ + (toMul-1)) / toMul) * toMul, Internal{} }; }  
  Time round( uint32_t toMul = HalfHour) const  { return { ((value_ + (toMul/2)) / toMul) * toMul, Internal{} }; }
  Time roundDown( uint32_t toMul = HalfHour) const  { return { (value_ / toMul) * toMul, Internal{} }; }

private:
    uint32_t value_;
};

/** Tariff datas
@todo If we need to olptimise space, we could reduce storing just StartTime(32bit) and deltaT(8bit) offset between each change of tarriff
*/
struct Tariff
{
  
    // Next day is published between 1600-2000 for the next "24-hour period" but actually until 22:30 the next day
    // 23 + (24 - 16) = 31 hours = 62 halfHours 
    // @note We round upto 64 as a resounable safe count
    static constexpr uint8_t MaxRecords = 64;

    Time startTimes[MaxRecords];
    Price prices[MaxRecords];
    
    uint8_t numRecords = 0; // No. of tariff records available from Octopus API
};
Tariff tariff;

uint8_t iCurrentTariff = 0;// to hold live tariff for display
uint8_t iLowestTariff = 0;  // to store lowest tariff & time slot present in available data
uint8_t iHighestTariff = 0;  // to store lowest tariff & time slot present in available data

bool haveLocalTime = false; //< IF we have NTP time @todo Deprecate by fixing orde rof logic/events!
Time currentTime; //< Current time in seconds since Epoch

long int nextTariffUpdate = 0;                  // used to store millis() of last tariff update
const int tariffUpdateIntervalSec = 60 * 60 ;  // millis() between successive tariff updates from Octopus (3600000ms = 1h, 10800s = 3h, 14400s = 4h)
const int tariffRetryIntervalSec = 30 ; //< Prevent API spamming for retries

const int displayMinimumUpdateInterval = 30; /// Don't update display faster than this
const int displayUpdateIntervalSec = 15 * 60;             // interval between checks of current tariff data against tariffThreshold
long int nextDisplayUpdate = 0;

//
WiFiClientSecure client;
JsonDocument doc;


/** STA driver started
*/
void WiFiStationStarted(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.print("WiFi started, connecting to SSID: ");
  Serial.println(wifiSsid);  
  WiFi.begin(wifiSsid, wifiPassword);
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  //TODO: arduino_event_info_t
  Serial.print("Connected to Wifi on: ");
  Serial.println(wifiSsid);
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.println("Got DHCP IP address: ");
  Serial.println(WiFi.localIP());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  wifi_err_reason_t reason = (wifi_err_reason_t)info.wifi_sta_disconnected.reason;

  Serial.print("WiFi AP diconnected. Reason: ");
  Serial.print(reason);
  Serial.print(" = ");
  Serial.println(WiFi.disconnectReasonName(reason));
  //TODO:
   // - NO_AP_FOUND == SSID isn't available

  // If not left by choice....
   if ( reason != WIFI_REASON_ASSOC_LEAVE )
   {
      Serial.print("WiFi connect failed to: ");
      Serial.println(wifiSsid);  
      nextTariffUpdate += tariffRetryIntervalSec * 1000; //< Delay between retry on connection failure 
      WiFi.reconnect();
  }
}

/** STA driver stopped
*/
void WiFiStationStopped(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.println("WiFi client stopped");
}

/*
	ARDUINO_EVENT_WIFI_READY,
	ARDUINO_EVENT_WIFI_SCAN_DONE,
	ARDUINO_EVENT_WIFI_STA_START,
	ARDUINO_EVENT_WIFI_STA_STOP,
	ARDUINO_EVENT_WIFI_STA_CONNECTED,
	ARDUINO_EVENT_WIFI_STA_DISCONNECTED,
	ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE,
	ARDUINO_EVENT_WIFI_STA_GOT_IP,
	ARDUINO_EVENT_WIFI_STA_GOT_IP6,
	ARDUINO_EVENT_WIFI_STA_LOST_IP,
*/
//void WiFiEvent(WiFiEvent_t event){
//    Serial.printf("[WiFi-event] event: %d\n", event);
//}

const std::array<int,3> tariffThresholds = { 0, 10, 20 }; //< TODO: Dynamic thresholds
const std::array<int,4> tariffColours = { SCREEN_BLUE, SCREEN_GREEN, SCREEN_ORANGE, SCREEN_RED }; //< TODO: Dynamic thresholds
const int tariffYScale = 7;
const std::array<int,3> tariffYIntervals = { tariffThresholds[0] * tariffYScale, tariffThresholds[1] * tariffYScale, tariffThresholds[2] * tariffYScale };

int colourForTariff( float tariff )
{
  int i = 0;
  for ( ; i < tariffThresholds.size(); ++i )
  {
    if ( tariff < tariffThresholds[i] )
      break;
  }
  return tariffColours[i];
}

//
void setup() {
  
  // initialize digital pin LED_PIN as an output.
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // turn the LED on (HIGH is the voltage level)

  //Initialize serial and wait for port to open:
  Serial.begin(115200);

  Serial.println(F("Booty bootface..."));
  if (!headless)
  {
    
    display.cp437(true); //< Use correct character tables
    display.setTextWrap(false); 
    
    if ( !display.begin(SCLK_PIN, DIN_PIN, CS_PIN)) {
      Serial.println(F("ePaper allocation failed"));
      for (;;);  // Don't proceed, loop forever
    }
  }
  
  //delay(2000);
  //
  // Time Setup
  sntp_set_time_sync_notification_cb(timeavailable);

  //https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32/api-reference/system/system_time.html#sntp-time-synchronization
  configTime(0, 0, ntpServer1, ntpServer2);  // 0, 0 because we will use TZ in the next line

  // NTP server address could be aquired via DHCP,
  sntp_servermode_dhcp(1);  // (optional)
  
  setenv("TZ", posixTimeZone, 1);            // Set environment variable with your time zone
  tzset();
  
  WiFi.onEvent(WiFiStationStarted, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_START);
  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);  
  WiFi.onEvent(WiFiStationStopped, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_STOP);
  /* Remove WiFi event
  Serial.print("WiFi Event ID: ");
  Serial.println(eventID);
  WiFi.removeEvent(eventID);*/

  //

 /* display.clearDisplay();
  //
  Serial.println("5555");
  display.setTextSize(2);  // Draw 2X-scale text
  display.setTextColor(SCREEN_BLACK);
  display.setCursor(5, 0);
  display.println(F("SSID: "));
  display.println(F(wifiSsid));
  Serial.println("33333");


  display.display();  // Show initial text
   display.waitForScreenBlocking();
  */
}

//**************************************
void loop() {
  
    //TODO: Should use NTC clock for daily fetch
  if (nextTariffUpdate == 0 
    || millis() >= nextTariffUpdate )  // update Octopus tariff periodically
  {
    if ( WiFi.getMode() == WIFI_OFF )
    {    
      WiFi.mode( WIFI_STA );
    }

    const auto wifiStatus = WiFi.status();

    if ( haveLocalTime 
      && wifiStatus == WL_CONNECTED )
    {
      tariff.numRecords = 0; //< Clear stale data

      Get_Octopus_Data();  

      if ( tariff.numRecords != 0)  
      {
        nextTariffUpdate += tariffUpdateIntervalSec * 1000; //< Delay until next tarriff update
        nextDisplayUpdate = millis(); //< Update display now 
        
      //Save power by diconnecting Wifi until next needed
        WiFi.disconnect(true);
      }
      else
      {
        // Prevent API spamming on retries
        nextTariffUpdate += tariffRetryIntervalSec * 1000;
      }
    }

  }
  
  // TODO: better timed event
  if ( tariff.numRecords 
    && (millis() > nextDisplayUpdate) )
  {
      currentTime = Time::nowUTC(); // mktime(&timeinfo);

      Time nextTariff = currentTime.roundUp();
      const auto timeToTariff = nextTariff-currentTime;
      if ( timeToTariff <= 5 )
      {
        currentTime = nextTariff; //< close enough to update as if at next tariff
      }
      else
      if ( timeToTariff <= displayMinimumUpdateInterval ) //< Should await tariff time if within 30 seconds of
      {
        nextDisplayUpdate += timeToTariff * 1000;
        return;
      }

      Serial.print("currentTime is ");
      Serial.println( currentTime );
      Serial.print("Number of Octopus Tariff Records = ");
      Serial.println(tariff.numRecords);

      iCurrentTariff = iLowestTariff = iHighestTariff = 0;
      for (int i = 0; i < tariff.numRecords; ++i )
      {
        if ( tariff.startTimes[i] > currentTime)  // find lowest published tariff beyond present one
        {          
          if (tariff.prices[i] < tariff.prices[iLowestTariff])
          {
              iLowestTariff = i;
          }

          if (tariff.prices[i] > tariff.prices[iHighestTariff])
          {
            iHighestTariff = i;  // find highest tariff present in available data
          }
        }
        else // This will be the current time
        {
          iCurrentTariff = i;
          break; //< Don't process past tariffs
        }
      }


      Serial.print("Current Tariff is ");
      Serial.print(tariff.prices[iCurrentTariff], 2);
      Serial.print("p (Record #");
      Serial.print(iCurrentTariff);
      Serial.println(")");

      Serial.print("Lowest Future Tariff Published = ");
      Serial.println(tariff.prices[iLowestTariff]);
      Serial.print("Time to Lowest Tariff is ");
      Serial.print((tariff.startTimes[iLowestTariff] - currentTime) / 3600);
      Serial.print("h (Record #");
      Serial.print(iLowestTariff);
      Serial.println(")");

      if (!headless)
      {
          display.clearDisplay();

          drawGraph();
          drawStats(); //< NOTES: Stats drawn ontop

          display.display();
          display.waitForScreenBlocking();
      }

      // @notice It may take some time to read Tariff and present information so we recalcukate time here
      currentTime = Time::nowUTC();
      Time nextRefreshAt = currentTime.roundUp(displayUpdateIntervalSec);
      auto timeToRefresh = nextRefreshAt-currentTime;
      
      Serial.print("Going to sleep for ");
      Serial.print(timeToRefresh);
      Serial.println(" seconds.");
      Serial.flush(); 
      digitalWrite(LED_PIN, LOW);   // turn the LED off by making the voltage LOW

      nextDisplayUpdate = millis() + (timeToRefresh * 1000);
      esp_deep_sleep( timeToRefresh * 1000 * 1000);
  }
} 

void Get_Octopus_Data()  // Get Octopus Data
{  
  Serial.println("\nBegin get octopus data...");
  client.setCACert(octopus);
  Serial.println("\nStarting connection to Octopus server...");
  if (!client.connect(server, 443)) {
    Serial.println("Connection failed!");
    
  } else {
    Serial.println("Connected to server!");
    


    // Make a HTTP request:
    client.print("GET https://api.octopus.energy/v1/products/AGILE-FLEX-22-11-25/electricity-tariffs/E-1R-AGILE-FLEX-22-11-25-J/standard-unit-rates/?page_size=");
    client.print(Tariff::MaxRecords);
    //TODO: Could use a 48-sample buffer and `period_from={Now}` in ISO 8601 date format e.g. "2018-05-17T16:00:00Z"
    client.println(" HTTP/1.1");
    client.println("Host: api.octopus.energy");
    client.println(auth_string); // enter Octopus authorisation string, collected from secrets.h
    client.println("Connection: close");
    client.println();
    
    //
    while (client.connected()) {
      String response = client.readStringUntil('\n');
        
        //Serial.println(response);
        
        /**Response starts with:
          Starting connection to Octopus server...
          Connected to server!
          HTTP/1.1 200 OK
          Date: Tue, 30 Jan 2024 20:32:57 GMT
          Content-Type: application/json
          Content-Length: 14095
          ...
          \r
        */
      if (response == "\r") {
        Serial.println("headers received");
        // TODO: CHeck the headers and read `Content-Length` etc
        break;
      }
    }

    //Await receipt of data
    while( client.connected() && !client.available() );
    
    // Wait for data bytes to be received
    String line;
    do
    {
      line += client.readString();
    }
    while( client.connected() /* && client.available()*/ ); //< While connected and more data to be received

    client.stop();

    // Serial.println(line);
    DeserializationError error = deserializeJson(doc, line);
    if (error) {
      Serial.print(F("deserializing JSON failed"));
      Serial.println(error.f_str());
      Serial.println("Here's the JSON I tried to parse");
      Serial.println(line);
    }
    else 
    {
      auto results = doc["results"];
      // We only consider the first X records as they ar eprovided latest to oldest
      // @note We only need 48 for a 24hr period
      tariff.numRecords = std::min( results.size(), (size_t)Tariff::MaxRecords );
      
      Serial.print("# of Records is ");
      Serial.println(tariff.numRecords);
      
      for ( int i =0; i < tariff.numRecords; ++i )
        {
        auto resultRate = results[i];
        float price = resultRate["value_inc_vat"];
        tariff.prices[i] = price;
        String periodStart = resultRate["valid_from"];
        struct tm tmpTime;
        strptime(periodStart.c_str(), "%Y-%m-%dT%H:%M:%SZ", &tmpTime);
        tariff.startTimes[i] = mktime(&tmpTime);
        
#if 0 //< Print tarriff
        Serial.print(price, 2);
        Serial.print("p, from ");
        Serial.print(periodStart);
        Serial.print(" ");
        Serial.println(tariff.startTimes[i].toPosix() );
#endif
      }

#if 0 //TODO: testing tariffs!
      tariff.prices[5] = -4.5f;
#endif

      Serial.print("Got Tariff until ");
      Serial.println(tariff.startTimes[0].toPosix() );
        
    }
  }
}
//
void printTime(time_t timeToPrint)  // Print time to serial monitor
{
  char buff1[100];
  struct tm tmpTime;
  gmtime_r(&timeToPrint, &tmpTime);
  strftime(buff1, 100, "%A, %B-%Y, %T", &tmpTime);
  Serial.print("buff1 = ");
  Serial.println(buff1);
}

/** 24-hour clock time as HH:MM
*/
struct Time24
{
    uint8_t hour;
    uint8_t minute;

    /** Duration as clock time HH:MM where the hours are not wrapped after 24 hours e.g. 35:59 for example for > 1 day
    */
    static Time24 fromSecondsDuration( uint32_t durationSeconds )
    {
        const auto durationMinutes = ((durationSeconds + 30) / 60); //< Round to nearest minute
        uint8_t hour = static_cast<uint8_t>(durationMinutes / 60);
        uint8_t minute = static_cast<uint8_t>(durationMinutes - (hour * 60));
        return { hour, minute };
    }

    /** Duration as clock time HH:MM within the 24-hour clock period from 0:00 to 23:59
    */
    static Time24 fromSecondsTimepoint( uint32_t timepointSeconds )
    {
      auto t = fromSecondsDuration(timepointSeconds);
      if ( t.hour >= 24 )
        t.hour -= (t.hour/24) * 24; //< Wrap hour on 24 hour clock
      return t;
    }
};

void drawStats() {
    
  Serial.print("Updating display...");

  //
  struct tm timeinfo;
  const time_t posixCurrentTime = currentTime.toPosix();
  localtime_r(&posixCurrentTime, &timeinfo);

  //
  //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  
  display.setFont(&FreeSansBold12pt7b);
  display.setTextSize(1);
  display.setTextColor(SCREEN_BLACK);
  display.setCursor(0, 16);
  display.println(&timeinfo, "%A, %B %d @ %H:%M:%S");
  
  display.setCursor(0, 70); //< TODO: Why gfx isn;t working this out correctly!?
  display.setTextSize(2);
  display.setTextColor( colourForTariff(tariff.prices[iCurrentTariff]));
  display.print("Now ");
  // We should always have future data so this is used as an error!
  if ( iCurrentTariff != 0 )
  {
    display.setTextSize(3);
    display.print(tariff.prices[iCurrentTariff]);
    display.setTextSize(1);
    display.println("p");
  }
  else
  {
    display.print("{{ERROR}}");
  }
  
#if 0
  display.setCursor(0, 80);
  display.setTextSize(1);
  display.setTextColor( colourForTariff(tariff.prices[iHighestTariff]));
  display.print("High ");
  display.print(tariff.prices[iHighestTariff]);
  display.print("p in ");
  const auto highIn = Time24::fromSecondsDuration(tariff.startTimes[iHighestTariff] - currentTime);
  display.print(highIn.hour);
  display.print("h ");
  display.print(highIn.minute);
  display.println("m");

  display.setTextColor( SCREEN_GREEN );
  display.print("Low ");
  display.print(tariff.prices[iLowestTariff]);
  display.print("p in ");
  const auto lowIn = Time24::fromSecondsDuration(tariff.startTimes[iLowestTariff] - currentTime);
  display.print(lowIn.hour);
  display.print("h ");
  display.print(lowIn.minute);
  display.println("m");
#endif

  display.setTextColor(SCREEN_BLACK);
}
// Callback function (get's called when time adjusts via NTP)
//
void timeavailable(struct timeval* t) {
  Serial.println("Got time adjustment from NTP!");
  haveLocalTime = true;
}

//TODO: impl properly
struct BoxX
{
  int16_t x;
  uint16_t w; 
  uint16_t h;
};

static struct BoxX centerAlignText( const char* text, uint16_t xCenter )
{
  int16_t pad = 4;
  int16_t left= pad;
  int16_t right = SCREEN_WIDTH-pad;
  int16_t x = 0, y = 0;
  uint16_t w = 0, h = 0;
  display.getTextBounds( text, 0, 0, &x, &y, &w, &h );
  const uint16_t halfWidth = w/2;
  const int16_t xText = (xCenter <= left+halfWidth ) ? left //< Clamp to left border
                  : (xCenter >= right-halfWidth ) ? right-w //< Clamp to right border
                  : xCenter - halfWidth; //< Center align

  return {xText, w, h};
}

void drawTariffMarker( const Time currentDayStart, uint xCoeff, uint yTariff
    , int iTariff, int colour)
{
    const auto markerHeight = 10;
    const auto markerWidth = 8;
    const auto xCurrent = (iCurrentTariff - iTariff) * xCoeff;
    const auto hTariff = int(tariff.prices[iTariff] * tariffYScale);
    const auto yMarker = yTariff - ((hTariff > 0) ? hTariff : 0);
   
    display.setTextSize(1);
    display.setTextColor(colour, SCREEN_WHITE);

    const auto time24 = Time24::fromSecondsTimepoint(tariff.startTimes[iTariff] - currentDayStart);

    // Pad between marker and text and text lines
    const auto lineSpacing = 3;
    auto yCursor = yMarker - markerHeight - lineSpacing;
    
    char text[64];

    snprintf( text, sizeof(text), "%u:%02u"
      , time24.hour
      , time24.minute );
    {
        display.setFont(&FreeSans9pt7b); 
        auto  pos = centerAlignText( text, xCurrent );
        display.setCursor( pos.x, yCursor );
        display.print(text);
        yCursor -= pos.h + lineSpacing;
    }
    
    const auto tarriffIn = Time24::fromSecondsDuration(tariff.startTimes[iTariff] - currentTime);      
    snprintf( text, sizeof(text), "%uh %02um"
      , tarriffIn.hour
      , tarriffIn.minute );
    {
      display.setFont(&FreeSansBold12pt7b); 
      auto pos = centerAlignText( text, xCurrent );
      display.setCursor( pos.x, yCursor );
      display.print(text);
      yCursor -= pos.h + lineSpacing;
    }

    snprintf( text, sizeof(text), "%.2f"
      , (float)tariff.prices[iTariff] );
    {
      display.setFont(&FreeSansBold12pt7b); 
      display.setTextSize(2);
      auto pos = centerAlignText( text, xCurrent );
      display.setCursor( pos.x, yCursor );
      display.print(text);
      display.setTextSize(1);
      display.print("p"); //< Small 'p'
      yCursor -= pos.h + lineSpacing;
    }

    display.fillTriangle(
         xCurrent - markerWidth / 2, yMarker - markerHeight
        , xCurrent + markerWidth / 2, yMarker - markerHeight
        , xCurrent, yMarker - 2, SCREEN_BLUE);

}

void drawGraph() {
    const int left = 0;
    const int top = SCREEN_HEIGHT / 2;
    const int w = SCREEN_WIDTH;
    const int h = SCREEN_HEIGHT / 2 - 25;

    //  display.drawLine(0, 15, 0, 63, SCREEN_BLACK);  // Draw Axes
    display.drawLine(left, top + h, w, top + h, SCREEN_BLACK);
    int i = 1;
    const Time currentTariffTime = currentTime.roundDown(Time::HalfHour);
    const Time currentDayStart = currentTariffTime.roundDown(Time::Day);

    const auto barCount = (tariff.startTimes[0] - currentTariffTime + Time::HalfHour) / Time::HalfHour;
    const auto xCoeff = (w + (barCount/2)) / barCount;
    //const auto maxCount = (w + (xCoeff-1)) /xCoeff;
#if 0
    while (i < w) {
        if (i % xCoeff == 0) {
            display.drawPixel(i, top + h / 2 + 10, SCREEN_BLACK);  // Draw grid line at 10p intervals
            display.drawPixel(i, top + h / 2, SCREEN_BLACK);
            display.drawPixel(i, top + h / 2 - 10, SCREEN_BLACK);
        }
        i++;
    }
    //display.setFont(); //< default font
#endif

    const auto yTariff = top + h;
    // only plot future values
    for (int i = 0; i <= std::min(iCurrentTariff, tariff.numRecords); ++i)
    {
        const auto xCurrent = (iCurrentTariff-i) * xCoeff;
        const int colour = colourForTariff(tariff.prices[i]);
        const auto hTariff = int(tariff.prices[i] * tariffYScale);

        int previousY = 0;
        for (int iY = 0; iY <= tariffYIntervals.size(); ++iY)
        {
            bool isAtTariff = iY == tariffYIntervals.size() || tariff.prices[i] < tariffThresholds[iY];
            const auto nextY = isAtTariff ? hTariff : tariffYIntervals[iY];
            if (!isAtTariff && nextY == previousY) continue;

            display.fillRect(
                xCurrent + 1
                , yTariff - nextY
                , xCoeff - 2
                , nextY - previousY, tariffColours[iY]); //< or `colour` to not have intervals

            if (isAtTariff)
            {
                //Add central spine of tarriff-colour over the lower intervals
                display.fillRect(
                    xCurrent + 3
                    , yTariff - previousY
                    , xCoeff - 6
                    , previousY, colour);

                break;
            }

            previousY = nextY;
        }
    }
    
    /// X-Axis hour markers    
    const auto currentHourEnd = currentTariffTime.roundUp(Time::Hour);
    const auto timetoHourEnd = currentHourEnd - currentTariffTime;

    const auto iFirstHour = iCurrentTariff - (timetoHourEnd / Time::HalfHour);
    const uint8_t firstHour = (currentHourEnd - currentDayStart) / Time::Hour;
  
    display.setTextSize(1);
    display.setTextColor(SCREEN_BLACK);

    const auto iHourMarkerBase = iCurrentTariff-iFirstHour;
    const auto hourCount = (iCurrentTariff-1)/2;
    for (int iHour = 0; iHour <= hourCount; ++iHour)
    {
        const auto xCurrent = (iHourMarkerBase + iHour*2) * xCoeff;
        
        display.drawFastVLine( xCurrent, top + h, 7, SCREEN_BLACK  );
        display.drawFastVLine( xCurrent+1, top + h, 7, SCREEN_BLACK  );

        const uint8_t hourValue = (firstHour + iHour) % 24;

        // Small Hour
        auto xMarker = xCurrent - (hourValue >= 10 ? 8 : 4);
        if ( xMarker < 0 ) xMarker = 0; //< Clip to left
        display.setCursor( xMarker, top + h + 13 );
        display.setFont(&FreeSans9pt7b);  
        display.print( hourValue );

        // Tiny ':00'
        display.setFont(nullptr); //< default  
        display.print( ":00" );
    }

    if (iLowestTariff != -1)
    { 
        // Draw triangle above the lowest tariff visible, to highlight it
        drawTariffMarker(currentDayStart, xCoeff, yTariff, iLowestTariff, SCREEN_GREEN);
    }
    if (iHighestTariff != -1)
    {
        // Draw triangle above the lowest tariff visible, to highlight it
        drawTariffMarker(currentDayStart,  xCoeff, yTariff, iHighestTariff, SCREEN_RED);
    }
}
