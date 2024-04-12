
#if defined(LILYGO_T5_V213)

#define EPD_MOSI                (23)
#define EPD_MISO                (-1)
#define EPD_SCLK                (18)
#define EPD_CS                  (5)

#define EPD_BUSY                (4)
#define EPD_RSET                (16)
#define EPD_DC                  (17)

#define SDCARD_CS               (13)
#define SDCARD_MOSI             (15)
#define SDCARD_MISO             (2)
#define SDCARD_SCLK             (14)

#define BUTTON_1                (39)
#define BUTTONS                 {39}

#define BUTTON_COUNT            (1)

#define LED_PIN                 (19)
#define LED_ON                  (LOW)

#define ADC_PIN                 (35)

#define _HAS_ADC_DETECTED_
#define _HAS_LED_
#define _HAS_SDCARD_

#else

#endif