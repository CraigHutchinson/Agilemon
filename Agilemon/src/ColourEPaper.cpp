#include "ColourEPaper.h"

//https://www.laskakit.cz/user/related_files/gdep0565d90.pdf
enum Command
{
    /** TODO: 2 bytes data */
      PanelSetting = 0b0
    , PowerOff = 0b00000010 // 0x02
    , PowerOn  = 0b00000100 // 0x04

    /** Refresh display according to SRAM data and LUT
     * After Display refresh, BUSY_N signal becomes '0' 
     * until diplsay update is finished
     * DataPayload = N/A
     */
    , DisplayRefresh = 0b00010010 // 0x12

    /** Stop data transmission
     * DataPayload = 1-bytes (0=  not all data sent, 1 all has been sent)
     */
    , DataStop = 0b00010001 
};

ColourEPaper::ColourEPaper(int w, int h, int rst_pin, int dc_pin, int busy_pin, int sclk_pin, int copi_pin, int cs_pin ) 
  : Adafruit_GFX(w, h), buffer1(NULL), buffer2(NULL)
{
    dcPin = dc_pin;
    busyPin = busy_pin;
    rstPin = rst_pin;
    csPin = cs_pin;
    
    if (debugOn)
    {
        Serial.println("Setting Constructor pinModes");
    }

    // pinModes
    pinMode(dcPin, OUTPUT);
    pinMode(busyPin, INPUT);
    pinMode(rstPin, OUTPUT);
    pinMode(csPin, OUTPUT);

    // pin and spi pointer allocation
    spi = new SPIClass(HSPI);
    spi->begin(sclk_pin, -1, copi_pin, cs_pin);
    spiSettingsObject = SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0);
}

ColourEPaper::~ColourEPaper()
{
    free(buffer1);
    free(buffer2);

    delete spi;
}

bool ColourEPaper::begin()
{
    // SPI init

    if (debugOn)
    {
        Serial.println("SPI init");
    }
    spi->beginTransaction(spiSettingsObject);

    return frameBufferAndInit();
}

bool ColourEPaper::frameBufferAndInit()
{
    // framebuffer allocation
    if (debugOn)
    {
        Serial.printf("Let's allocate some memory w=%i h=%i\n", WIDTH, HEIGHT );
    }


    // We often won't have enough contiguous memory to represent the frame buffer in a single allocation
    const auto frameSizeBytes = (WIDTH * HEIGHT / 2);
    const auto bufferSize = frameSizeBytes / 2;
    buffer1 = new char[bufferSize];
    
    if (debugOn)
    {
        Serial.printf("First buffer of size %u\n", bufferSize );
    }


    buffer2 = new char[bufferSize];

    if (debugOn)
    {
        Serial.printf("Second buffer of size %u\n", bufferSize );
    }

    if (buffer1 == NULL || buffer2 == NULL)
    {
        if (debugOn)
        {
            Serial.println("Memory not allocated");
        }
        return false;
    }

    if (debugOn)
    {
        Serial.println("Setting everything to 1");
    }
    memset(buffer1, 0x11, bufferSize); // fill everything with white
    memset(buffer2, 0x11, bufferSize); // fill everything with white

    // send initialization commands to screen
    if (debugOn)
    {
        Serial.println("Resetting screen");
    }
    resetScreen();
    if (!(busyHigh()))
    {
        if (debugOn)
        {
            Serial.println("Busy High Failed");
        }
    }

    if (debugOn)
    {
        Serial.println("Reset complete");
    }

    writeSPI(PanelSetting, true);
    union ScanPanelSetting
    {
        struct Bits
        {
            uint8_t _SET : 2;// = 1;
            uint8_t _DONTCARE : 2;// = 0;
            uint8_t verticalScanUp : 1;// = 0;
            uint8_t horizontalScanRight : 1;// = 1;
            uint8_t dcDcConverterOn : 1;// = 1;
            uint8_t controllerReset : 1;// = 0;
        } bit = {};
        uint8_t raw;
    };
    ScanPanelSetting scanSetting;
    scanSetting.bit._SET = 0b11;
    scanSetting.bit._DONTCARE = 0b11;
    scanSetting.bit.verticalScanUp = 0;
    scanSetting.bit.horizontalScanRight = 1;
    scanSetting.bit.dcDcConverterOn = 1;
    scanSetting.bit.controllerReset = 1; //< TODO: Reset on init?
    //Serial.printf( "scanSetting.raw %x\n", (int)scanSetting.raw);
    writeSPI( 0x2f/*scanSetting.raw*/, false);
    writeSPI(0b00001000, false); //TODO: was 0x00 but should be set as per 8.1.1

    //8.1.2 Power Settign Register  
    writeSPI(0x01, true);
    writeSPI(0x37, false); // trying default of 00001000 orig 0x37
    writeSPI(0x00, false); // trying default of 0x01, orig 0x00
    writeSPI(0x05, false);
    writeSPI(0x05, false);

    writeSPI(0x03, true);
    writeSPI(0x00, false);

    writeSPI(0x06, true);
    writeSPI(0xC7, false);
    writeSPI(0xC7, false);
    writeSPI(0x1D, false);

    writeSPI(0x41, true);
    writeSPI(0x00, false);

    writeSPI(0x50, true);
    writeSPI(0x37, false);

    writeSPI(0x60, true);
    writeSPI(0x22, false);

    writeSPI(0x61, true);
    writeSPI(0x02, false);
    writeSPI(0x80, false);
    writeSPI(0x01, false);
    writeSPI(0x90, false);

    writeSPI(0xE3, true);
    writeSPI(0xAA, false);

    spi->endTransaction();

    if (debugOn)
    {
        Serial.println("Init complete");
    }

    return true;
}

