#include <lvgl.h>
#include <ESP_Panel_Library.h>

#include "icon_fuelpressure.h"
#include "icon_stall.h"
#include "icon_oilpressure.h"
#include "icon_watertemp.h"

#include "font_gear.h"
#include "font_messages.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// ESP_Panel_Library configuration according to ESP32-8048S043 spec (LCD) ////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define LCD_NAME                        ST7262
#define LCD_WIDTH                       (800)
#define LCD_HEIGHT                      (480)
#define LCD_COLOR_BITS                  (24)
#define LCD_RGB_DATA_WIDTH              (16)
#define LCD_RGB_TIMING_FREQ_HZ          (16 * 1000 * 1000)
#define LCD_RGB_TIMING_HPW              (4)  // hsync pulse width
#define LCD_RGB_TIMING_HBP              (8)  // hsync back porch
#define LCD_RGB_TIMING_HFP              (8)  // hsync front porch
#define LCD_RGB_TIMING_VPW              (4)  // vsync pulse width
#define LCD_RGB_TIMING_VBP              (8)  // vsync back porch
#define LCD_RGB_TIMING_VFP              (8)  // vsync front porch

#define LCD_PIN_NUM_RGB_DISP            (-1) // not connected
#define LCD_PIN_NUM_RGB_VSYNC           (41)
#define LCD_PIN_NUM_RGB_HSYNC           (39)
#define LCD_PIN_NUM_RGB_DE              (40)
#define LCD_PIN_NUM_RGB_PCLK            (42)

/* original pin layout as found in the board's documentation
#define LCD_PIN_NUM_RGB_DATA0           (45) // r0
#define LCD_PIN_NUM_RGB_DATA1           (48) // r1
#define LCD_PIN_NUM_RGB_DATA2           (47) // r2
#define LCD_PIN_NUM_RGB_DATA3           (21) // r3
#define LCD_PIN_NUM_RGB_DATA4           (14) // r4
#define LCD_PIN_NUM_RGB_DATA5           (5)  // g0
#define LCD_PIN_NUM_RGB_DATA6           (6)  // g1
#define LCD_PIN_NUM_RGB_DATA7           (7)  // g2
#define LCD_PIN_NUM_RGB_DATA8           (15) // g3
#define LCD_PIN_NUM_RGB_DATA9           (16) // g4
#define LCD_PIN_NUM_RGB_DATA10          (4)  // g5
#define LCD_PIN_NUM_RGB_DATA11          (8)  // b0
#define LCD_PIN_NUM_RGB_DATA12          (3)  // b1
#define LCD_PIN_NUM_RGB_DATA13          (46) // b2
#define LCD_PIN_NUM_RGB_DATA14          (9)  // b3
#define LCD_PIN_NUM_RGB_DATA15          (1)  // b4
*/

// color order reversed to BGR compared to what is in the original documentation
// (this version displays the correct colors)
#define LCD_PIN_NUM_RGB_DATA0           (8)  // r0
#define LCD_PIN_NUM_RGB_DATA1           (3)  // r1
#define LCD_PIN_NUM_RGB_DATA2           (46) // r2
#define LCD_PIN_NUM_RGB_DATA3           (9)  // r3
#define LCD_PIN_NUM_RGB_DATA4           (1)  // r4
#define LCD_PIN_NUM_RGB_DATA5           (5)  // g0
#define LCD_PIN_NUM_RGB_DATA6           (6)  // g1
#define LCD_PIN_NUM_RGB_DATA7           (7)  // g2
#define LCD_PIN_NUM_RGB_DATA8           (15) // g3
#define LCD_PIN_NUM_RGB_DATA9           (16) // g4
#define LCD_PIN_NUM_RGB_DATA10          (4)  // g5
#define LCD_PIN_NUM_RGB_DATA11          (45) // b0
#define LCD_PIN_NUM_RGB_DATA12          (48) // b1
#define LCD_PIN_NUM_RGB_DATA13          (47) // b2
#define LCD_PIN_NUM_RGB_DATA14          (21) // b3
#define LCD_PIN_NUM_RGB_DATA15          (14) // b4

#define LCD_PIN_NUM_RST                 (-1) // not connected
#define LCD_PIN_NUM_BK_LIGHT            (2)
#define LCD_BK_LIGHT_ON_LEVEL           (1)

#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL

// Enable or disable printing RGB refresh rate
#define ENABLE_PRINT_LCD_FPS            (1)

#define _LCD_CLASS(name, ...) ESP_PanelLcd_##name(__VA_ARGS__)
#define LCD_CLASS(name, ...)  _LCD_CLASS(name, ##__VA_ARGS__)

ESP_PanelLcd *lcd;

#if ENABLE_PRINT_LCD_FPS
#define LCD_FPS_COUNT_MAX               (100)

DRAM_ATTR int frame_count = 0;
DRAM_ATTR int fps = 0;
DRAM_ATTR long start_time = 0;

