#include <WiFi.h>
#include "time.h"
#include <esp_wifi.h>
#include <esp_sntp.h>
#include <WiFiClientSecure.h>
#include <esp_crt_bundle.h>
#include <ssl_client.h>

#define RAPIDJSON_DEFAULT_ALLOCATOR ::rapidjson::CrtAllocator
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <limits>
#include <vector> //< TODO: Remove?
#include <algorithm> //< std::clamp


#include "Tariff.hpp"

//TODO: Migrate to using SDF text rendering - smoother  scaling + reduced memory usage goals
//https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include "ColourEPaper.h"

const char firmwareDate[] = "07/02/2024";

/** User-provided configuration that contains SSID, WiFi wifiPassword & Octopus personal authorisation code
@code
  const char* wifiSsid = "WiFi SSID";
  const char* wifiPassword = "WiFi Password";
  const char* auth_string = "API Authorisation Code from Octopus Energy Account";
@endcode
*/
#include "secrets.h"
#include "board.h"

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

#if 1
/// - 600*448 == 256 KB for Video so < 150 KB for everything else assuming 400KB SRAM (520 for WROOM)
const int EPD_4IN01F_WIDTH = 640;
const int EPD_4IN01F_HEIGHT = 400;
#else //5.65 inch AC057TC1
/// - 600*448 == 262 KB for Video so < 138 KB for everything else assuming 400KB SRAM (520 for WROOM)
const int EPD_4IN01F_WIDTH = 600;
const int EPD_4IN01F_HEIGHT = 440; //< Free up 8 lines for JSON parsing?
#endif


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
// - Good Display datasheet https://www.good-display.com/product/381.html
ColourEPaper display(
    SCREEN_WIDTH
  , SCREEN_HEIGHT
  , EPD_RESET
  , EPD_DC
  , EPD_BUSY);

void getOctopusTariff();             // Get Octopus Data
void timeavailable(struct timeval* t);  // Callback function (get's called when time adjusts via NTP)
void drawStats();                      // Routine Refresh of Display
void drawGraph();                       // Draw tariff graph

const char* server = "api.octopus.energy";  // Server URL
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";

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


Tariff tariff;

uint8_t iCurrentTariff = 0;// to hold live tariff for display
uint8_t iLowestTariff = 0;  // to store lowest tariff & time slot present in available data
uint8_t iHighestTariff = 0;  // to store lowest tariff & time slot present in available data

uint8_t iNextLow = 0;

bool haveLocalTime = false; //< IF we have NTP time @todo Deprecate by fixing orde rof logic/events!
Time currentTime; //< Current time in seconds since Epoch

int rssi = 0; //< Last Wifi RSSI

long int nextTariffUpdate = 0;                  // used to store millis() of last tariff update
const int tariffUpdateIntervalSec = 60 * 60 ;  // millis() between successive tariff updates from Octopus (3600000ms = 1h, 10800s = 3h, 14400s = 4h)
const int tariffRetryIntervalSec = 30 ; //< Prevent API spamming for retries

const int displayMinimumUpdateInterval = 30; /// Don't update display faster than this
const int displayUpdateIntervalSec =  15 * 60;             // interval between checks of current tariff data against tariffThreshold
long int nextDisplayUpdate = 0;

WiFiClientSecure client;

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

#if HAS_BATTERY
uint8_t getBatteryPercent(void)
{
    float voltage = analogRead( BATTERY_ADC ) / 4096.0 * 7.46; 
    uint8_t percentage = 100; 
    if (voltage > 1)
    { 
      // Only display if there is a valid reading 
      Serial.println("Voltage = " + String(voltage)); 

      //TODO: update polynomial and remove need for pow!
      percentage = 2836.9625f * std::pow(voltage, 4) 
        - 43987.4889f * std::pow(voltage, 3) 
        + 255233.8134f * std::pow(voltage, 2)
        - 656689.7123f * voltage 
        + 632041.7303f; 
      if (voltage >= 4.20) 
        percentage = 100; 
      if (voltage <= 3.50) 
        percentage = 0; 
        
      Serial.println("Percentage = " + String(percentage)); 
    }
    return percentage;
}
#endif