void ColourEPaper::display(void)
{
    spi->beginTransaction(spiSettingsObject);
    setResolution();

    writeSPI(0x10, true);
    if (debugOn)
    {
        Serial.println("Writing to GDDR");
    }
    
    //Data
    digitalWrite(dcPin, HIGH);
    digitalWrite(csPin, LOW);

    spi->transfer( buffer1, (WIDTH * HEIGHT / 2) / 2 );
    spi->transfer( buffer2, (WIDTH * HEIGHT / 2) / 2 );

    digitalWrite(csPin, HIGH);

    if (debugOn)
    {
        Serial.println("Wrote stuff to GDDR");
        // trigger gddr to screen
        Serial.println("Triggering send to screen.");
    }
    writeSPI(PowerOn, true);
    if (!(busyHigh()))
    {
        Serial.println("BusyHigh1 failed");
    }
    writeSPI(DisplayRefresh, true);

    // either block until screen finishes (waitForScreenBlocking) or do something else and then send POF + endtransaction yourself once busy is high (checkBusy + sendPOFandLeaveSPI)
}
void ColourEPaper::setResolution()
{
    writeSPI(0x61, true); // Set Resolution setting

    //0x02, 0x80, 0x01,0x90 = 600x400
    char resolution[4] = {
         char(WIDTH>>8), char(WIDTH & 0xFF),
         char(HEIGHT>>8), char(HEIGHT & 0xFF),
    };

    //Data
    digitalWrite(dcPin, HIGH);
    digitalWrite(csPin, LOW);
    spi->transfer( resolution, sizeof(resolution) );
    digitalWrite(csPin, HIGH);
}
void ColourEPaper::clearDisplay(void)
{
    if (debugOn)
    {
        Serial.println("writing all white to memory");
    }
    memset(buffer1, 0x11, (WIDTH * HEIGHT / 2) / 2);
    memset(buffer2, 0x11, (WIDTH * HEIGHT / 2) / 2);
}

void ColourEPaper::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if (x >= WIDTH || x < 0
     || y >= HEIGHT || y < 0)
    {
        return;
    }
    long pixelNum = (y * WIDTH) + x;
    
    bool after = pixelNum % 2; // if remainder is 0, most significant nibble, if remainder is 1, least significant nibble
    long byteNum = pixelNum / 2;
    
    const bool secondBuffer = byteNum >= (WIDTH * (HEIGHT / 2)) / 2;
    if ( secondBuffer )
    {
        byteNum -= WIDTH * (HEIGHT / 2) / 2;
    }

    // at this point, we know which buffer, which byte, and position
    // now we have to locate the byte, and edit it
    char* buffer = (secondBuffer ? buffer2 : buffer1);
    char newByte = buffer[byteNum]; // get the byte
    buffer[byteNum] = after ? (newByte & 0xF0) | color
                            : (newByte & 0x0F) | (color<<4) ;                 // clear the latter half for new colour
}

void ColourEPaper::test()
{
    Serial.println("EPD::Test - Screen BARS Started");
    // used for testing begin function
    // This function writes bars of each colour to the screen. Use blocking wait function or do it manually with check busy and POF+SPIShutdown
    spi->beginTransaction(spiSettingsObject);
    
    setResolution();
    Serial.println("EPD::Test - Resolution set");

    writeSPI(0x10, true);

    const uint8_t bars[9] = { 0x0, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x77 }; 
    const int16_t barWidth = WIDTH/8;

    for (int16_t j = 0; j < HEIGHT; j++)
    {
        for (int16_t i = 0; i < WIDTH; )
        {
            const int16_t barEnd = std::min( int16_t(i + barWidth), WIDTH);
            const uint8_t barColour  = bars[i/barWidth];
            for ( ; i < barEnd; i += 2 )
            {
                writeSPI( barColour, false);
            }
        }
    }
    
    Serial.println("EPD::Test - Sent all clear commands. Refreshing screen");
    
    writeSPI(PowerOn, true);
    if (!(busyHigh()))
    {
        Serial.println("EPD::Test - BusyHigh1 failed");
    }
    writeSPI(0x12, true);

    Serial.println("EPD::Test - Waiting for screen");
    waitForScreenBlocking();
    Serial.println("EPD::Test - Testing COMPLETE");
}

