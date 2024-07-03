#pragma once

#if defined(LILYGO_TWRIST_V1_1)
// PIN_MOTOR= 4
// PIN_KEY GPIO_NUM_35
// PWR_EN 5
// Backlight 33
// SRAM_CS -1


    // SPI
    const int SPI_MOSI = 13;	//SPI MOSI pin, data input
    const int SPI_SCLK = 14;	//SPI CLK pin, clock signal input
    const int EPD_CS = 15;	//Chip selection, low active
    const int EPD_DC = 2;	//Data/command, low for commands, high for data
    const int EPD_RESET = 17;	//Reset, low active
    const int EPD_BUSY = 16;	//Busy status output pin (means busy)

    const int LED_PIN = 33; //< BACKLIGHT!
    const bool LED_ON = LOW;

    const int BUTTON_PIN = 5; //PWR_EN

    #define HAS_BATTERY 1
    const int BATTERY_ADC = 34;


#elif defined(LILYGO_T5_V213)

    // SPI
    const int SPI_MOSI = 23;	//SPI MOSI pin, data input
    const int SPI_SCLK = 18;	//SPI CLK pin, clock signal input
    const int EPD_CS = 5;	//Chip selection, low active

    const int EPD_DC = 17;	//Data/command, low for commands, high for data
    const int EPD_RESET = 16;	//Reset, low active
    const int EPD_BUSY = 4;	//Busy status output pin (means busy)

    const int LED_PIN = 22; //< LED
    const bool LED_ON = LOW;

    const int BUTTON_PIN = 39;

    #define HAS_BATTERY 1
    const int BATTERY_ADC = 35;

#elif defined(WAVESHARE_EPAPER)

    // SPI
    const int SPI_MOSI = 14;	//SPI MOSI pin, data input
    const int SPI_SCLK = 13;	//SPI CLK pin, clock signal input
    const int EPD_CS = 15;	//Chip selection, low active

    const int EPD_DC = 27;	//Data/command, low for commands, high for data
    const int EPD_RESET = 26;	//Reset, low active
    const int EPD_BUSY = 25;	//Busy status output pin (means busy)

    const int LED_PIN = 2; //< GPIO2 has LED
    const bool LED_ON = HIGH;

    const int BUTTON_PIN = 0;
#else
#warning "Define board"
#endif