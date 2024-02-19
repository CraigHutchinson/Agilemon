
#include <WiFi.h>
#include "time.h"
#include <esp_sntp.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <limits>
#include <algorithm> //< std::clamp

//https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts
#include <Fonts/FreeMonoBoldOblique12pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>
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
const int EPD_4IN01F_HEIGHT = 400;


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
//void printLocalTime();
void timeavailable(struct timeval* t);  // Callback function (get's called when time adjusts via NTP)
void drawStats();                      // Routine Refresh of Display
void drawGraph();                       // Draw tariff graph

const char* server = "api.octopus.energy";  // Server URL
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;
const char* time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";  // TimeZone rule for Europe/Rome including daylight adjustment rules (optional)
//
// Cert for Octopus
//
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
  static constexpr uint32_t HalfHour = 60 * 30; //half Hour in seconds (=1800)
  static constexpr time_t EpochOffset = 1577836800LL; // time_t offset from 00:00 1-1-1970 to 00:00 1-1-2020

  struct Internal{};

  constexpr Time() : value_() {}
  constexpr Time(time_t t) : value_( t - EpochOffset ) {}
  constexpr Time(const Time& rhs) = default;
  constexpr Time(uint32_t value, Internal ) : value_(value) {}

  constexpr time_t to_time_t() const
  {
      return static_cast<time_t>(value_) + EpochOffset;
  }
  
  constexpr operator uint32_t() const
  {
      return value_;
  }
 
  Time roundUp( uint32_t toMul = HalfHour )  { return { ((value_ + (toMul-1)) / toMul) * toMul, Internal{} }; }  
  Time round( uint32_t toMul = HalfHour)   { return { ((value_ + (toMul/2)) / toMul) * toMul, Internal{} }; }
  Time roundDown( uint32_t toMul = HalfHour)  { return { (value_ / toMul) * toMul, Internal{} }; }

private:
    uint32_t value_;
};

//
//Define Variables
//
// Create arrays to store start times of each 1/2hr tariff slot and agile tariff for each
//
struct Tariff
{
    static constexpr uint8_t MaxRecords = 100;

    Time startTimes[MaxRecords];
    Price prices[MaxRecords];
    
    uint8_t numRecords = 0; // No. of tariff records available from Octopus API
};
Tariff tariff;

uint8_t iCurrentTariff = 0;// to hold live tariff for display
uint8_t iLowestTariff = 0;  // to store lowest tariff & time slot present in available data
uint8_t iHighestTariff = 0;  // to store lowest tariff & time slot present in available data

bool haveLocalTime = false;
Time currentTime; //< CUrrent time in seconds since Epoch

long int nextTarriffUpdate = 0;                  // used to store millis() of last tariff update
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
      nextTarriffUpdate += tariffRetryIntervalSec * 1000; //< Delay between retry on connection failure 
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