void setup()
 {
  
  // initialize digital pin LED_PIN as an output.
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_ON);  // turn the LED on

  //Initialize serial and wait for port to open:
  Serial.begin(115200);

  if (!headless)
  {    
    display.cp437(true); //< Use correct character tables
    display.setTextWrap(false); 
    
    if ( !display.begin(SPI_SCLK, SPI_MOSI, EPD_CS)) {
      Serial.println(F("ePaper allocation failed"));
      for (;;);  // Don't proceed, loop forever
    }
  }

      Serial.println(F("testing display"));
  display.test();
      Serial.println(F("testing done"));

  
  // Time Setup
  sntp_set_time_sync_notification_cb(timeavailable);

  //https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32/api-reference/system/system_time.html#sntp-time-synchronization
  configTime(0, 0, ntpServer1, ntpServer2);  // 0, 0 because we will use TZ in the next line

  // NTP server address could be aquired via DHCP,
  sntp_servermode_dhcp(1);  // (optional)
  
  setenv("TZ", posixTimeZone, 1); // Set tiem zone
  tzset();  
  
  WiFi.onEvent(WiFiStationStarted, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_START);
  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);  
  WiFi.onEvent(WiFiStationStopped, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_STOP);

  
  display.waitForScreenBlocking();
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

      esp_wifi_sta_get_rssi(&rssi);
      
      getOctopusTariff();  

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

      // Find last tarriff for current time
      auto itCurrent = std::find_if( tariff.startTimes, tariff.startTimes+tariff.numRecords, []( Time time ) { return time <= currentTime; } );
      iCurrentTariff = std::distance(tariff.startTimes,itCurrent);

      // Find minimum & maximum tariff extremes
      auto itMinmaxTariffPrice = std::minmax_element( tariff.prices, tariff.prices+iCurrentTariff );
      iLowestTariff = std::distance( tariff.prices, itMinmaxTariffPrice.first );
      iHighestTariff = std::distance( tariff.prices, itMinmaxTariffPrice.second );
     
      Price pricesLoG[Tariff::MaxRecords]; 
          
#if 1
      // Find the soonest low before the high or low we know about
      // + (iCurrentTariff/5) to avoid overlapping the current min/max
      auto nextLowWindow = std::max( iLowestTariff, iHighestTariff) + (iCurrentTariff/5);
      if ( iCurrentTariff - nextLowWindow > 4 ) //< TODO: Should be time or generlly just better selection of a wide enough window
      {
        auto itNextLow = std::min_element( tariff.prices+nextLowWindow, tariff.prices+iCurrentTariff );
        iNextLow = std::distance( tariff.prices, itNextLow);
      }
      else
      {
        iNextLow = -1; //< Next low is lowest in tariff
      }

#else // Trough detection... TODO: sensitive to noise!
    // Laplacian of Gaussian (LoG) calculation
      // Compute the second derivative (Laplacian)
      for (size_t i = 1; i < iCurrentTariff - 1; ++i)
       {
          pricesLoG[i] = tariff.prices[i - 1] - 2 * tariff.prices[i] + tariff.prices[i + 1];
      }
    // Detect zero crossings using std::adjacent_find
  Price* itZeroCrossing = pricesLoG-1; //< -1 for first iteration as we +1 this back to begin
    do
    {
      iNextLow = std::distance( pricesLoG, itZeroCrossing );
        itZeroCrossing = std::adjacent_find(itZeroCrossing+1, pricesLoG+iCurrentTariff,
            []( Price lhs, Price rhs )
            {
                return (lhs > 0 && rhs < 0) && (lhs-rhs) > 2.5;
            });

    } while (itZeroCrossing <  pricesLoG+iCurrentTariff);
#endif
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
      digitalWrite(LED_PIN, !LED_ON);   // turn the LED off by making the voltage LOW

      nextDisplayUpdate = millis() + (timeToRefresh * 1000);
      
      esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, LOW);
      esp_deep_sleep( timeToRefresh * 1000 * 1000);
  }
} 

