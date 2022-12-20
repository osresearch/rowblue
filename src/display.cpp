/*
 * Draw the rowing machine status to the display
 * esphome config:
# 240x135
display:
  - platform: st7789v
    backlight_pin: GPIO2
    cs_pin: GPIO5
    dc_pin: GPIO16
    reset_pin: GPIO23
    update_interval: 1s
 */
#include <SPI.h>
#include <Wire.h>
#include <Arduino.h>


// rotated 90 deg
//#define CGRAM_OFFSET
//#define TFT_BACKLIGHT_ON 1
//#define LOAD_GLCD

//#define LOAD_GFXFF
//#define SMOOTH_FONT
//#define SPI_FREQUENCY 20000000
//#define SPI_READ_FREQUENCY 6000000

//#define USER_SETUP_INFO "esp32 ttgo"
//#define USER_SETUP_LOADED


#include <TFT_eSPI.h>
#include "DinBold15pt7b.h"
#include "DinBold32pt7b.h"

TFT_eSPI    display  = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT);
#define DISPLAY_WIDTH   display.width()
#define DISPLAY_HEIGHT  display.height()


void display_setup()
{
	display.init();
	display.setRotation(1); // 90 degrees

#ifdef CONFIG_TFT_BL
	// TFT_BL has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
	// Set backlight pin to output mode
	pinMode(CONFIG_TFT_BL, OUTPUT);
	// Turn backlight on. TFT_BACKLIGHT_ON has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
	// should do pwm instead for brightness control
	digitalWrite(CONFIG_TFT_BL, 1);
#endif


	//display.fillScreen(TFT_WHITE);
	//display.setSwapBytes(true);
	//display.pushImage(display.width(), display.width(), display.height(), display.height(), rower_bits);

	display.setTextSize(1);
	display.setFreeFont(&DinBold32pt7b);
	display.setTextColor(TFT_WHITE, TFT_BLUE);
	display.drawString("RowBlue", display.width() / 2, display.height() / 2);

	delay(300);
	display.fillScreen(TFT_BLACK);
}

static float frac(float x)
{
	return x - (int) x;
}

void display_loop(float spm, float vel)
{
	display.fillScreen(TFT_BLACK);
	display.setTextSize(1);
	display.setTextDatum(BR_DATUM);
	display.setFreeFont(&DinBold15pt7b);
	display.setTextColor(TFT_BLUE, TFT_BLACK);
	display.drawString("SPM", display.width(), display.height());
	display.drawString("M/S", display.width(), display.height()/2);

	display.setFreeFont(&DinBold32pt7b);
	if (spm < 1)
		display.setTextColor(TFT_DARKGREY, TFT_BLACK);
	else
		display.setTextColor(TFT_WHITE, TFT_BLACK);
	display.drawString(String(spm,0), display.width() - 70, display.height());
	display.setFreeFont(&DinBold15pt7b);
	display.drawString(String(frac(spm)*10,0), display.width() - 40, display.height() - 30);

	if (vel < 1)
		display.setTextColor(TFT_DARKGREY, TFT_BLACK);
	else
		display.setTextColor(TFT_WHITE, TFT_BLACK);
	display.setFreeFont(&DinBold32pt7b);
	display.drawString(String(vel,0), display.width() - 70 , display.height()/2);
	display.setFreeFont(&DinBold15pt7b);
	display.drawString(String(frac(vel)*10,0), display.width() - 40, display.height()/2 - 30);
}



void display_off()
{
#ifdef CONFIG_TFT_BL
	digitalWrite(CONFIG_TFT_BL, 0);
#endif

	//display.writecommand(TFT_DISPOFF);
	//display.writecommand(TFT_SLPIN);
}