IRAM_ATTR bool onVsyncEndCallback(void *user_data)
{
    long frame_start_time = *(long *)user_data;
    if (frame_start_time == 0)
    {
      (*(long *)user_data) = esp_timer_get_time() / 1000;
      return false;
    }

    frame_count++;
    if (frame_count >= LCD_FPS_COUNT_MAX)
    {
      fps = LCD_FPS_COUNT_MAX * 1000 / (esp_timer_get_time() / 1000 - frame_start_time);
      frame_count = 0;
      (*(long *)user_data) = esp_timer_get_time() / 1000;
    }

    return false;
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// ESP_Panel_Library configuration according to ESP32-8048S043 spec (touch) //////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define TOUCH_NAME                GT911
#define TOUCH_WIDTH               (480)
#define TOUCH_WIDTH_SCALE_FACTOR  (1.6666)      // (LCD_WIDTH / TOUCH_WIDTH = 800 / 480)
#define TOUCH_HEIGHT              (272)
#define TOUCH_HEIGHT_SCALE_FACTOR (1.7647)      // (LCD_HEIGHT / TOUCH_HEIGHT = 480 / 272)
#define TOUCH_I2C_FREQ_HZ         (400 * 1000)
#define TOUCH_READ_POINTS_NUM     (5)           // maximum number of simultaneous touch points

#define TOUCH_PIN_NUM_I2C_SCL     (20)
#define TOUCH_PIN_NUM_I2C_SDA     (19)
#define TOUCH_PIN_NUM_RST         (38)
#define TOUCH_PIN_NUM_INT         (-1)

#define _TOUCH_CLASS(name, ...) ESP_PanelTouch_##name(__VA_ARGS__)
#define _TOUCH_CLASS(name, ...)  _TOUCH_CLASS(name, ##__VA_ARGS__)

ESP_PanelTouch *touch = nullptr;
ESP_PanelTouchPoint point[TOUCH_READ_POINTS_NUM];

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// LVGL support functions                                                 ////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define TFT_ROTATION   LV_DISPLAY_ROTATION_0
lv_display_t *disp;                             // handler for display device
lv_indev_t *indev;                              // handler for touch device
lv_obj_t *screen_gauges, *screen_carselection;  // handler for screen layouts

// LVGL draw into this buffer, 1/10 screen size usually works well. The size is in bytes
#define DRAW_BUF_SIZE (LCD_WIDTH * LCD_HEIGHT / 10 * (LCD_RGB_DATA_WIDTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

#if LV_USE_LOG != 0
void my_print( lv_log_level_t level, const char * buf )
{
    LV_UNUSED(level);
    Serial.println(buf);
    Serial.flush();
}
#endif

// LVGL calls it when a rendered image needs to copied to the display
void my_disp_flush( lv_display_t *disp, const lv_area_t *area, uint8_t * px_map)
{
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;
    lcd->drawBitmap(offsetx1, offsety1, offsetx2 - offsetx1 + 1, offsety2 - offsety1 + 1, (const uint8_t *)px_map);
    
    // Call it to tell LVGL you are ready
    lv_display_flush_ready(disp);
}

// Read the touchpad
void my_touchpad_read( lv_indev_t * indev, lv_indev_data_t * data )
{
  ESP_PanelTouchPoint point[TOUCH_READ_POINTS_NUM];
  int read_touch_result = touch->readPoints(point, TOUCH_READ_POINTS_NUM, -1);
  float coord_x, coord_y;

  // Send LVGL only the first touch point from the simultaneous touch points
  if (read_touch_result > 0)
  {
    coord_x = point[0].x * TOUCH_WIDTH_SCALE_FACTOR;
    coord_y = point[0].y * TOUCH_HEIGHT_SCALE_FACTOR;
    
    data->point.x = (int32_t) coord_x;
    data->point.y = (int32_t) coord_y;
    data->state = LV_INDEV_STATE_PRESSED;
    Serial.printf("Touch point sent: x %d, y %d\n", data->point.x, data->point.y);
    
    // print all touch coordinates for debug purposes
    for (int i = 0; i < read_touch_result; i++)
      Serial.printf("Touch point(%d): x %d, y %d, strength %d\n", i, point[i].x, point[i].y, point[i].strength);
  }
  else data->state = LV_INDEV_STATE_RELEASED;
}

// Tick source
static uint32_t my_tick(void)
{
  return esp_timer_get_time() / 1000;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// iRDash global variables                                                ////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define NAMELENGTH 10
// define car identification numbers
#define NUMOFCARS   10   // number of car profiles, maximum is 16
#define NUMOFGEARS  10   // maximum number of gears including reverse and neutral
#define DEFAULTCAR  9    // car profile to show at startup
#define ID_Skippy   0    // Skip Barber
#define ID_CTS_V    1    // Cadillac CTS-V
#define ID_MX5_NC   2    // Mazda MX5 NC
#define ID_MX5_ND   3    // Mazda MX5 ND
#define ID_FR20     4    // Formula Renault 2.0
#define ID_DF3      5    // Dallara Formula 3
#define ID_992_CUP  6    // Porsche 911 GT3 cup (992)
#define ID_GR86     7    // Toyota GR86
#define ID_SFL      8    // Super Formula Lights
#define ID_G82_M4   9    // BMW G82 M4

// bit fields of engine warnings for reference
/*enum irsdk_EngineWarnings 
{
  irsdk_waterTempWarning    = 0x01,
  irsdk_fuelPressureWarning = 0x02,
  irsdk_oilPressureWarning  = 0x04,
  irsdk_engineStalled     = 0x08,
  irsdk_pitSpeedLimiter   = 0x10,
  irsdk_revLimiterActive    = 0x20,
}*/

// color shading for engine management lights
// values between 0 - 255; 255 means black, 0 means original color
#define WARNINGLIGHT_ON 0
#define WARNINGLIGHT_OFF 170

// structure of the incoming serial data block
// variables are aligned for 16/32 bit systems to prevent padding
struct SIncomingData
{
  char EngineWarnings;
  char Gear;
  bool IsInGarage;
  bool IsOnTrack;
  float Fuel;
  float RPM;
  float Speed;
  float WaterTemp;
};

// structure to store the actual and the previous screen data
struct SViewData
{
  char EngineWarnings;
  int Fuel;
  char Gear;
  int RPMgauge;
  int Speed;
  int SLI;
  int WaterTemp;
};

// limits where different drawing color have to be used
struct SCarProfile
{
  char CarName[NAMELENGTH];
  byte ID;
  int Fuel;                   // value which below the warning color is used (value in liter * 10)
  int RPM;                    // max value of RPM gauge
  int WaterTemp;              // value in Celsius
  int SLI[NUMOFGEARS][8];     // RPM values for each SLI light
};

char inByte;                  // incoming byte from the serial port
int blockpos;                 // position of the actual incoming byte in the telemetry data block
SIncomingData* InData;        // pointer to access the telemetry data as a structure
char* pInData;                // pointer to access the telemetry data as a byte array
SViewData Screen[2];          // store the actual and the previous screen data
char ActiveCar;   

// variables to manage screen layout
SCarProfile CarProfile[NUMOFCARS];     // store warning limits for each car

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Support functions                                                      ////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Clear our internal data block
void ResetInternalData()
{
  Screen[0].EngineWarnings = 0;
  Screen[0].Fuel = 0;
  Screen[0].Gear = -1;
  Screen[0].RPMgauge = 0;
  Screen[0].Speed = -1;
  Screen[0].SLI = 0;
  Screen[0].WaterTemp = 0;
  blockpos = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Car profiles                                                           ////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UploadCarProfiles()
{
  /**************************/
  // Skip Barber
  /**************************/
  CarProfile[ID_Skippy].CarName[0] = 'S';
  CarProfile[ID_Skippy].CarName[1] = 'k';
  CarProfile[ID_Skippy].CarName[2] = 'i';
  CarProfile[ID_Skippy].CarName[3] = 'p';
  CarProfile[ID_Skippy].CarName[4] = 'p';
  CarProfile[ID_Skippy].CarName[5] = 'y';
  CarProfile[ID_Skippy].CarName[6] = 0;
  CarProfile[ID_Skippy].ID = ID_Skippy;

  CarProfile[ID_Skippy].Fuel = 25;
  //CarProfile[ID_Skippy].RPM = 291;          // 6000 / RPMscale; where the redline starts on the gauge
  //CarProfile[ID_Skippy].RPMscale = 20.625;  // 6600 / 320; max RPM divided by screen width
  CarProfile[ID_Skippy].WaterTemp = 90;
  CarProfile[ID_Skippy].RPM = 6600;

  for (int i = 0; i<NUMOFGEARS; i++)
  {
    CarProfile[ID_Skippy].SLI[i][0] = 4700;
    CarProfile[ID_Skippy].SLI[i][1] = 4900;
    CarProfile[ID_Skippy].SLI[i][2] = 5100;
    CarProfile[ID_Skippy].SLI[i][3] = 5300;
    CarProfile[ID_Skippy].SLI[i][4] = 5500;
    CarProfile[ID_Skippy].SLI[i][5] = 5700;
    CarProfile[ID_Skippy].SLI[i][6] = 5900;
    CarProfile[ID_Skippy].SLI[i][7] = 6100;
  }
                               
  /**************************/
  // Cadillac CTS-V
  /**************************/
  CarProfile[ID_CTS_V].CarName[0] = 'C';
  CarProfile[ID_CTS_V].CarName[1] = 'T';
  CarProfile[ID_CTS_V].CarName[2] = 'S';
  CarProfile[ID_CTS_V].CarName[3] = '-';
  CarProfile[ID_CTS_V].CarName[4] = 'V';
  CarProfile[ID_CTS_V].CarName[5] = 0;
  CarProfile[ID_CTS_V].ID = ID_CTS_V;

  CarProfile[ID_CTS_V].Fuel = 40;
  //CarProfile[ID_CTS_V].RPM = 288;         // 7200 / RPMscale
  //CarProfile[ID_CTS_V].RPMscale = 25;     // 8000 / 320
  CarProfile[ID_CTS_V].WaterTemp = 110;
  CarProfile[ID_CTS_V].RPM = 8000;

  for (int i = 0; i<NUMOFGEARS; i++)
  {
    CarProfile[ID_CTS_V].SLI[i][0] = 6600;
    CarProfile[ID_CTS_V].SLI[i][1] = 6700;
    CarProfile[ID_CTS_V].SLI[i][2] = 6800;
    CarProfile[ID_CTS_V].SLI[i][3] = 6900;
    CarProfile[ID_CTS_V].SLI[i][4] = 7000;
    CarProfile[ID_CTS_V].SLI[i][5] = 7100;
    CarProfile[ID_CTS_V].SLI[i][6] = 7200;
    CarProfile[ID_CTS_V].SLI[i][7] = 7300;
  }
  
  /**************************/
  // Mazda MX-5 NC
  /**************************/
  CarProfile[ID_MX5_NC].CarName[0] = 'M';
  CarProfile[ID_MX5_NC].CarName[1] = 'X';
  CarProfile[ID_MX5_NC].CarName[2] = '5';
  CarProfile[ID_MX5_NC].CarName[3] = ' ';
  CarProfile[ID_MX5_NC].CarName[4] = 'N';
  CarProfile[ID_MX5_NC].CarName[5] = 'C';
  CarProfile[ID_MX5_NC].CarName[6] = 0;
  CarProfile[ID_MX5_NC].ID = ID_MX5_NC;

  CarProfile[ID_MX5_NC].Fuel = 40;
  //CarProfile[ID_MX5_NC].RPM = 296;           // 6750 / RPMscale
  //CarProfile[ID_MX5_NC].RPMscale = 22.8125;  // 7300 / 320
  CarProfile[ID_MX5_NC].WaterTemp = 100;
  CarProfile[ID_MX5_NC].RPM = 7300;

  for (int i = 0; i<NUMOFGEARS; i++)
  {
    CarProfile[ID_MX5_NC].SLI[i][0] = 6000;
    CarProfile[ID_MX5_NC].SLI[i][1] = 6125;
    CarProfile[ID_MX5_NC].SLI[i][2] = 6250;
    CarProfile[ID_MX5_NC].SLI[i][3] = 6375;
    CarProfile[ID_MX5_NC].SLI[i][4] = 6500;
    CarProfile[ID_MX5_NC].SLI[i][5] = 6625;
    CarProfile[ID_MX5_NC].SLI[i][6] = 6750;
    CarProfile[ID_MX5_NC].SLI[i][7] = 6850;
  }

  /**************************/
  // Mazda MX-5 ND
  /**************************/
  CarProfile[ID_MX5_ND].CarName[0] = 'M';
  CarProfile[ID_MX5_ND].CarName[1] = 'X';
  CarProfile[ID_MX5_ND].CarName[2] = '5';
  CarProfile[ID_MX5_ND].CarName[3] = ' ';
  CarProfile[ID_MX5_ND].CarName[4] = 'N';
  CarProfile[ID_MX5_ND].CarName[5] = 'D';
  CarProfile[ID_MX5_ND].CarName[6] = 0;
  CarProfile[ID_MX5_ND].ID = ID_MX5_ND;

  CarProfile[ID_MX5_ND].Fuel = 40;
  //CarProfile[ID_MX5_ND].RPM = 294;           // 6900 / RPMscale
  //CarProfile[ID_MX5_ND].RPMscale = 23.4375;  // 7500 / 320
  CarProfile[ID_MX5_ND].WaterTemp = 100;
  CarProfile[ID_MX5_ND].RPM = 7500;

  for (int i = 0; i<NUMOFGEARS; i++)
  {
    CarProfile[ID_MX5_ND].SLI[i][0] = 4600;
    CarProfile[ID_MX5_ND].SLI[i][1] = 5000;
    CarProfile[ID_MX5_ND].SLI[i][2] = 5400;
    CarProfile[ID_MX5_ND].SLI[i][3] = 5750;
    CarProfile[ID_MX5_ND].SLI[i][4] = 6100;
    CarProfile[ID_MX5_ND].SLI[i][5] = 6800;
    CarProfile[ID_MX5_ND].SLI[i][6] = 7200;
    CarProfile[ID_MX5_ND].SLI[i][7] = 7200;
  }

  /**************************/
  // Formula Renault 2.0
  /**************************/
  CarProfile[ID_FR20].CarName[0] = 'F';
  CarProfile[ID_FR20].CarName[1] = 'R';
  CarProfile[ID_FR20].CarName[2] = ' ';
  CarProfile[ID_FR20].CarName[3] = '2';
  CarProfile[ID_FR20].CarName[4] = '.';
  CarProfile[ID_FR20].CarName[5] = '0';
  CarProfile[ID_FR20].CarName[6] = 0;
  CarProfile[ID_FR20].ID = ID_FR20;

  CarProfile[ID_FR20].Fuel = 30;
  //CarProfile[ID_FR20].RPM = 307;           // 7300 / RPMscale
  //CarProfile[ID_FR20].RPMscale = 23.75;    // 7600 / 320
  CarProfile[ID_FR20].WaterTemp = 90;
  CarProfile[ID_FR20].RPM = 7600;

  for (int i = 0; i<NUMOFGEARS; i++)
  {
    CarProfile[ID_FR20].SLI[i][0] = 6600;
    CarProfile[ID_FR20].SLI[i][1] = 6700;
    CarProfile[ID_FR20].SLI[i][2] = 6800;
    CarProfile[ID_FR20].SLI[i][3] = 6900;
    CarProfile[ID_FR20].SLI[i][4] = 7000;
    CarProfile[ID_FR20].SLI[i][5] = 7100;
    CarProfile[ID_FR20].SLI[i][6] = 7200;
    CarProfile[ID_FR20].SLI[i][7] = 7300;
  }

  /**************************/
  // Dallara Formula 3
  /**************************/
  CarProfile[ID_DF3].CarName[0] = 'D';
  CarProfile[ID_DF3].CarName[1] = 'a';
  CarProfile[ID_DF3].CarName[2] = 'l';
  CarProfile[ID_DF3].CarName[3] = 'l';
  CarProfile[ID_DF3].CarName[4] = 'F';
  CarProfile[ID_DF3].CarName[5] = '3';
  CarProfile[ID_DF3].CarName[6] = 0;
  CarProfile[ID_DF3].ID = ID_DF3;

  CarProfile[ID_DF3].Fuel = 10;
  //CarProfile[ID_DF3].RPM = 305;           // 7050 / RPMscale
  //CarProfile[ID_DF3].RPMscale = 23.125;   // 7400 / 320
  CarProfile[ID_DF3].WaterTemp = 110;
  CarProfile[ID_DF3].RPM = 7400;

  for (int i = 0; i<NUMOFGEARS; i++)
  {
    CarProfile[ID_DF3].SLI[i][0] = 6450;
    CarProfile[ID_DF3].SLI[i][1] = 6550;
    CarProfile[ID_DF3].SLI[i][2] = 6650;
    CarProfile[ID_DF3].SLI[i][3] = 6750;
    CarProfile[ID_DF3].SLI[i][4] = 6850;
    CarProfile[ID_DF3].SLI[i][5] = 6950;
    CarProfile[ID_DF3].SLI[i][6] = 7050;
    CarProfile[ID_DF3].SLI[i][7] = 7150;
  }

  /**************************/
  // Porsche 992 GT3 cup
  /**************************/
  CarProfile[ID_992_CUP].CarName[0] = '9';
  CarProfile[ID_992_CUP].CarName[1] = '9';
  CarProfile[ID_992_CUP].CarName[2] = '2';
  CarProfile[ID_992_CUP].CarName[3] = 'c';
  CarProfile[ID_992_CUP].CarName[4] = 'u';
  CarProfile[ID_992_CUP].CarName[5] = 'p';
  CarProfile[ID_992_CUP].CarName[6] = 0;
  CarProfile[ID_992_CUP].ID = ID_992_CUP;

  CarProfile[ID_992_CUP].Fuel = 30;
  //CarProfile[ID_992_CUP].RPM = 298;         // 8200 / RPMscale
  //CarProfile[ID_992_CUP].RPMscale = 27.5;   // 8800 / 320
  CarProfile[ID_992_CUP].WaterTemp = 110;
  CarProfile[ID_992_CUP].RPM = 8800;

  for (int i = 0; i<2; i++)
  // reverse, neutral
  {
    CarProfile[ID_992_CUP].SLI[i][0] = 7000; // 1st green
    CarProfile[ID_992_CUP].SLI[i][1] = 7200; // 2nd green
    CarProfile[ID_992_CUP].SLI[i][2] = 7400; // 3rd green
    CarProfile[ID_992_CUP].SLI[i][3] = 7600; // 1st yellow
    CarProfile[ID_992_CUP].SLI[i][4] = 7800; // 2nd yellow
    CarProfile[ID_992_CUP].SLI[i][5] = 8000; // 3rd yellow
    CarProfile[ID_992_CUP].SLI[i][6] = 8200; // 1st red
    CarProfile[ID_992_CUP].SLI[i][7] = 8400; // 2nd red
  } // all blue and blinking is not recorded

  // 1st gear
  CarProfile[ID_992_CUP].SLI[2][0] = 1500;
  CarProfile[ID_992_CUP].SLI[2][1] = 1500;
  CarProfile[ID_992_CUP].SLI[2][2] = 1500;
  CarProfile[ID_992_CUP].SLI[2][3] = 2000;
  CarProfile[ID_992_CUP].SLI[2][4] = 3200;
  CarProfile[ID_992_CUP].SLI[2][5] = 4350;
  CarProfile[ID_992_CUP].SLI[2][6] = 5540;
  CarProfile[ID_992_CUP].SLI[2][7] = 6720;

  // 2nd gear
  CarProfile[ID_992_CUP].SLI[3][0] = 2800;
  CarProfile[ID_992_CUP].SLI[3][1] = 3480;
  CarProfile[ID_992_CUP].SLI[3][2] = 4135;
  CarProfile[ID_992_CUP].SLI[3][3] = 4820;
  CarProfile[ID_992_CUP].SLI[3][4] = 5500;
  CarProfile[ID_992_CUP].SLI[3][5] = 6175;
  CarProfile[ID_992_CUP].SLI[3][6] = 6850;
  CarProfile[ID_992_CUP].SLI[3][7] = 7520;

  // 3rd gear
  CarProfile[ID_992_CUP].SLI[4][0] = 5200;
  CarProfile[ID_992_CUP].SLI[4][1] = 5600;
  CarProfile[ID_992_CUP].SLI[4][2] = 6000;
  CarProfile[ID_992_CUP].SLI[4][3] = 6400;
  CarProfile[ID_992_CUP].SLI[4][4] = 6800;
  CarProfile[ID_992_CUP].SLI[4][5] = 7200;
  CarProfile[ID_992_CUP].SLI[4][6] = 7600;
  CarProfile[ID_992_CUP].SLI[4][7] = 8000;

  // 4th gear
  CarProfile[ID_992_CUP].SLI[5][0] = 6700;
  CarProfile[ID_992_CUP].SLI[5][1] = 6925;
  CarProfile[ID_992_CUP].SLI[5][2] = 7150;
  CarProfile[ID_992_CUP].SLI[5][3] = 7375;
  CarProfile[ID_992_CUP].SLI[5][4] = 7600;
  CarProfile[ID_992_CUP].SLI[5][5] = 7825;
  CarProfile[ID_992_CUP].SLI[5][6] = 8050;
  CarProfile[ID_992_CUP].SLI[5][7] = 8275;

  // 5th gear
  CarProfile[ID_992_CUP].SLI[6][0] = 7600;
  CarProfile[ID_992_CUP].SLI[6][1] = 7725;
  CarProfile[ID_992_CUP].SLI[6][2] = 7850;
  CarProfile[ID_992_CUP].SLI[6][3] = 7975;
  CarProfile[ID_992_CUP].SLI[6][4] = 8100;
  CarProfile[ID_992_CUP].SLI[6][5] = 8225;
  CarProfile[ID_992_CUP].SLI[6][6] = 8350;
  CarProfile[ID_992_CUP].SLI[6][7] = 8475;

  // 6th gear and above
  for (int i = 7; i<NUMOFGEARS; i++)
  {
    CarProfile[ID_992_CUP].SLI[i][0] = 8600;
    CarProfile[ID_992_CUP].SLI[i][1] = 8650;
    CarProfile[ID_992_CUP].SLI[i][2] = 8700;
    CarProfile[ID_992_CUP].SLI[i][3] = 8750;
    CarProfile[ID_992_CUP].SLI[i][4] = 8800;
    CarProfile[ID_992_CUP].SLI[i][5] = 8850;
    CarProfile[ID_992_CUP].SLI[i][6] = 8900;
    CarProfile[ID_992_CUP].SLI[i][7] = 8950;
  }

  /**************************/
  // Toyota GR86
  /**************************/
  CarProfile[ID_GR86].CarName[0] = 'G';
  CarProfile[ID_GR86].CarName[1] = 'R';
  CarProfile[ID_GR86].CarName[2] = '8';
  CarProfile[ID_GR86].CarName[3] = '6';
  CarProfile[ID_GR86].CarName[4] = 0;
  CarProfile[ID_GR86].ID = ID_GR86;
 
  CarProfile[ID_GR86].Fuel = 10;
  //CarProfile[ID_GR86].RPM = 296;            // 6950 / RPMscale
  //CarProfile[ID_GR86].RPMscale = 23.4375;   // 7500 / 320
  CarProfile[ID_GR86].WaterTemp = 95;
  CarProfile[ID_GR86].RPM = 7500;

  for (int i = 0; i<NUMOFGEARS; i++)
  {
    CarProfile[ID_GR86].SLI[i][0] = 6000; // 1st light
    CarProfile[ID_GR86].SLI[i][1] = 6225; // added middle
    CarProfile[ID_GR86].SLI[i][2] = 6450; // 2nd light
    CarProfile[ID_GR86].SLI[i][3] = 6575; // added middle
    CarProfile[ID_GR86].SLI[i][4] = 6700; // 3rd light
    CarProfile[ID_GR86].SLI[i][5] = 6825; // added middle
    CarProfile[ID_GR86].SLI[i][6] = 6950; // 4th light
    CarProfile[ID_GR86].SLI[i][7] = 7250; // all blue
  }

  /**************************/
  // Super Formula Lights
  /**************************/
  CarProfile[ID_SFL].CarName[0] = 'S';
  CarProfile[ID_SFL].CarName[1] = 'F';
  CarProfile[ID_SFL].CarName[2] = 'L';
  CarProfile[ID_SFL].CarName[3] = 0;
  CarProfile[ID_SFL].ID = ID_SFL;

  CarProfile[ID_SFL].Fuel = 10;
  //CarProfile[ID_SFL].RPM = 305;           // 7050 / RPMscale
  //CarProfile[ID_SFL].RPMscale = 23.125;   // 7400 / 320
  CarProfile[ID_SFL].WaterTemp = 110;
  CarProfile[ID_SFL].RPM = 7400;

  for (int i = 0; i<NUMOFGEARS; i++)
  {
    CarProfile[ID_SFL].SLI[i][0] = 6200;
    CarProfile[ID_SFL].SLI[i][1] = 6300;
    CarProfile[ID_SFL].SLI[i][2] = 6400;
    CarProfile[ID_SFL].SLI[i][3] = 6500;
    CarProfile[ID_SFL].SLI[i][4] = 6600;
    CarProfile[ID_SFL].SLI[i][5] = 6700;
    CarProfile[ID_SFL].SLI[i][6] = 6800;
    CarProfile[ID_SFL].SLI[i][7] = 6900;
  }

  /**************************/
  // BMW G82 M4
  /**************************/
  CarProfile[ID_G82_M4].CarName[0] = 'G';
  CarProfile[ID_G82_M4].CarName[1] = '8';
  CarProfile[ID_G82_M4].CarName[2] = '2';
  CarProfile[ID_G82_M4].CarName[3] = ' ';
  CarProfile[ID_G82_M4].CarName[4] = 'M';
  CarProfile[ID_G82_M4].CarName[5] = '4';
  CarProfile[ID_G82_M4].CarName[6] = 0;
  CarProfile[ID_G82_M4].ID = ID_G82_M4;

  CarProfile[ID_G82_M4].Fuel = 30;
  //CarProfile[ID_G82_M4].RPM = 311;           // 7300 / RPMscale
  //CarProfile[ID_G82_M4].RPMscale = 23.4375;  // 7500 / 320
  CarProfile[ID_G82_M4].WaterTemp = 85;
  CarProfile[ID_G82_M4].RPM = 7500;

  for (int i = 0; i<2; i++)
  // reverse, neutral
  {
    CarProfile[ID_G82_M4].SLI[i][0] = 5600; // 1st green
    CarProfile[ID_G82_M4].SLI[i][1] = 5600; // 1st green
    CarProfile[ID_G82_M4].SLI[i][2] = 5800; // 2nd green
    CarProfile[ID_G82_M4].SLI[i][3] = 5800; // 2nd green
    CarProfile[ID_G82_M4].SLI[i][4] = 6000; // 3rd yellow
    CarProfile[ID_G82_M4].SLI[i][5] = 6200; // 4th yellow
    CarProfile[ID_G82_M4].SLI[i][6] = 6400; // 5th red
    CarProfile[ID_G82_M4].SLI[i][7] = 6600; // all red and blinking
  }

  // 1st gear
  CarProfile[ID_G82_M4].SLI[2][0] = 5450;
  CarProfile[ID_G82_M4].SLI[2][1] = 5450;
  CarProfile[ID_G82_M4].SLI[2][2] = 5850;
  CarProfile[ID_G82_M4].SLI[2][3] = 5850;
  CarProfile[ID_G82_M4].SLI[2][4] = 6250;
  CarProfile[ID_G82_M4].SLI[2][5] = 6650;
  CarProfile[ID_G82_M4].SLI[2][6] = 7050;
  CarProfile[ID_G82_M4].SLI[2][7] = 7450;

  // 2nd gear
  CarProfile[ID_G82_M4].SLI[3][0] = 5800;
  CarProfile[ID_G82_M4].SLI[3][1] = 5800;
  CarProfile[ID_G82_M4].SLI[3][2] = 6125;
  CarProfile[ID_G82_M4].SLI[3][3] = 6125;
  CarProfile[ID_G82_M4].SLI[3][4] = 6450;
  CarProfile[ID_G82_M4].SLI[3][5] = 6775;
  CarProfile[ID_G82_M4].SLI[3][6] = 7100;
  CarProfile[ID_G82_M4].SLI[3][7] = 7450;

  // 3rd gear
  CarProfile[ID_G82_M4].SLI[4][0] = 6450;
  CarProfile[ID_G82_M4].SLI[4][1] = 6450;
  CarProfile[ID_G82_M4].SLI[4][2] = 6650;
  CarProfile[ID_G82_M4].SLI[4][3] = 6650;
  CarProfile[ID_G82_M4].SLI[4][4] = 6850;
  CarProfile[ID_G82_M4].SLI[4][5] = 7050;
  CarProfile[ID_G82_M4].SLI[4][6] = 7250;
  CarProfile[ID_G82_M4].SLI[4][7] = 7450;

  // 4th gear
  CarProfile[ID_G82_M4].SLI[5][0] = 6900;
  CarProfile[ID_G82_M4].SLI[5][1] = 6900;
  CarProfile[ID_G82_M4].SLI[5][2] = 7010;
  CarProfile[ID_G82_M4].SLI[5][3] = 7010;
  CarProfile[ID_G82_M4].SLI[5][4] = 7120;
  CarProfile[ID_G82_M4].SLI[5][5] = 7230;
  CarProfile[ID_G82_M4].SLI[5][6] = 7340;
  CarProfile[ID_G82_M4].SLI[5][7] = 7450;

  // 5th gear and above
  for (int i = 6; i<NUMOFGEARS; i++)
  {
    CarProfile[ID_G82_M4].SLI[i][0] = 7250;
    CarProfile[ID_G82_M4].SLI[i][1] = 7250;
    CarProfile[ID_G82_M4].SLI[i][2] = 7290;
    CarProfile[ID_G82_M4].SLI[i][3] = 7290;
    CarProfile[ID_G82_M4].SLI[i][4] = 7330;
    CarProfile[ID_G82_M4].SLI[i][5] = 7370;
    CarProfile[ID_G82_M4].SLI[i][6] = 7410;
    CarProfile[ID_G82_M4].SLI[i][7] = 7450;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Car profile selection menu                                             ////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// car button matrix
lv_obj_t *carselectionmatrix[NUMOFCARS];
lv_obj_t *carselectionmatrix_label[NUMOFCARS];

// car selection menu button
lv_obj_t *actualcarbutton;
lv_obj_t *actualcarbutton_label;

// LVGL event handler for actual car button
static void actualcar_handler(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);

  switch (code)
  {
    case LV_EVENT_CLICKED:
      DrawCarSelectionMenu();
      break;
  }
}

// LVGL event handler for car button matrix
static void carselectionmatrix_handler(lv_event_t * e)
{
  lv_obj_t *pressedbutton;
  byte *ID;
  
  lv_event_code_t code = lv_event_get_code(e);
  switch (code)
  {
    case LV_EVENT_CLICKED:
      pressedbutton = (lv_obj_t*)lv_event_get_current_target(e);
      ID = (byte*)lv_obj_get_user_data(pressedbutton);     // get the car ID from the pressed button
      
      ActiveCar = *ID;
      AdjustMaxRPM(ActiveCar);
      DrawGaugesScreen(ActiveCar);
      break;
  }
}

void SetupCarSelectionMenu()
{
  for (int i=0; i<NUMOFCARS; i++)
  {
    carselectionmatrix[i] = lv_button_create(screen_carselection);
    lv_obj_set_user_data(carselectionmatrix[i], &CarProfile[i].ID);     // link the car ID to the button
    carselectionmatrix_label[i] = lv_label_create(carselectionmatrix[i]);
    
    lv_obj_center(carselectionmatrix_label[i]);
    lv_label_set_text(carselectionmatrix_label[i], CarProfile[i].CarName);

    // Setup the buttons
    lv_obj_add_event_cb(carselectionmatrix[i], carselectionmatrix_handler, LV_EVENT_ALL, NULL);
    lv_obj_remove_flag(carselectionmatrix[i], LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_size(carselectionmatrix[i], 100, 50);
    lv_obj_set_pos(carselectionmatrix[i], ((i%7)*110)+20, ((i/7)*60)+20);
  }
}

void DrawCarSelectionMenu()
{
  lv_screen_load(screen_carselection);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Gauge variables                                                        ////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// LVGL color variables
lv_color_t dc, mc, wc, bc, ddc;

// gauge screen background
lv_obj_t *backgroundline1, *backgroundline2;
lv_style_t style_backgroundline;

// RPM bar
lv_obj_t *RPMbar;

// SLI
lv_obj_t *SLI1, *SLI2, *SLI3, *SLI4, *SLI5, *SLI6, *SLI7, *SLI8;

// Engine management lights
lv_image_dsc_t img_fuelpressure, img_oilpressure, img_stall, img_watertemp;
lv_obj_t *icon_fuelpressure, *icon_oilpressure, *icon_stall, *icon_watertemp;

// Gear number indicator
lv_obj_t *gear;

// fuel, water, speed numbers
lv_obj_t *fuel_static_text1, *speed_static_text1, *water_static_text1;
lv_obj_t *fuel_static_text2, *speed_static_text2, *water_static_text2;
lv_obj_t *fuel_value_text, *speed_value_text, *water_value_text;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Gauge positions                                                        ////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define FUELX 0
#define FUELY 270
#define SPEEDX 0
#define SPEEDY 180
#define WATERX 0
#define WATERY 320
#define DELTA1 260  // offset for the unit
#define DELTA2 170  // offset for the actual value

#define GEARX 630
#define GEARY 180

#define RPMY 80
#define SLIY 10

#define EML_FUELPRESSX 0
#define EML_FUELPRESSY 400
#define EML_OILPRESSX 80
#define EML_OILPRESSY 400
#define EML_WATERTEMPX 160
#define EML_WATERTEMPY 400
#define EML_STALLX 240
#define EML_STALLY 400

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Gauge functions                                                        ////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**********************************************/
// Draw the background for the instruments
/**********************************************/
void SetupGaugesScreen()
{
  // Create an array for the points of the background lines
  static lv_point_precise_t line_points1[] = { {0, 150}, {799, 150} };
  static lv_point_precise_t line_points2[] = { {0, 390}, {799, 400} };

  backgroundline1 = lv_line_create(screen_gauges);
  backgroundline2 = lv_line_create(screen_gauges);

  // Create background line style
  lv_style_init(&style_backgroundline);
  lv_style_set_line_width(&style_backgroundline, 6);
  lv_style_set_line_color(&style_backgroundline, ddc);
  lv_style_set_line_rounded(&style_backgroundline, false);

  // Draw background lines and apply the new style
  lv_line_set_points(backgroundline1, line_points1, 2);
  lv_line_set_points(backgroundline2, line_points2, 2);
  lv_obj_add_style(backgroundline1, &style_backgroundline, 0);
  lv_obj_add_style(backgroundline2, &style_backgroundline, 0);

  // Draw button for car selection menu
  actualcarbutton = lv_button_create(screen_gauges);
  actualcarbutton_label = lv_label_create(actualcarbutton);
  lv_obj_center(actualcarbutton_label);

  // Setup the car selection menu button
  lv_obj_add_event_cb(actualcarbutton, actualcar_handler, LV_EVENT_ALL, NULL);
  lv_obj_remove_flag(actualcarbutton, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_set_size(actualcarbutton, 100, 50);
  lv_obj_set_pos(actualcarbutton, 700, 420);
}

// customize background for the actual car profile
void DrawGaugesScreen(char ID)
{
  // display the name of actual profile on car selection menu button
  lv_label_set_text(actualcarbutton_label, CarProfile[ID].CarName);

  /*switch (ID)
  {
    case ID_Skippy:
    case ID_CTS_V:
    case ID_MX5_NC:
    case ID_MX5_ND:
    case ID_FR20:
    case ID_DF3:
    case ID_992_CUP:
    case ID_GR86:
    case ID_SFL:
    case ID_G82_M4:
      break;
  }*/
  
  lv_screen_load(screen_gauges);
}

/**********************************************/
// Draw the engine warning icons
/**********************************************/
void SetupEngineWarnings()
{
  img_fuelpressure.header.cf = LV_COLOR_FORMAT_RGB565A8;
  img_fuelpressure.header.magic = LV_IMAGE_HEADER_MAGIC;
  img_fuelpressure.header.w = 80;
  img_fuelpressure.header.h = 80;
  img_fuelpressure.data_size = 6400 * 3;
  img_fuelpressure.data = fuelpressure_map;
  img_fuelpressure.header.flags = 0;
  img_fuelpressure.header.reserved_2 = 0;
  img_fuelpressure.header.stride = 0;
  img_fuelpressure.reserved = NULL;

  img_oilpressure.header.cf = LV_COLOR_FORMAT_RGB565A8;
  img_oilpressure.header.magic = LV_IMAGE_HEADER_MAGIC;
  img_oilpressure.header.w = 80;
  img_oilpressure.header.h = 80;
  img_oilpressure.data_size = 6400 * 3;
  img_oilpressure.data = oilpressure_map;
  img_oilpressure.header.flags = 0;
  img_oilpressure.header.reserved_2 = 0;
  img_oilpressure.header.stride = 0;
  img_oilpressure.reserved = NULL;

  img_stall.header.cf = LV_COLOR_FORMAT_RGB565A8;
  img_stall.header.magic = LV_IMAGE_HEADER_MAGIC;
  img_stall.header.w = 80;
  img_stall.header.h = 80;
  img_stall.data_size = 6400 * 3;
  img_stall.data = stall_map;
  img_stall.header.flags = 0;
  img_stall.header.reserved_2 = 0;
  img_stall.header.stride = 0;
  img_stall.reserved = NULL;

  img_watertemp.header.cf = LV_COLOR_FORMAT_RGB565A8;
  img_watertemp.header.magic = LV_IMAGE_HEADER_MAGIC;
  img_watertemp.header.w = 80;
  img_watertemp.header.h = 80;
  img_watertemp.data_size = 6400 * 3;
  img_watertemp.data = watertemp_map;
  img_watertemp.header.flags = 0;
  img_watertemp.header.reserved_2 = 0;
  img_watertemp.header.stride = 0;
  img_watertemp.reserved = NULL;

  icon_fuelpressure = lv_image_create(screen_gauges);
  lv_image_set_src(icon_fuelpressure, &img_fuelpressure);
  lv_obj_set_pos(icon_fuelpressure, EML_FUELPRESSX, EML_FUELPRESSY);

  icon_oilpressure = lv_image_create(screen_gauges);
  lv_image_set_src(icon_oilpressure, &img_oilpressure);
  lv_obj_set_pos(icon_oilpressure, EML_OILPRESSX, EML_OILPRESSY);

  icon_watertemp = lv_image_create(screen_gauges);
  lv_image_set_src(icon_watertemp, &img_watertemp);
  lv_obj_set_pos(icon_watertemp, EML_WATERTEMPX, EML_WATERTEMPY);

  icon_stall = lv_image_create(screen_gauges);
  lv_image_set_src(icon_stall, &img_stall);
  lv_obj_set_pos(icon_stall, EML_STALLX, EML_STALLY);

  lv_obj_set_style_image_recolor_opa(icon_watertemp, WARNINGLIGHT_OFF, 0);
  lv_obj_set_style_image_recolor_opa(icon_stall, WARNINGLIGHT_OFF, 0);
}

void DrawEngineWarnings(char Warning, char WarningPrev)
{
  char Filtered, FilteredPrev;

  // set water temp light
  Filtered = Warning & 0x01;
  FilteredPrev = WarningPrev & 0x01;
  if (Filtered != FilteredPrev)
  {
    if (Filtered != 0) lv_obj_set_style_image_recolor_opa(icon_watertemp, WARNINGLIGHT_ON, 0);
    else lv_obj_set_style_image_recolor_opa(icon_watertemp, WARNINGLIGHT_OFF, 0);
  }

  // set fuel pressure light
  Filtered = Warning & 0x02;
  FilteredPrev = WarningPrev & 0x02;
  if (Filtered != FilteredPrev)
  {
    if (Filtered != 0) lv_obj_set_style_image_recolor_opa(icon_fuelpressure, WARNINGLIGHT_ON, 0);
    else lv_obj_set_style_image_recolor_opa(icon_fuelpressure, WARNINGLIGHT_OFF, 0);
  }

  // set oil pressure light
  Filtered = Warning & 0x04;
  FilteredPrev = WarningPrev & 0x04;
  if (Filtered != FilteredPrev)
  {
    if (Filtered != 0) lv_obj_set_style_image_recolor_opa(icon_oilpressure, WARNINGLIGHT_ON, 0);
    else lv_obj_set_style_image_recolor_opa(icon_oilpressure, WARNINGLIGHT_OFF, 0);
  }

  // set stall sign light
  Filtered = Warning & 0x08;
  FilteredPrev = WarningPrev & 0x08;
  if (Filtered != FilteredPrev)
  {
    if (Filtered != 0) lv_obj_set_style_image_recolor_opa(icon_stall, WARNINGLIGHT_ON, 0);
    else lv_obj_set_style_image_recolor_opa(icon_stall, WARNINGLIGHT_OFF, 0);
  }

  // draw limiter sign
  Filtered = Warning & 0x10;
  FilteredPrev = WarningPrev & 0x10;
}

/**********************************************/
// Draw fuel gauge
/**********************************************/
void SetupFuel()
{
  fuel_static_text1 = lv_label_create(screen_gauges);
  fuel_static_text2 = lv_label_create(screen_gauges);
  fuel_value_text = lv_label_create(screen_gauges);
  lv_label_set_text(fuel_static_text1, "Fuel:");
  lv_label_set_text(fuel_static_text2, "L");
  lv_label_set_text(fuel_value_text, "0.0");
  lv_obj_set_pos(fuel_static_text1, FUELX, FUELY);
  lv_obj_set_pos(fuel_static_text2, FUELX + DELTA1, FUELY);
  lv_obj_set_pos(fuel_value_text, FUELX + DELTA2, FUELY);
  
  lv_obj_set_style_text_font(fuel_static_text1, &font_messages, 0);
  lv_obj_set_style_text_font(fuel_static_text2, &font_messages, 0);
  lv_obj_set_style_text_font(fuel_value_text, &font_messages, 0);
}

void DrawFuel(int Fuel, int FuelPrev)
{
  // received value is multiplied by 10 to keep first fraction
  if(Fuel != FuelPrev) lv_label_set_text_fmt(fuel_value_text, "%d.%d", Fuel / 10, Fuel % 10);
}

/**********************************************/
// Draw speed gauge
/**********************************************/
void SetupSpeed()
{
  speed_static_text1 = lv_label_create(screen_gauges);
  speed_static_text2 = lv_label_create(screen_gauges);
  speed_value_text = lv_label_create(screen_gauges);
  lv_label_set_text(speed_static_text1, "Speed:");
  lv_label_set_text(speed_static_text2, "Km/h");
  lv_label_set_text(speed_value_text, "0");
  lv_obj_set_pos(speed_static_text1, SPEEDX, SPEEDY);
  lv_obj_set_pos(speed_static_text2, SPEEDX + DELTA1, SPEEDY);
  lv_obj_set_pos(speed_value_text, SPEEDX + DELTA2, SPEEDY);
  
  lv_obj_set_style_text_font(speed_static_text1, &font_messages, 0);
  lv_obj_set_style_text_font(speed_static_text2, &font_messages, 0);
  lv_obj_set_style_text_font(speed_value_text, &font_messages, 0);
}

void DrawSpeed(int Speed, int SpeedPrev)
{
  if (Speed != SpeedPrev) lv_label_set_text_fmt(speed_value_text, "%d", Speed);
}

/**********************************************/
// Draw water temperature gauge
/**********************************************/
void SetupWater()
{
  water_static_text1 = lv_label_create(screen_gauges);
  water_static_text2 = lv_label_create(screen_gauges);
  water_value_text = lv_label_create(screen_gauges);
  lv_label_set_text(water_static_text1, "Water:");
  lv_label_set_text(water_static_text2, "C");
  lv_label_set_text(water_value_text, "0");
  lv_obj_set_pos(water_static_text1, WATERX, WATERY);
  lv_obj_set_pos(water_static_text2, WATERX + DELTA1, WATERY);
  lv_obj_set_pos(water_value_text, WATERX + DELTA2, WATERY);
  
  lv_obj_set_style_text_font(water_static_text1, &font_messages, 0);
  lv_obj_set_style_text_font(water_static_text2, &font_messages, 0);
  lv_obj_set_style_text_font(water_value_text, &font_messages, 0);
}

void DrawWater(int Water, int WaterPrev)
{
  if (Water != WaterPrev) lv_label_set_text_fmt(water_value_text, "%d", Water);
}

/**********************************************/
// Draw actual gear number
/**********************************************/
void SetupGear()
{
  gear = lv_label_create(screen_gauges);
  lv_label_set_text(gear, "N");
  lv_obj_set_pos(gear, GEARX, GEARY);
  
  lv_obj_set_style_text_font(gear, &font_gear, 0);
}

void DrawGear(signed char GearNum, signed char GearNumPrev)
{
  if (GearNum != GearNumPrev)
  {
    switch (GearNum)
    {
      case -1:
        lv_label_set_text(gear, "R");
        break;
      case 0:
        lv_label_set_text(gear, "N");
        break;
      default:
        lv_label_set_text_fmt(gear, "%d", GearNum);
        break;
    }
  }
}

/**********************************************/
// Draw RPM gauge
/**********************************************/
void SetupRPM()
{
  RPMbar = lv_bar_create(screen_gauges);

  lv_obj_set_size(RPMbar, 800, 50);
  lv_obj_set_pos(RPMbar, 0, RPMY);
  lv_bar_set_range(RPMbar, 0, CarProfile[DEFAULTCAR].RPM);
  lv_bar_set_value(RPMbar, 3000, LV_ANIM_OFF);
}

void AdjustMaxRPM(char ID)
{
  lv_bar_set_range(RPMbar, 0, CarProfile[ID].RPM);
}

void DrawRPM(int RPM, int RPMPrev)
{
  lv_bar_set_value(RPMbar, RPM, LV_ANIM_OFF);
}

/**********************************************/
// Draw shift light indicator
/**********************************************/
void SetupSLI()
{
  // draw rectangles
  SLI1 = lv_obj_create(screen_gauges);
  SLI2 = lv_obj_create(screen_gauges);
  SLI3 = lv_obj_create(screen_gauges);
  SLI4 = lv_obj_create(screen_gauges);
  SLI5 = lv_obj_create(screen_gauges);
  SLI6 = lv_obj_create(screen_gauges);
  SLI7 = lv_obj_create(screen_gauges);
  SLI8 = lv_obj_create(screen_gauges);

  lv_obj_set_style_bg_color(SLI1, bc, 0);
  lv_obj_set_size(SLI1 , 100, 50);
  lv_obj_set_pos(SLI1 , 0, SLIY);
  
  lv_obj_set_style_bg_color(SLI2, bc, 0);
  lv_obj_set_size(SLI2 , 100, 50);
  lv_obj_set_pos(SLI2 , 100, SLIY);

  lv_obj_set_style_bg_color(SLI3, bc, 0);
  lv_obj_set_size(SLI3 , 100, 50);
  lv_obj_set_pos(SLI3 , 200, SLIY);

  lv_obj_set_style_bg_color(SLI4, bc, 0);
  lv_obj_set_size(SLI4 , 100, 50);
  lv_obj_set_pos(SLI4 , 300, SLIY);

  lv_obj_set_style_bg_color(SLI5, bc, 0);
  lv_obj_set_size(SLI5 , 100, 50);
  lv_obj_set_pos(SLI5 , 400, SLIY);

  lv_obj_set_style_bg_color(SLI6, bc, 0);
  lv_obj_set_size(SLI6 , 100, 50);
  lv_obj_set_pos(SLI6 , 500, SLIY);

  lv_obj_set_style_bg_color(SLI7, bc, 0);
  lv_obj_set_size(SLI7 , 100, 50);
  lv_obj_set_pos(SLI7 , 600, SLIY);

  lv_obj_set_style_bg_color(SLI8, bc, 0);
  lv_obj_set_size(SLI8 , 100, 50);
  lv_obj_set_pos(SLI8 , 700, SLIY);
}

void DrawSLI(int SLI, int SLIPrev)
{
  if (SLI < SLIPrev)
  {
  // clear only the disappeared indicators
    for (int i=SLI; i<=SLIPrev-1; i++)
    {
      switch (i)
      {
        case 0: lv_obj_set_style_bg_color(SLI1, bc, 0);
                break;
        case 1: lv_obj_set_style_bg_color(SLI2, bc, 0);
                break;
        case 2: lv_obj_set_style_bg_color(SLI3, bc, 0);
                break;
        case 3: lv_obj_set_style_bg_color(SLI4, bc, 0);
                break;
        case 4: lv_obj_set_style_bg_color(SLI5, bc, 0);
                break;
        case 5: lv_obj_set_style_bg_color(SLI6, bc, 0);
                break;
        case 6: lv_obj_set_style_bg_color(SLI7, bc, 0);
                break;
        case 7: lv_obj_set_style_bg_color(SLI8, bc, 0);
                break;                
      }
    }
  }
  else
  {
 // draw only the appeared indicators
    for (int i=SLIPrev+1; i<= SLI; i++)
    {
      switch (i)
      {
        case 1: lv_obj_set_style_bg_color(SLI1, dc, 0);
                break;
        case 2: lv_obj_set_style_bg_color(SLI2, dc, 0);
                break;
        case 3: lv_obj_set_style_bg_color(SLI3, dc, 0);
                break;
        case 4: lv_obj_set_style_bg_color(SLI4, dc, 0);
                break;
        case 5: lv_obj_set_style_bg_color(SLI5, mc, 0);
                break;
        case 6: lv_obj_set_style_bg_color(SLI6, mc, 0);
                break;
        case 7: lv_obj_set_style_bg_color(SLI7, wc, 0);
                break;
        case 8: lv_obj_set_style_bg_color(SLI8, wc, 0);
                break;
     }
   }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// setup()                                                                ////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{
    Serial.begin( 115200 );
    Serial.println("iRDash client v3 start");

/******************************************************************/
// ESP_Panel_Library: Initialize display device
/******************************************************************/
    #if EXAMPLE_LCD_PIN_NUM_BK_LIGHT >= 0
      Serial.println("ESP32_Display_Panel: Initialize backlight control pin and turn it off");
      ESP_PanelBacklight *backlight = new ESP_PanelBacklight(LCD_PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_ON_LEVEL, true);
      backlight->begin();
      backlight->off();
    #endif

    ESP_PanelBus_RGB *panel_bus = new ESP_PanelBus_RGB(LCD_WIDTH, LCD_HEIGHT,
                                                       LCD_PIN_NUM_RGB_DATA0, LCD_PIN_NUM_RGB_DATA1,
                                                       LCD_PIN_NUM_RGB_DATA2, LCD_PIN_NUM_RGB_DATA3,
                                                       LCD_PIN_NUM_RGB_DATA4, LCD_PIN_NUM_RGB_DATA5,
                                                       LCD_PIN_NUM_RGB_DATA6, LCD_PIN_NUM_RGB_DATA7,
                                                       LCD_PIN_NUM_RGB_DATA8, LCD_PIN_NUM_RGB_DATA9,
                                                       LCD_PIN_NUM_RGB_DATA10, LCD_PIN_NUM_RGB_DATA11,
                                                       LCD_PIN_NUM_RGB_DATA12, LCD_PIN_NUM_RGB_DATA13,
                                                       LCD_PIN_NUM_RGB_DATA14, LCD_PIN_NUM_RGB_DATA15,
                                                       LCD_PIN_NUM_RGB_HSYNC, LCD_PIN_NUM_RGB_VSYNC,
                                                       LCD_PIN_NUM_RGB_PCLK, LCD_PIN_NUM_RGB_DE,
                                                       LCD_PIN_NUM_RGB_DISP);

    panel_bus->configRgbTimingFreqHz(LCD_RGB_TIMING_FREQ_HZ);
    panel_bus->configRgbTimingPorch(LCD_RGB_TIMING_HPW, LCD_RGB_TIMING_HBP, LCD_RGB_TIMING_HFP,
                                    LCD_RGB_TIMING_VPW, LCD_RGB_TIMING_VBP, LCD_RGB_TIMING_VFP);
    
    panel_bus->configRgbBounceBufferSize(LCD_WIDTH * 10); // Set bounce buffer to avoid screen drift
    panel_bus->begin();

    Serial.println("ESP32_Display_Panel: Create LCD device");

    lcd = new LCD_CLASS(LCD_NAME, panel_bus, LCD_COLOR_BITS, LCD_PIN_NUM_RST);
    lcd->init();
    lcd->reset();
    lcd->begin();
    
    #if LCD_PIN_NUM_RGB_DISP >= 0
        lcd->displayOn();
    #endif
    #if ENABLE_PRINT_LCD_FPS
        lcd->attachRefreshFinishCallback(onVsyncEndCallback, (void *)&start_time);
    #endif

    #if LCD_PIN_NUM_BK_LIGHT >= 0
        Serial.println("ESP32_Display_Panel: Turn on the backlight");
        backlight->on();
    #endif

/******************************************************************/
// ESP_Panel_Library: Initialize touch device
/******************************************************************/
    Serial.println("ESP_PanelTouch: Create I2C bus");
    ESP_PanelBus_I2C *touch_bus = new ESP_PanelBus_I2C(TOUCH_PIN_NUM_I2C_SCL, TOUCH_PIN_NUM_I2C_SDA,
                                                       ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG());
    touch_bus->configI2cFreqHz(TOUCH_I2C_FREQ_HZ);
    touch_bus->begin();

    touch = new ESP_PanelTouch_GT911(touch_bus, TOUCH_WIDTH, TOUCH_HEIGHT,
                                     TOUCH_PIN_NUM_RST, TOUCH_PIN_NUM_INT);
    touch->init();
    touch->begin();
    Serial.println("ESP_PanelTouch: Touch device created");

/******************************************************************/
// LVGL: Initialize library
/******************************************************************/
    String LVGL_Arduino = "LVGL: Hello Arduino! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    Serial.println(LVGL_Arduino);
    lv_init();

    // Set a tick source so that LVGL will know how much time elapsed.
    lv_tick_set_cb(my_tick);

    // register print function for debugging
    #if LV_USE_LOG != 0
      lv_log_register_print_cb( my_print );
    #endif

    // Create a display
    disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    Serial.println("LVGL: display buffer initialized");

    // Initialize the input device driver
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); // Touchpad should have POINTER type
    lv_indev_set_read_cb(indev, my_touchpad_read);
    Serial.println("LVGL: input device initialized");

    Serial.println("HW setup done");

/******************************************************************/
// iRDash: Setup car profiles, colors, gauges and screen layouts
/******************************************************************/
    // initialize internal variables
    InData = new SIncomingData;  // allocate the data structure of the telemetry data
    pInData = (char*)InData;     // set the byte array pointer to the telemetry data
    ResetInternalData();

    // Setup LVGL draw colors
    // (green)         red: 80  ; green: 255 ; blue: 80
    // (yellow)        red: 255 ; green: 240 ; blue: 0
    // (red)           red: 255 ; green: 50  ; blue: 50
    // (darker green)  red: 70  ; green: 180 ; blue: 70
    // (background)    red: 0   ; green: 0   ; blue: 0
    dc =  lv_color_make(80, 255, 80);   // draw color
    mc =  lv_color_make(255, 240, 0);   // middle color
    wc =  lv_color_make(255, 50,  50);  // warning color
    ddc = lv_color_make(70,  180, 70);  // darker draw color
    bc =  lv_color_make(0,   0,   0);   // background color

    // Initialize fonts
    LV_FONT_DECLARE(font_gear);
    LV_FONT_DECLARE(font_messages);

    screen_gauges = lv_obj_create(NULL);
    screen_carselection = lv_obj_create(NULL);

    UploadCarProfiles();

    SetupGaugesScreen();
    SetupCarSelectionMenu();

    SetupSLI();
    SetupRPM();
    SetupGear();
    SetupEngineWarnings();
    SetupWater();
    SetupFuel();
    SetupSpeed();

    DrawGaugesScreen(DEFAULTCAR);
    ActiveCar = DEFAULTCAR;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// loop()                                                                 ////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop()
{
  int rpm_int;
  char gear;

  // read serial port
  if (Serial.available() > 0)
  {
    inByte = Serial.read();

    // check the incoming byte, store the byte in the correct telemetry data block position
    switch (blockpos)
    {
      // first identify if the three byte long header is received in the correct order
      case 0: if (inByte == 3) blockpos = 1;
              break;
      case 1: if (inByte == 12) blockpos = 2;
              else blockpos = 0;
              break;
      case 2: if (inByte == 48) blockpos = 3;
              else blockpos = 0;
              break;
      default:
              if (blockpos >=3) // the three byte long identification header was found, now we store the next incoming bytes in our telemetry data block.
              {
                *(pInData +blockpos-3) = inByte;  // we don't store the identification header
                blockpos++;
              }
              
              if (blockpos == sizeof(SIncomingData)+4)  // last byte of the incoming telemetry data was received, now the screen can be drawn
              {
                blockpos = 0; // reset block position

                // calculate and draw Engine Warning lights
                Screen[1].EngineWarnings = InData->EngineWarnings;
                if (Screen[0].EngineWarnings != Screen[1].EngineWarnings) DrawEngineWarnings(Screen[1].EngineWarnings, Screen[0].EngineWarnings);

                // calculate and draw RPM gauge
                Screen[1].RPMgauge = (int)InData->RPM;
                if (Screen[0].RPMgauge != Screen[1].RPMgauge) DrawRPM(Screen[1].RPMgauge, Screen[0].RPMgauge);

                // calculate and draw gear number
                Screen[1].Gear = InData->Gear;
                if (Screen[0].Gear != Screen[1].Gear) DrawGear(Screen[1].Gear, Screen[0].Gear);

                // calculate and draw fuel level gauge
                Screen[1].Fuel = (int)(InData->Fuel*10); // convert float data to int and keep the first digit of the fractional part also
                if (Screen[0].Fuel != Screen[1].Fuel) DrawFuel(Screen[1].Fuel, Screen[0].Fuel);

                // calculate and draw speed gauge
                Screen[1].Speed = (int)(InData->Speed*3.6);  // convert m/s to km/h
                if (Screen[0].Speed != Screen[1].Speed) DrawSpeed(Screen[1].Speed, Screen[0].Speed);

                // calculate and draw water temperature gauge
                Screen[1].WaterTemp = (int)InData->WaterTemp;
                if (Screen[0].WaterTemp != Screen[1].WaterTemp) DrawWater(Screen[1].WaterTemp, Screen[0].WaterTemp);
                
                // calculate and draw shift light indicator
                rpm_int = (int)InData->RPM;
                gear = InData->Gear+1;  // "-1" corresponds to reverse but index should start with "0"
                if (rpm_int <= CarProfile[ActiveCar].SLI[gear][0]) Screen[1].SLI = 0; // determine how many light to be activated for the current gear
                else
                {
                  if (rpm_int > CarProfile[ActiveCar].SLI[gear][7]) Screen[1].SLI = 8;
                  else if (rpm_int > CarProfile[ActiveCar].SLI[gear][6]) Screen[1].SLI = 7;
                       else if (rpm_int > CarProfile[ActiveCar].SLI[gear][5]) Screen[1].SLI = 6;
                            else if (rpm_int > CarProfile[ActiveCar].SLI[gear][4]) Screen[1].SLI = 5;
                                 else if (rpm_int > CarProfile[ActiveCar].SLI[gear][3]) Screen[1].SLI = 4;
                                      else if (rpm_int > CarProfile[ActiveCar].SLI[gear][2]) Screen[1].SLI = 3;
                                           else if (rpm_int > CarProfile[ActiveCar].SLI[gear][1]) Screen[1].SLI = 2;
                                                else if (rpm_int > CarProfile[ActiveCar].SLI[gear][0]) Screen[1].SLI = 1;
                }
                if (Screen[0].SLI != Screen[1].SLI) DrawSLI(Screen[1].SLI, Screen[0].SLI);

                // update old screen data
                Screen[0].EngineWarnings = Screen[1].EngineWarnings;
                Screen[0].Fuel           = Screen[1].Fuel;
                Screen[0].Gear           = Screen[1].Gear;
                Screen[0].RPMgauge       = Screen[1].RPMgauge;
                Screen[0].Speed          = Screen[1].Speed;
                Screen[0].WaterTemp      = Screen[1].WaterTemp;
                Screen[0].SLI            = Screen[1].SLI;
  
                // clear the incoming serial buffer which was filled up during the drawing, if it is not done then data misalignment can happen when we read the next telemetry block
                inByte = Serial.available();
                for (int i=0; i<inByte; i++) Serial.read();
              }
              break;
    }
  }

  lv_timer_periodic_handler(); // use the sleep time automatically calculated by LVGL
}
