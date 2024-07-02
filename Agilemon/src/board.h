#pragma once

#if defined(LILYGO_T5_V213)

// SPI
const int DIN_PIN = 23;	//SPI MOSI pin, data input
const int SCLK_PIN = 18;	//SPI CLK pin, clock signal input
const int CS_PIN = 5;	//Chip selection, low active

const int DC_PIN = 17;	//Data/command, low for commands, high for data
const int RST_PIN = 16;	//Reset, low active
const int BUSY_PIN = 4;	//Busy status output pin (means busy)

const int LED_PIN = 22; //< LED
const bool LED_ON = LOW;

const int BUTTON_PIN = 39;

#define HAS_BATTERY 1
const int BATTERY_PIN = 35;

#elif defined(WAVESHARE_EPAPER)

// SPI
const int DIN_PIN = 14;	//SPI MOSI pin, data input
const int SCLK_PIN = 13;	//SPI CLK pin, clock signal input
const int CS_PIN = 15;	//Chip selection, low active

const int DC_PIN = 27;	//Data/command, low for commands, high for data
const int RST_PIN = 26;	//Reset, low active
const int BUSY_PIN = 25;	//Busy status output pin (means busy)

const int LED_PIN = 2; //< GPIO2 has LED
const bool LED_ON = HIGH;

const int BUTTON_PIN = 0;
#else
#warning "Define board"
#endif