int colorForTarif( float tarif )
{
  if ( tarif < 0 ) return SCREEN_BLUE;
  if ( tarif < 10 ) return SCREEN_GREEN;
  if ( tarif < 20 ) return SCREEN_ORANGE;
  return SCREEN_RED;
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
    if ( !display.begin(SCLK_PIN, DIN_PIN, CS_PIN)) {
      Serial.println(F("ePaper allocation failed"));
      for (;;);  // Don't proceed, loop forever
    }
  }
  
  //delay(2000);
  //
  // Time Setup
  sntp_set_time_sync_notification_cb(timeavailable);

  /**
    NTP server address could be aquired via DHCP,

    NOTE: This call should be made BEFORE esp32 aquires IP address via DHCP,
    otherwise SNTP option 42 would be rejected by default.
    NOTE: configTime() function call if made AFTER DHCP-client run
    will OVERRIDE aquired NTP server address
  */
  sntp_servermode_dhcp(1);  // (optional)
      
  /**
    This will set configured ntp servers and constant TimeZone/daylightOffset
    should be OK if your time zone does not need to adjust daylightOffset twice a year,
    in such a case time adjustment won't be handled automagicaly.
  */
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

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
  if (nextTarriffUpdate == 0 
    || millis() >= nextTarriffUpdate )  // update Octopus tariff periodically
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
        nextTarriffUpdate += tariffUpdateIntervalSec * 1000; //< Delay until next tarriff update
        nextDisplayUpdate = millis(); //< Update display now 
        
      //Save power by diconnecting Wifi until next needed
        WiFi.disconnect(true);
      }
      else
      {
        // Prevent API spamming on retries
        nextTarriffUpdate += tariffRetryIntervalSec * 1000;
      }
    }

  }
  
  // TODO: better timed event
  if ( tariff.numRecords 
    && (millis() > nextDisplayUpdate) )
  {
    //
    //printLocalTime();  // it will take some time to sync time :)
    //
      //CHECK TARIFF AND SET OUTPUT PIN HERE
      struct tm timeinfo;
      getLocalTime(&timeinfo);
      currentTime = mktime(&timeinfo);
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
      Serial.println(currentTime);
      Serial.print("Number of Octopus Tariff Records = ");
      Serial.println(tariff.numRecords);

      int i = 0;

      iCurrentTariff = iLowestTariff = iHighestTariff = 0;
      while (i < tariff.numRecords) {
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
        else
        if ((currentTime > tariff.startTimes[i]) && ((currentTime - tariff.startTimes[i]) < Time::HalfHour)) 
        {
          iCurrentTariff = i;

          Serial.print("Current Tariff is ");          
          Serial.print(tariff.prices[iCurrentTariff], 2);
          Serial.print("p (Record #");
          Serial.print(i);
          Serial.println(")");

          break; //< DOn't process the past...
        }
        i++;
      }
                      
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

      
      getLocalTime(&timeinfo);
      Time now = mktime(&timeinfo);
      Time nextRefreshAt = now.roundUp(displayUpdateIntervalSec);
      auto timeToRefresh = nextRefreshAt-now;
      
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
    client.println("GET https://api.octopus.energy/v1/products/AGILE-FLEX-22-11-25/electricity-tariffs/E-1R-AGILE-FLEX-22-11-25-J/standard-unit-rates/?page_size=60 HTTP/1.1");
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
    
    
    delay(500); //< Must be better way to know more data is to come...

    // Wait for data bytes to be received
    String line;
    do
    {
      line += client.readString();
      delay(500); //< Must be better way to know more data is to come...
    }
    while( client.connected() && client.available() ); //< While connected and more data to be received

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
      // note: We only need 48 for a 24hr period
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
        Serial.println(tariff.startTimes[i].to_time_t() );
#endif
      }
    
      Serial.print("Got Tariff until ");
      Serial.println(tariff.startTimes[0].to_time_t() );
        
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
// Clock Functions
//
/*
void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("No time available (yet)");
    return;
  }
  //
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
//  display.print(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}
*/
void drawStats() {
    
  Serial.print("Updating display...");

  //
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("No time available (yet)");
    return;
  }
  //
  //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  
  display.setFont(&FreeMonoBoldOblique12pt7b);    
  display.setTextSize(1);
  display.setTextColor(SCREEN_BLACK);
  display.setCursor(0, 15);
  display.println(&timeinfo, "%A, %B %d @ %H:%M:%S");
  
  display.setCursor(0, 55); //< TODO: Why gfx isn;t working this out correctly!?
  display.setTextSize(2);
  display.setTextColor( colorForTarif(tariff.prices[iCurrentTariff]));
  display.print("Current = ");
  display.print(tariff.prices[iCurrentTariff]);
  display.println(" p");
  
  display.setTextSize(1);
  display.setTextColor( colorForTarif(tariff.prices[iHighestTariff]));
  display.print("Next High = ");
  display.print(tariff.prices[iHighestTariff]);
  display.print("p (In ");
  const auto highIn = tariff.startTimes[iHighestTariff] - currentTime;
  int highHours = highIn / (60*60);
  int highMinutes = ((highIn+30) / 60) - (highHours * 60);
  display.print(highHours);
  display.print("h ");
  display.print(highMinutes);
  display.println("m)");

  display.setTextColor( SCREEN_GREEN );//colorForTarif(tariff.prices[iHighestTariff]));
  display.print("Next Low = ");
  display.print(tariff.prices[iLowestTariff]);
  display.print("p (In ");
  const auto lowIn = tariff.startTimes[iLowestTariff] - currentTime;
  int lowHours = lowIn / (60*60);
  int lowMinutes = ((lowIn+30) / 60) - (lowHours * 60);
  display.print(lowHours);
  display.print("h ");
  display.print(lowMinutes);
  display.println("m)");

  display.setTextColor(SCREEN_BLACK);
}
// Callback function (get's called when time adjusts via NTP)
//
void timeavailable(struct timeval* t) {
  Serial.println("Got time adjustment from NTP!");
  haveLocalTime = true;
  //  printLocalTime();
}
//
void drawGraph() {
  const int left = 0;
  const int top =  SCREEN_HEIGHT/2;
  const int w = SCREEN_WIDTH;
  const int h = SCREEN_HEIGHT/2 - 25;

  //  display.drawLine(0, 15, 0, 63, SCREEN_BLACK);  // Draw Axes
  display.drawLine(left, top+h, w, top+h, SCREEN_BLACK);
  int i = 1;
  const Time currentTarriffTime = currentTime.roundDown();
  const auto barCount = (tariff.startTimes[0] - currentTarriffTime + Time::HalfHour) /  + Time::HalfHour;
  const auto xCoeff = (w / barCount);

  while (i < w) {
    if (i % xCoeff == 0) {
      display.drawPixel(i, top+ h/2 + 10, SCREEN_BLACK);  // Draw grid line at 10p intervals
      display.drawPixel(i, top+ h/2, SCREEN_BLACK);
      display.drawPixel(i, top+ h/2 - 10, SCREEN_BLACK);
    }
    i++;
  } 
    display.setFont(); //< default font

  display.setCursor(0, top+h);
  display.setTextSize(2);
  display.print("Now");

// only plot future values
  for (int i =0; i <= tariff.numRecords && currentTarriffTime <= tariff.startTimes[i]; ++i)
   {
      const auto xCurrent = ((tariff.startTimes[i] - currentTarriffTime) / Time::HalfHour) * xCoeff;
      int color = colorForTarif( tariff.prices[i]);
      
      const auto hTarrif = int(tariff.prices[i])*7;
      const auto yTarrif = top + (h - hTarrif);

      if (tariff.startTimes[i] == tariff.startTimes[iLowestTariff]) 
      { // Draw circle around the lowest tariff visible, to highlight it
        display.drawCircle(xCurrent + xCoeff/2, yTarrif-xCoeff/2, xCoeff/2, SCREEN_BLUE);
      }
      
      display.fillRect(
            xCurrent
          , yTarrif
          , xCoeff-2
          , hTarrif, color);
    }
}