void getOctopusTariff()  // Get Octopus Data
{  
  Serial.println("\nBegin get octopus data...");
  client.setCACert(octopus);
  Serial.println("\nStarting connection to Octopus server...");
  if (!client.connect(server, 443)) {

    char buff[96];
    if (client.lastError(buff, sizeof(buff)) < 0) {
        Serial.println(buff);
    }
    Serial.println("Connection error");
    return;
  }

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
  
  const auto contentLengthTag = "Content-Length";
  size_t contentLength = 0;
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
     // Serial.printf("Response= %s\n", response.c_str() );
      // TODO: CHeck the headers and read `Content-Length` etc
    if ( contentLength == 0 && response.startsWith( contentLengthTag ) )
    {
      const auto contentLengthStr = response.c_str() + strlen(contentLengthTag) + 2; //< +2 to skip ": "
      contentLength = atoi(contentLengthStr);
      
      Serial.printf("Got Content-Length = %u\n", contentLength );
    }
    else
    if (response == "\r") {
      Serial.println("headers received");
      break;
    }
  }

  if ( contentLength == 0 )
  {
      Serial.println("FAILED: Didn't get 'Content-Length' header");
    client.stop();
    return;
  }

  //Await receipt of data
  while( client.connected() && !client.available() );
  
  // Poll for data while connected
  std::vector<char> json;
  json.resize(contentLength);

  auto iCursor = json.data();
  const auto iCursorEnd = iCursor + contentLength;
  do
  {
    const auto remain = iCursorEnd-iCursor;
    const auto available = client.available();
    iCursor += client.readBytes( iCursor, std::min(remain,available) );
  }
  while( iCursor != iCursorEnd && (client.connected() || client.available()) ); //< While connected and more data to be received

  client.stop();


  rapidjson::Document doc;
      Serial.println("About to parse JSON");
  rapidjson::ParseResult parseOk = doc.Parse( (char*)json.data(), contentLength );
  if (!parseOk) 
  {
      Serial.printf( "JSON parse error: %s (%u)\n"
          , rapidjson::GetParseError_En(parseOk.Code())
          , static_cast<unsigned>(parseOk.Offset()) );
          
    Serial.println("JSON was:");
    Serial.write( json.data(), contentLength );
  }
  else 
  {
      Serial.println("JSON OK");

    const auto& results = doc["results"].GetArray();

      Serial.println("got results");
    // We only consider the first X records as they are provided latest to oldest
    // @note We only need 48 for a 24hr period
    tariff.numRecords = std::min( (size_t)results.Size(), (size_t)Tariff::MaxRecords );
    
    Serial.print("# of Records is ");
    Serial.println(tariff.numRecords);
    
    for ( int i =0; i < tariff.numRecords; ++i )
    {
      const auto& resultRate = results[i];
      const float price = resultRate["value_inc_vat"].GetFloat();
      tariff.prices[i] = price;
      const auto periodStart = resultRate["valid_from"].GetString();
      struct tm tmpTime;
      strptime(periodStart, "%Y-%m-%dT%H:%M:%SZ", &tmpTime);
      tmpTime.tm_isdst = false;
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

void drawStats()
{    
  Serial.print("Updating display...");

  struct tm timeinfo;
  const time_t posixCurrentTime = currentTime.toPosix();
  localtime_r(&posixCurrentTime, &timeinfo);
  //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  
  display.setFont(nullptr); //< default
  display.setTextSize(2);
  display.setTextColor(SCREEN_BLACK);

  #if HAS_BATTERY
  display.setCursor(SCREEN_WIDTH-115, 4);
  display.print("Batt ");
  display.print(getBatteryPercent());
  display.print("%");
  #endif
  
  display.setCursor(SCREEN_WIDTH-115, 34);
  
  display.print("Rssi ");
  display.print(rssi);
  //RSSI > -30 dBm	 Amazing
  //RSSI < – 55 dBm	 Very good signal
  //RSSI < – 67 dBm	 Fairly Good
  //RSSI < – 70 dBm	 Okay
  //RSSI < – 80 dBm	 Not good
  //RSSI < – 90 dBm	 Extremely weak signal (unusable)

  display.setFont(&FreeSansBold12pt7b);
  display.setTextSize(1);
  display.setTextColor(SCREEN_BLACK);
  display.setCursor(0, 16);
  display.print(&timeinfo, "%A, %B %d @ %H:%M:%S");
  
  
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
void timeavailable(struct timeval* t) 
{
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
    const auto markerHeight = 15;
    const auto markerWidth = 8;
    const auto xCurrent = (iCurrentTariff - iTariff) * xCoeff;
    const auto hTariff = int(tariff.prices[iTariff] * tariffYScale);
    const auto yMarker = yTariff - ((hTariff > 0) ? hTariff : 0);
   
    display.setTextSize(1);
    display.setTextColor(colour, SCREEN_WHITE);


    // Pad between marker and text and text lines
    const auto lineSpacing = 4;
    auto yCursor = yMarker - markerHeight - lineSpacing;
    
    char text[64];

#if false 
    // @note Todo: Time is printed incorrectly for Daylight Savings... maybe should use localtime_r etc.
    const auto time24 = Time24::fromSecondsTimepoint(tariff.startTimes[iTariff] - currentDayStart);
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
#endif

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

void drawGraph() 
{
    const int left = 0;
    const int top = SCREEN_HEIGHT / 2;
    const int w = SCREEN_WIDTH;
    const int h = SCREEN_HEIGHT / 2 - 50;

    //  display.drawLine(0, 15, 0, 63, SCREEN_BLACK);  // Draw Axes
    display.drawLine(left, top + h, w, top + h, SCREEN_BLACK);
    int i = 1;
    const Time currentTariffTime = currentTime.roundDown(Time::HalfHour);
    const Time currentDayStart = currentTariffTime.roundDown(Time::Day);

    const auto barCount = (tariff.startTimes[0] - currentTariffTime + Time::HalfHour) / Time::HalfHour;
    
    //< Note: We round to nearest, this may mean we miss some data in far-future.
    // - This is better than when we have 24 bars and rounding down would leave a massive gap on right side
    const auto xCoeff = (w + (barCount/2)) / barCount; 

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
    
  struct tm timeinfo;
  const time_t posixCurrentTime = currentHourEnd.toPosix();
  localtime_r(&posixCurrentTime, &timeinfo);

    const uint8_t firstHour = timeinfo.tm_hour;// (currentHourEnd - currentDayStart) / Time::Hour + ;
  
    display.setTextSize(1);
    display.setTextColor(SCREEN_BLACK);

    const auto iHourMarkerBase = iCurrentTariff-iFirstHour;
    const auto hourCount = (iCurrentTariff-1)/2;
    const auto iHourStep = (hourCount > 12) + 1; // @note X-Axis labels otherwise overlap!
    for (int iHour = 0; iHour <= hourCount; iHour += iHourStep)
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
    if (iNextLow != -1)
    {
        // Draw triangle above the lowest tariff visible, to highlight it
        drawTariffMarker(currentDayStart,  xCoeff, yTariff, iNextLow, SCREEN_BLUE);
    }
}