void ColourEPaper::writeSPI(uint8_t something, bool command)
{
    if (command)
    {
        digitalWrite(dcPin, LOW);
    }
    else
    {
        digitalWrite(dcPin, HIGH);
    }

    digitalWrite(csPin, LOW);

    spi->transfer(something);

    digitalWrite(csPin, HIGH);
}

void ColourEPaper::resetScreen(void)
{

    digitalWrite(rstPin, HIGH);
    delay(200);
    digitalWrite(rstPin, LOW);
    delay(1);
    digitalWrite(rstPin, HIGH);
    delay(200);
}

bool ColourEPaper::busyHigh()
{
    unsigned long endTime = millis() + BUSY_THRESH;
    while (!digitalRead(busyPin) && (millis() < endTime))
        ;

    if (digitalRead(busyPin) == 0)
    {
        return false;
    }

    return true;
}

bool ColourEPaper::busyLow()
{
    unsigned long endTime = millis() + BUSY_THRESH;
    while (digitalRead(busyPin) && (millis() < endTime))
        ;

    if (digitalRead(busyPin) == 1)
    {
        return false;
    }

    return true;
}

void ColourEPaper::waitForScreenBlocking(void)
{
    if (debugOn)
    {
        Serial.println("Blocking wait for screen to finish updating");
    }
    while (!digitalRead(busyPin))
        ;
    sendPOFandLeaveSPI();
}

void ColourEPaper::sendPOFandLeaveSPI(void)
{
    if (debugOn)
    {
        Serial.println("Shutting off and leaving SPI");
    }

    writeSPI(PowerOff, true);
    if (!(busyLow()))
    {
        if (debugOn)
        {
            Serial.println("BusyLow1 failed");
        }
    }
    spi->endTransaction();
}
/*
// ENter sleep mode
void EPD_4IN01F_Sleep(void)
{
    DEV_Delay_ms(100);
    EPD_4IN01F_SendCommand(0x07);
    EPD_4IN01F_SendData(0xA5);
}

static void EPD_4IN01F_BusyHigh(void)// If BUSYN=0 then waiting
{
    while(!(digitalRead(PIN_SPI_BUSY)));
}

static void EPD_4IN01F_BusyLow(void)// If BUSYN=1 then waiting
{
    while(digitalRead(PIN_SPI_BUSY));
}

static void EPD_4IN01F_Show(void)
{
    EPD_SendCommand(PowerOn);//0x04
    EPD_4IN01F_BusyHigh();
    EPD_SendCommand(0x12);//0x12
    EPD_4IN01F_BusyHigh();
    EPD_SendCommand(0x02);//0x02
    EPD_4IN01F_BusyLow();
	delay(200);
    Serial.print("EPD_4IN01F_Show END\r\n");

    delay(100);     
    EPD_SendCommand(0x07);//sleep
    EPD_SendData(0xA5);
    delay(100);
}

int EPD_4IN01F_init() 
{
    EPD_Reset();
    EPD_4IN01F_BusyHigh();
    EPD_SendCommand(0x00);
    EPD_SendData(0x2f);
    EPD_SendData(0x00);
    EPD_SendCommand(0x01);
    EPD_SendData(0x37);
    EPD_SendData(0x00);
    EPD_SendData(0x05);
    EPD_SendData(0x05);
    EPD_SendCommand(0x03);
    EPD_SendData(0x00);
    EPD_SendCommand(0x06);
    EPD_SendData(0xC7);
    EPD_SendData(0xC7);
    EPD_SendData(0x1D);
    EPD_SendCommand(0x41);
    EPD_SendData(0x00);
    EPD_SendCommand(0x50);
    EPD_SendData(0x37);
    EPD_SendCommand(0x60);
    EPD_SendData(0x22);
    EPD_SendCommand(0x61);
    EPD_SendData(0x02);
    EPD_SendData(0x80);
    EPD_SendData(0x01);
    EPD_SendData(0x90);
    EPD_SendCommand(0xE3);
    EPD_SendData(0xAA);
	
	EPD_SendCommand(0x61);//Set Resolution setting
    
    //Data
    digitalWrite(dcPin, HIGH);
    digitalWrite(csPin, LOW);
    spi->transfer( &WIDTH, 2 );
    spi->transfer( &HEIGHT, 2 );
    digitalWrite(csPin, HIGH);
    
    EPD_SendCommand(0x10);//begin write data to e-Paper
	
    return 0;
}
*/

bool ColourEPaper::checkBusy(void)
{
    if (debugOn)
    {
        Serial.println("Blocking wait for busy to be high");
    }
    return !(digitalRead(busyPin));
}