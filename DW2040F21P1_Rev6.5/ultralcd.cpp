/*
Modified by Dennis Patella WASPROJECT TEAM on 2015
http://www.wasproject.it
 */
#include "temperature.h"
#include "ultralcd.h"
#ifdef ULTRA_LCD
#include "Marlin.h"
#include "language.h"
#include "cardreader.h"
#include "temperature.h"
#include "stepper.h"
#include "ConfigurationStore.h"

int8_t encoderDiff; /* encoderDiff is updated from interrupt context and added to encoderPosition every LCD update */

/* Configuration settings */
int plaPreheatHotendTemp;
int plaPreheatHPBTemp;
int plaPreheatFanSpeed;

int absPreheatHotendTemp;
int absPreheatHPBTemp;
int absPreheatFanSpeed;
int step_adj=0;
bool conf=false;



#ifdef ULTIPANEL
static float manual_feedrate[] = MANUAL_FEEDRATE;
#endif // ULTIPANEL

/* !Configuration settings */

//Function pointer to menu functions.
typedef void (*menuFunc_t)();

uint8_t lcd_status_message_level;
char lcd_status_message[LCD_WIDTH+1] = WELCOME_MSG;

#ifdef DOGLCD
#include "dogm_lcd_implementation.h"
#else
#include "ultralcd_implementation_hitachi_HD44780.h"
#endif

/** forward declarations **/

void copy_and_scalePID_i();
void copy_and_scalePID_d();

/* Different menus */
static void lcd_status_screen();
#ifdef ULTIPANEL
extern bool powersupply;
static void lcd_main_menu();
static void lcd_tune_menu();
static void lcd_prepare_menu();
static void lcd_move_menu();
static bool tmpAutoResurr;
#ifdef UNLOCK_CODE
static void lcd_unlock_menu();
static void lcd_lock();
#endif
bool yesNo=false;
static int velocity=5;
static void lcd_manual_calib();

static void lcd_modify_z_max();
static void lcd_change_mode();
static void lcd_change_resurr();
static void lcd_confirm_stop();
static void lcd_confirm_reset();
static void lcd_force_shutdown();
static void lcd_info_sw();
static void lcd_delta_adj();
static void lcd_fan_type();
static int level_steps = 0;
//static int prev_level_steps = 0; 
static void lcd_control_menu();
static void lcd_control_temperature_menu();
static void lcd_control_temperature_preheat_pla_settings_menu();
static void lcd_control_temperature_preheat_abs_settings_menu();
static void lcd_control_motion_menu();
#ifdef DOGLCD
static void lcd_set_contrast();
#endif
static void lcd_control_retract_menu();
static void lcd_sdcard_menu();

static void lcd_quick_feedback();//Cause an LCD refresh, and give the user visual or audible feedback that something has happened

/* Different types of actions that can be used in menu items. */
static void menu_action_back(menuFunc_t data);
static void menu_action_submenu(menuFunc_t data);
static void menu_action_gcode(const char* pgcode);
static void menu_action_function(menuFunc_t data);
static void menu_action_sdfile(const char* filename, char* longFilename);
static void menu_action_sddirectory(const char* filename, char* longFilename);
static void menu_action_setting_edit_bool(const char* pstr, bool* ptr);
static void menu_action_setting_edit_int3(const char* pstr, int* ptr, int minValue, int maxValue);
static void menu_action_setting_edit_float3(const char* pstr, float* ptr, float minValue, float maxValue);
static void menu_action_setting_edit_float32(const char* pstr, float* ptr, float minValue, float maxValue);
static void menu_action_setting_edit_float5(const char* pstr, float* ptr, float minValue, float maxValue);
static void menu_action_setting_edit_float51(const char* pstr, float* ptr, float minValue, float maxValue);
static void menu_action_setting_edit_float52(const char* pstr, float* ptr, float minValue, float maxValue);
static void menu_action_setting_edit_long5(const char* pstr, unsigned long* ptr, unsigned long minValue, unsigned long maxValue);
static void menu_action_setting_edit_callback_bool(const char* pstr, bool* ptr, menuFunc_t callbackFunc);
static void menu_action_setting_edit_callback_int3(const char* pstr, int* ptr, int minValue, int maxValue, menuFunc_t callbackFunc);
static void menu_action_setting_edit_callback_float3(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
static void menu_action_setting_edit_callback_float32(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
static void menu_action_setting_edit_callback_float5(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
static void menu_action_setting_edit_callback_float51(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
static void menu_action_setting_edit_callback_float52(const char* pstr, float* ptr, float minValue, float maxValue, menuFunc_t callbackFunc);
static void menu_action_setting_edit_callback_long5(const char* pstr, unsigned long* ptr, unsigned long minValue, unsigned long maxValue, menuFunc_t callbackFunc);

#define ENCODER_FEEDRATE_DEADZONE 10

#if !defined(LCD_I2C_VIKI)
  #ifndef ENCODER_STEPS_PER_MENU_ITEM
    #define ENCODER_STEPS_PER_MENU_ITEM 5
  #endif
  #ifndef ENCODER_PULSES_PER_STEP
    #define ENCODER_PULSES_PER_STEP 1
  #endif
#else
  #ifndef ENCODER_STEPS_PER_MENU_ITEM
    #define ENCODER_STEPS_PER_MENU_ITEM 2 // VIKI LCD rotary encoder uses a different number of steps per rotation
  #endif
  #ifndef ENCODER_PULSES_PER_STEP
    #define ENCODER_PULSES_PER_STEP 1
  #endif
#endif


/* Helper macros for menus */
#define START_MENU() do { \
    if (encoderPosition > 0x8000) encoderPosition = 0; \
    if (encoderPosition / ENCODER_STEPS_PER_MENU_ITEM < currentMenuViewOffset) currentMenuViewOffset = encoderPosition / ENCODER_STEPS_PER_MENU_ITEM;\
    uint8_t _lineNr = currentMenuViewOffset, _menuItemNr; \
    bool wasClicked = LCD_CLICKED;\
    for(uint8_t _drawLineNr = 0; _drawLineNr < LCD_HEIGHT; _drawLineNr++, _lineNr++) { \
        _menuItemNr = 0;
#define MENU_ITEM(type, label, args...) do { \
    if (_menuItemNr == _lineNr) { \
        if (lcdDrawUpdate) { \
            const char* _label_pstr = PSTR(label); \
            if ((encoderPosition / ENCODER_STEPS_PER_MENU_ITEM) == _menuItemNr) { \
                lcd_implementation_drawmenu_ ## type ## _selected (_drawLineNr, _label_pstr , ## args ); \
            }else{\
                lcd_implementation_drawmenu_ ## type (_drawLineNr, _label_pstr , ## args ); \
            }\
        }\
        if (wasClicked && (encoderPosition / ENCODER_STEPS_PER_MENU_ITEM) == _menuItemNr) {\
            lcd_quick_feedback(); \
            menu_action_ ## type ( args ); \
            return;\
        }\
    }\
    _menuItemNr++;\
} while(0)
#define MENU_ITEM_DUMMY() do { _menuItemNr++; } while(0)
#define MENU_ITEM_EDIT(type, label, args...) MENU_ITEM(setting_edit_ ## type, label, PSTR(label) , ## args )
#define MENU_ITEM_EDIT_CALLBACK(type, label, args...) MENU_ITEM(setting_edit_callback_ ## type, label, PSTR(label) , ## args )
#define END_MENU() \
    if (encoderPosition / ENCODER_STEPS_PER_MENU_ITEM >= _menuItemNr) encoderPosition = _menuItemNr * ENCODER_STEPS_PER_MENU_ITEM - 1; \
    if ((uint8_t)(encoderPosition / ENCODER_STEPS_PER_MENU_ITEM) >= currentMenuViewOffset + LCD_HEIGHT) { currentMenuViewOffset = (encoderPosition / ENCODER_STEPS_PER_MENU_ITEM) - LCD_HEIGHT + 1; lcdDrawUpdate = 1; _lineNr = currentMenuViewOffset - 1; _drawLineNr = -1; } \
    } } while(0)

/** Used variables to keep track of the menu */
#ifndef REPRAPWORLD_KEYPAD
volatile uint8_t buttons;//Contains the bits of the currently pressed buttons.
#else
volatile uint8_t buttons_reprapworld_keypad; // to store the reprapworld_keypad shift register values
#endif
#ifdef LCD_HAS_SLOW_BUTTONS
volatile uint8_t slow_buttons;//Contains the bits of the currently pressed buttons.
#endif
uint8_t currentMenuViewOffset;              /* scroll offset in the current menu */
uint32_t blocking_enc;
uint8_t lastEncoderBits;
uint32_t encoderPosition;
#if (SDCARDDETECT > 0)
bool lcd_oldcardstatus;
#endif
#endif//ULTIPANEL
static bool homeCalib=false;
menuFunc_t currentMenu = lcd_status_screen; /* function pointer to the currently active menu */
uint32_t lcd_next_update_millis;
uint8_t lcd_status_update_delay;
uint8_t lcdDrawUpdate = 2;                  /* Set to none-zero when the LCD needs to draw, decreased after every draw. Set to 2 in LCD routines so the LCD gets at least 1 full redraw (first redraw is partial) */
uint8_t dgtunlock =0;
uint8_t unlockStep =0;
int dgt1unlock =0;
int dgt2unlock =0;
int dgt3unlock =0;
int dgt4unlock =0;
bool unlocked = false;
//prevMenu and prevEncoderPosition are used to store the previous menu location when editing settings.
menuFunc_t prevMenu = NULL;
uint16_t prevEncoderPosition;
//Variables used when editing values.
const char* editLabel;
void* editValue;
int32_t minEditValue, maxEditValue;
menuFunc_t callbackFunc;

	

// place-holders for Ki and Kd edits
float raw_Ki, raw_Kd;

/* Main status screen. It's up to the implementation specific part to show what is needed. As this is very display dependent */
static void lcd_status_screen()
{
    if (lcd_status_update_delay)
        lcd_status_update_delay--;
    else
        lcdDrawUpdate = 1;
    if (lcdDrawUpdate)
    {
        lcd_implementation_status_screen();
        lcd_status_update_delay = 10;   /* redraw the main screen every second. This is easier then trying keep track of all things that change on the screen */
    }
#ifdef ULTIPANEL
    if (LCD_CLICKED)
    {
		
		
		#ifdef UNLOCK_CODE
		(!unlocked)?currentMenu = lcd_unlock_menu:currentMenu = lcd_main_menu;
		#else
		currentMenu = lcd_main_menu; 
		#endif
		
        encoderPosition = 0;
        lcd_quick_feedback();
        lcd_implementation_init(); // to maybe revive the LCD if static electricity killed it.
    }

#ifdef ULTIPANEL_FEEDMULTIPLY
    // Dead zone at 100% feedrate
    if ((feedmultiply < 100 && (feedmultiply + int(encoderPosition)) > 100) ||
            (feedmultiply > 100 && (feedmultiply + int(encoderPosition)) < 100))
    {
        encoderPosition = 0;
        feedmultiply = 100;
    }

    if (feedmultiply == 100 && int(encoderPosition) > ENCODER_FEEDRATE_DEADZONE)
    {
        feedmultiply += int(encoderPosition) - ENCODER_FEEDRATE_DEADZONE;
        encoderPosition = 0;
    }
    else if (feedmultiply == 100 && int(encoderPosition) < -ENCODER_FEEDRATE_DEADZONE)
    {
        feedmultiply += int(encoderPosition) + ENCODER_FEEDRATE_DEADZONE;
        encoderPosition = 0;
    }
    else if (feedmultiply != 100)
    {
        feedmultiply += int(encoderPosition);
        encoderPosition = 0;
    }
#endif//ULTIPANEL_FEEDMULTIPLY

    if (feedmultiply < 10)
        feedmultiply = 10;
    if (feedmultiply > 999)
        feedmultiply = 999;
#endif//ULTIPANEL
}
#ifdef UNLOCK_CODE
static void lcd_unlock_menu()
{

	if (encoderPosition != 0)
    {
        refresh_cmd_timeout();
      if (unlockStep==0) dgt1unlock+=(int)encoderPosition;
		if (unlockStep==1) dgt2unlock+=(int)encoderPosition;
		if (unlockStep==2) dgt3unlock+=(int)encoderPosition;
		if (unlockStep==3) dgt4unlock+=(int)encoderPosition;
		delay(50);
        encoderPosition = 0;
      if (dgt1unlock<0) dgt1unlock=9;
		if (dgt1unlock>9) dgt1unlock=0;
		if (dgt2unlock<0) dgt2unlock=9;
		if (dgt2unlock>9) dgt2unlock=0;
		if (dgt3unlock<0) dgt3unlock=9;
		if (dgt3unlock>9) dgt3unlock=0;
		if (dgt4unlock<0) dgt4unlock=9;
		if (dgt4unlock>9) dgt4unlock=0;
        lcdDrawUpdate = 1;
    }
    if (lcdDrawUpdate)
    {
		char showCode[5] ;
		sprintf(showCode,"%d%d%d%d",dgt1unlock,dgt2unlock,dgt3unlock,dgt4unlock);		
		
        lcd_implementation_drawedit(PSTR("PASS CODE"),showCode );
		lcdDrawUpdate = 0;
    }
if (LCD_CLICKED)
    {		
        if (unlockStep>3) {
        	int defaultUnlockCode[]=UNLOCK_CODE;
        	int codeInserted[]={dgt1unlock,dgt2unlock,dgt3unlock,dgt4unlock};
        	bool pass=false;
        	for (short a=0; a<4;a++) {
			if (defaultUnlockCode[a] != codeInserted[a]) break;
                          else pass=true;    		
        		}
        if (pass) {		
        unlocked=true;
		  currentMenu = lcd_main_menu;
		  
	} else {
		unlocked=false;
		currentMenu = lcd_status_screen;
		}
		  dgt1unlock=0;
		  dgt2unlock=0;
		  dgt3unlock=0;
		  dgt4unlock=0;
		  unlockStep=0;
        encoderPosition = 0;
        lcd_quick_feedback();  
} else {
	unlockStep++;
	delay(200);
}		
    }	


}
static void lcd_lock()
{	
unlocked=false;
dgt1unlock=0;
dgt2unlock=0;
dgt3unlock=0;
dgt4unlock=0;
unlockStep=0;
encoderPosition = 0;
currentMenu = lcd_status_screen;		
}
#endif	
#ifdef ULTIPANEL
static void lcd_return_to_status()
{
    encoderPosition = 0;
    currentMenu = lcd_status_screen;
}

static void lcd_sdcard_pause()    
{
	card.pauseSDPrint();          
}
static void lcd_sdcard_resume()  
{
    	card.startFileprint();   
}

static void lcd_sdcard_stop()
{
    card.sdprinting = false;
    card.closefile();
    quickStop();
    if(SD_FINISHED_STEPPERRELEASE)
    {
        enquecommand_P(PSTR(SD_FINISHED_RELEASECOMMAND));
    }
if (!clayMode)    autotempShutdown();
}

static void goto_manual_xy() {
char Zposition[15];
char gotoXY[50] ;
dtostrf(current_position[Z_AXIS],1,3,Zposition);
strcpy(gotoXY,"G1 ");
strcat(gotoXY, MANUAL_POSITION_XY);
strcat(gotoXY, " F10000");
enquecommand_P(PSTR("G28"));
delay(50);
enquecommand(gotoXY);
		
}
/* Menu implementation */
static void lcd_main_menu()
{
	lcd_reset_alert_level();
	
    START_MENU();
    MENU_ITEM(back, MSG_WATCH, lcd_status_screen);
    if (movesplanned() || IS_SD_PRINTING)
    {
        MENU_ITEM(submenu, MSG_TUNE, lcd_tune_menu);
    }else{
        MENU_ITEM(submenu, MSG_PREPARE, lcd_prepare_menu);
    }
    MENU_ITEM(submenu, MSG_CONTROL, lcd_control_menu);
#ifdef SDSUPPORT
    if (card.cardOK)
    {
        if (card.isFileOpen())
        {
            if (card.sdprinting)
			{
		MENU_ITEM(gcode, MSG_PAUSE_PRINT, PSTR("M25"));
                
		if (!clayMode) MENU_ITEM(gcode, MSG_PAUSE_PRINT_WASP, PSTR("M600"));
            }
	    
       
	    MENU_ITEM(gcode, MSG_STOP_SAVE_PRINT, PSTR("M2"));
	    MENU_ITEM(submenu, MSG_STOP_PRINT, lcd_confirm_stop);
        }else{
			if (resurrectionData.resurrActive) 
				MENU_ITEM(gcode, "Resurrection", PSTR("M3"));  
            MENU_ITEM(submenu, MSG_CARD_MENU, lcd_sdcard_menu);
			
		
#if SDCARDDETECT < 1
            MENU_ITEM(gcode, MSG_CNG_SDCARD, PSTR("M21"));  // SD-card changed by user
#endif
        }
    }else{
        MENU_ITEM(submenu, MSG_NO_CARD, lcd_sdcard_menu);
#if SDCARDDETECT < 1
        MENU_ITEM(gcode, MSG_INIT_SDCARD, PSTR("M21")); // Manually initialize the SD-card via user interface
#endif
    }
#endif
#ifdef UNLOCK_CODE
	MENU_ITEM(submenu, "Lock", lcd_lock); 
#endif
	MENU_ITEM(submenu, "Info", lcd_info_sw); 
    END_MENU();
}

#ifdef SDSUPPORT
static void lcd_autostart_sd()
{
    card.lastnr=0;
    card.setroot();
    card.checkautostart(true);
}
#endif

#ifdef BABYSTEPPING
static void lcd_babystep_x()
{
    if (encoderPosition != 0)
    {
        babystepsTodo[X_AXIS]+=(int)encoderPosition;
        encoderPosition=0;
        lcdDrawUpdate = 1;
    }
    if (lcdDrawUpdate)
    {
        lcd_implementation_drawedit(PSTR(MSG_BABYSTEPPING_X),"");
    }
    if (LCD_CLICKED)
    {
        lcd_quick_feedback();
        currentMenu = lcd_tune_menu;
        encoderPosition = 0;
    }
}

static void lcd_babystep_y()
{
    if (encoderPosition != 0)
    {
        babystepsTodo[Y_AXIS]+=(int)encoderPosition;
        encoderPosition=0;
        lcdDrawUpdate = 1;
    }
    if (lcdDrawUpdate)
    {
        lcd_implementation_drawedit(PSTR(MSG_BABYSTEPPING_Y),"");
    }
    if (LCD_CLICKED)
    {
        lcd_quick_feedback();
        currentMenu = lcd_tune_menu;
        encoderPosition = 0;
    }
}

static void lcd_babystep_z()
{
    if (encoderPosition != 0)
    {
        babystepsTodo[Z_AXIS]+=BABYSTEP_Z_MULTIPLICATOR*(int)encoderPosition;
        encoderPosition=0;
        lcdDrawUpdate = 1;
    }
    if (lcdDrawUpdate)
    {
        lcd_implementation_drawedit(PSTR(MSG_BABYSTEPPING_Z),"");
    }
    if (LCD_CLICKED)
    {
        lcd_quick_feedback();
        currentMenu = lcd_tune_menu;
        encoderPosition = 0;
    }
}
#endif //BABYSTEPPING

static void lcd_tune_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_MAIN, lcd_main_menu);
    MENU_ITEM_EDIT(int3, MSG_SPEED, &feedmultiply, 10, 999);
if (!clayMode)    MENU_ITEM_EDIT(int3, MSG_NOZZLE, &target_temperature[0], 0, HEATER_0_MAXTEMP - 15);
#if TEMP_SENSOR_1 != 0
if (!clayMode)    MENU_ITEM_EDIT(int3, MSG_NOZZLE1, &target_temperature[1], 0, HEATER_1_MAXTEMP - 15);
#endif
#if TEMP_SENSOR_2 != 0
if (!clayMode)    MENU_ITEM_EDIT(int3, MSG_NOZZLE2, &target_temperature[2], 0, HEATER_2_MAXTEMP - 15);
#endif
#if TEMP_SENSOR_BED != 0
if (!clayMode)    MENU_ITEM_EDIT(int3, MSG_BED, &target_temperature_bed, 0, BED_MAXTEMP - 15);
#endif
    MENU_ITEM_EDIT(int3, MSG_FAN_SPEED, &fanSpeed, defaultfan, MAXFAN);
    MENU_ITEM_EDIT(int3, MSG_FLOW, &extrudemultiply, 10, 999);
//    MENU_ITEM_EDIT(int3, MSG_FLOW0, &extruder_multiply[0], 10, 999);
#if TEMP_SENSOR_1 != 0
    MENU_ITEM_EDIT(int3, MSG_FLOW1, &extruder_multiply[1], 10, 999);
#endif
#if TEMP_SENSOR_2 != 0
    MENU_ITEM_EDIT(int3, MSG_FLOW2, &extruder_multiply[2], 10, 999);
#endif

#ifdef BABYSTEPPING
   
    MENU_ITEM(submenu, MSG_BABYSTEP_Z, lcd_babystep_z);
#endif

    END_MENU();
}

void lcd_preheat_pla0()
{
start_timeout_preheat = millis();
    setTargetHotend0(plaPreheatHotendTemp);
    setTargetBed(plaPreheatHPBTemp);
    fanSpeed = plaPreheatFanSpeed;
    lcd_return_to_status();
    setWatch(); // heater sanity check timer
}

void lcd_preheat_abs0()
{
start_timeout_preheat = millis();
    setTargetHotend0(absPreheatHotendTemp);
    setTargetBed(absPreheatHPBTemp);
    fanSpeed = absPreheatFanSpeed;
    lcd_return_to_status();
    setWatch(); // heater sanity check timer
}

#if TEMP_SENSOR_1 != 0 //2nd extruder preheat
void lcd_preheat_pla1()
{
start_timeout_preheat = millis();
    setTargetHotend1(plaPreheatHotendTemp);
    setTargetBed(plaPreheatHPBTemp);
    fanSpeed = plaPreheatFanSpeed;
    lcd_return_to_status();
    setWatch(); // heater sanity check timer
}

void lcd_preheat_abs1()
{
start_timeout_preheat = millis();
    setTargetHotend1(absPreheatHotendTemp);
    setTargetBed(absPreheatHPBTemp);
    fanSpeed = absPreheatFanSpeed;
    lcd_return_to_status();
    setWatch(); // heater sanity check timer
}
#endif //2nd extruder preheat

#if TEMP_SENSOR_2 != 0 //3 extruder preheat
void lcd_preheat_pla2()
{
start_timeout_preheat = millis();
    setTargetHotend2(plaPreheatHotendTemp);
    setTargetBed(plaPreheatHPBTemp);
    fanSpeed = plaPreheatFanSpeed;
    lcd_return_to_status();
    setWatch(); // heater sanity check timer
}

void lcd_preheat_abs2()
{
start_timeout_preheat = millis();
    setTargetHotend2(absPreheatHotendTemp);
    setTargetBed(absPreheatHPBTemp);
    fanSpeed = absPreheatFanSpeed;
    lcd_return_to_status();
    setWatch(); // heater sanity check timer
}
#endif //3 extruder preheat

#if TEMP_SENSOR_1 != 0 || TEMP_SENSOR_2 != 0 //more than one extruder present
void lcd_preheat_pla012()
{
start_timeout_preheat = millis();
    setTargetHotend0(plaPreheatHotendTemp);
    setTargetHotend1(plaPreheatHotendTemp);
    setTargetHotend2(plaPreheatHotendTemp);
    setTargetBed(plaPreheatHPBTemp);
    fanSpeed = plaPreheatFanSpeed;
    lcd_return_to_status();
    setWatch(); // heater sanity check timer
}

void lcd_preheat_abs012()
{
start_timeout_preheat = millis();
    setTargetHotend0(absPreheatHotendTemp);
    setTargetHotend1(absPreheatHotendTemp);
    setTargetHotend2(absPreheatHotendTemp);
    setTargetBed(absPreheatHPBTemp);
    fanSpeed = absPreheatFanSpeed;
    lcd_return_to_status();
    setWatch(); // heater sanity check timer
}
#endif //more than one extruder present

void lcd_preheat_pla_bedonly()
{
    setTargetBed(plaPreheatHPBTemp);
    fanSpeed = plaPreheatFanSpeed;
    lcd_return_to_status();
    setWatch(); // heater sanity check timer
}

void lcd_preheat_abs_bedonly()
{
    setTargetBed(absPreheatHPBTemp);
    fanSpeed = absPreheatFanSpeed;
    lcd_return_to_status();
    setWatch(); // heater sanity check timer
}

static void lcd_preheat_pla_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_PREPARE, lcd_prepare_menu);
    MENU_ITEM(function, MSG_PREHEAT_PLA0, lcd_preheat_pla0);
#if TEMP_SENSOR_1 != 0 //2 extruder preheat
    MENU_ITEM(function, MSG_PREHEAT_PLA1, lcd_preheat_pla1);
#endif //2 extruder preheat
#if TEMP_SENSOR_2 != 0 //3 extruder preheat
    MENU_ITEM(function, MSG_PREHEAT_PLA2, lcd_preheat_pla2);
#endif //3 extruder preheat
#if TEMP_SENSOR_1 != 0 || TEMP_SENSOR_2 != 0 //all extruder preheat
    MENU_ITEM(function, MSG_PREHEAT_PLA012, lcd_preheat_pla012);
#endif //2 extruder preheat
#if TEMP_SENSOR_BED != 0
   // MENU_ITEM(function, MSG_PREHEAT_PLA_BEDONLY, lcd_preheat_pla_bedonly);
#endif
    END_MENU();
}

static void lcd_preheat_abs_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_PREPARE, lcd_prepare_menu);
    MENU_ITEM(function, MSG_PREHEAT_ABS0, lcd_preheat_abs0);
#if TEMP_SENSOR_1 != 0 //2 extruder preheat
    MENU_ITEM(function, MSG_PREHEAT_ABS1, lcd_preheat_abs1);
#endif //2 extruder preheat
#if TEMP_SENSOR_2 != 0 //3 extruder preheat
    MENU_ITEM(function, MSG_PREHEAT_ABS2, lcd_preheat_abs2);
#endif //3 extruder preheat
#if TEMP_SENSOR_1 != 0 || TEMP_SENSOR_2 != 0 //all extruder preheat
    MENU_ITEM(function, MSG_PREHEAT_ABS012, lcd_preheat_abs012);
#endif //2 extruder preheat
#if TEMP_SENSOR_BED != 0
//    MENU_ITEM(function, MSG_PREHEAT_ABS_BEDONLY, lcd_preheat_abs_bedonly);
#endif
    END_MENU();
}

void lcd_cooldown()
{
    setTargetHotend0(0);
    setTargetHotend1(0);
    setTargetHotend2(0);
    setTargetBed(0);
    fanSpeed = defaultfan;
    lcd_return_to_status();
}

static void lcd_prepare_menu()
{

    START_MENU();
    MENU_ITEM(back, MSG_MAIN, lcd_main_menu);
#ifdef SDSUPPORT
    #ifdef MENU_ADDAUTOSTART
      MENU_ITEM(function, MSG_AUTOSTART, lcd_autostart_sd);
    #endif
#endif
    
    MENU_ITEM(gcode, MSG_DISABLE_STEPPERS, PSTR("M84"));
    MENU_ITEM(gcode, MSG_AUTO_HOME, PSTR("G28"));
    MENU_ITEM(gcode, "G667", PSTR("G667"));
    MENU_ITEM(function, MSG_FIL_CRG, goto_manual_xy);
    MENU_ITEM(submenu, MSG_MOVE_AXIS, lcd_move_menu);
    //MENU_ITEM(gcode, MSG_SET_ORIGIN, PSTR("G92 X0 Y0 Z0"));
    
    MENU_ITEM(submenu, MSG_MODIFY_Z_MAX, lcd_modify_z_max);
    
	MENU_ITEM(submenu, MSG_WASP_MAN_LEVEL, lcd_manual_calib);
if (!clayMode) {	
	

#if TEMP_SENSOR_0 != 0
  
    MENU_ITEM(function, MSG_PREHEAT_PLA, lcd_preheat_pla0);
    MENU_ITEM(function, MSG_PREHEAT_ABS, lcd_preheat_abs0);
  
#endif
    MENU_ITEM(function, MSG_COOLDOWN, lcd_cooldown);
}
//#if PS_ON_PIN > -1
//    if (powersupply)
//    {
//        MENU_ITEM(gcode, MSG_SWITCH_PS_OFF, PSTR("M81"));
//    }else{
//        MENU_ITEM(gcode, MSG_SWITCH_PS_ON, PSTR("M80"));
//    }
//#endif
    //MENU_ITEM(submenu, MSG_MOVE_AXIS, lcd_move_menu);
    END_MENU();
}

float move_menu_scale;
static void lcd_move_menu_axis();

static void lcd_move_x()
{
    if (encoderPosition != 0)
    {
        refresh_cmd_timeout();
        current_position[X_AXIS] += float((int)encoderPosition) * move_menu_scale;
        if (min_software_endstops && current_position[X_AXIS] < X_MIN_POS)
            current_position[X_AXIS] = X_MIN_POS;
        if (max_software_endstops && current_position[X_AXIS] > X_MAX_POS)
            current_position[X_AXIS] = X_MAX_POS;
        encoderPosition = 0;
        #ifdef DELTA
        calculate_delta(current_position);
        plan_buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS], manual_feedrate[X_AXIS]/60, active_extruder);
        #else
        plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], manual_feedrate[X_AXIS]/60, active_extruder);
        #endif
        lcdDrawUpdate = 1;
    }
    if (lcdDrawUpdate)
    {
        lcd_implementation_drawedit(PSTR("X"), ftostr31(current_position[X_AXIS]));
    }
    if (LCD_CLICKED)
    {
        lcd_quick_feedback();
        currentMenu = lcd_move_menu_axis;
        encoderPosition = 0;
    }
}
static void lcd_move_y()
{
    if (encoderPosition != 0)
    {
        refresh_cmd_timeout();
        current_position[Y_AXIS] += float((int)encoderPosition) * move_menu_scale;
        if (min_software_endstops && current_position[Y_AXIS] < Y_MIN_POS)
            current_position[Y_AXIS] = Y_MIN_POS;
        if (max_software_endstops && current_position[Y_AXIS] > Y_MAX_POS)
            current_position[Y_AXIS] = Y_MAX_POS;
        encoderPosition = 0;
        #ifdef DELTA
        calculate_delta(current_position);
        plan_buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS], manual_feedrate[Y_AXIS]/60, active_extruder);
        #else
        plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], manual_feedrate[Y_AXIS]/60, active_extruder);
        #endif
        lcdDrawUpdate = 1;
    }
    if (lcdDrawUpdate)
    {
        lcd_implementation_drawedit(PSTR("Y"), ftostr31(current_position[Y_AXIS]));
    }
    if (LCD_CLICKED)
    {
        lcd_quick_feedback();
        currentMenu = lcd_move_menu_axis;
        encoderPosition = 0;
    }
}
static void lcd_move_z()
{


	
    if (encoderPosition != 0)
    {
        refresh_cmd_timeout();
        current_position[Z_AXIS] += float((int)encoderPosition) * move_menu_scale;
        if (min_software_endstops && current_position[Z_AXIS] < Z_MIN_POS)
            current_position[Z_AXIS] = Z_MIN_POS;
        if (max_software_endstops && current_position[Z_AXIS] > (Z_MAX_POS + tmpZcorrect))
            current_position[Z_AXIS] = (Z_MAX_POS + tmpZcorrect);
        encoderPosition = 0;
        #ifdef DELTA
        calculate_delta(current_position);
        plan_buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS], manual_feedrate[Z_AXIS]/60, active_extruder);
        #else
        plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], manual_feedrate[Z_AXIS]/60, active_extruder);
        #endif
        lcdDrawUpdate = 1;
    }
    if (lcdDrawUpdate)
    {
        lcd_implementation_drawedit(PSTR("Z"), ftostr31(current_position[Z_AXIS]));
    }
    if (LCD_CLICKED)
    {
        lcd_quick_feedback();
        currentMenu = lcd_move_menu_axis;
        encoderPosition = 0;
    }
}
static void lcd_change_resurr()
{
	
if (encoderPosition != 0)
    		{	
				refresh_cmd_timeout();
				tmpAutoResurr=!tmpAutoResurr;
    				encoderPosition = 0;
						
        	lcdDrawUpdate = 1;	
    		}
if (lcdDrawUpdate)
    		{
        		if (tmpAutoResurr) {
        			lcd_implementation_drawedit(PSTR("AUTORESURR"), " ON");
        		} else {
        			lcd_implementation_drawedit(PSTR("AUTORESURR"), "OFF");
        		}
    		}
if (LCD_CLICKED)
    		{
				
        		lcd_quick_feedback();
        		resurrectionData.autoResurr=tmpAutoResurr;
				Store_ResurData();	
				
        		
        		
        		currentMenu = lcd_control_menu;
        		encoderPosition = 0;
        		
        		
    		}
}
static void lcd_change_mode()
{
	float estXmove=0.00;
		
		if (encoderPosition != 0)
    		{	
				refresh_cmd_timeout();
				clayMode=!clayMode;
    				encoderPosition = 0;
						
        	lcdDrawUpdate = 1;	
    		}
		if (lcdDrawUpdate)
    		{
        		if (clayMode) {
        			lcd_implementation_drawedit(PSTR(MSG_LCD_CLAY), " ON");
        		} else {
        			lcd_implementation_drawedit(PSTR(MSG_LCD_CLAY), "OFF");
        		}
    		}
    		if (LCD_CLICKED)
    		{
        		lcd_quick_feedback();
        		if (actualClaymode!=clayMode) {
        		if (clayMode) {
        			
        			estXmove = axis_steps_per_unit[3];
        			axis_steps_per_unit[3]=tmp_e_step_per_unit;
        			tmp_e_step_per_unit=estXmove;
        		} else {
        			estXmove = tmp_e_step_per_unit;
        			tmp_e_step_per_unit=axis_steps_per_unit[3];
        			axis_steps_per_unit[3]=estXmove;
        		}
       
        		Config_StoreSettings();
        		//currentMenu = lcd_prepare_menu;
        		currentMenu = lcd_force_shutdown;
        		encoderPosition = 0;
        		
        		
    		} else {
					 currentMenu = lcd_control_menu;   		 		
    		 		encoderPosition = 0;
    		 		}
	}

}
static void lcd_force_shutdown()
{
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(MSG_FORCE_RESTART1);
	lcd.setCursor(0, 1);
	lcd.print(MSG_FORCE_RESTART2);
	lcd.setCursor(0, 2);
	lcd.print(MSG_FORCE_RESTART3);
	lcd.setCursor(0, 3);
	lcd.print(MSG_FORCE_RESTART4);
	
	do {
	} while (true);
	}
static void lcd_confirm_stop()
{
		
		if (encoderPosition != 0)
    		{	
				refresh_cmd_timeout();
				yesNo=!yesNo;
    				encoderPosition = 0;
						
        	lcdDrawUpdate = 1;	
    		}
		if (lcdDrawUpdate)
    		{
        		if (yesNo) {
        			lcd_implementation_drawedit(PSTR("ARE YOU SURE? "), "YES");
        		} else {
        			lcd_implementation_drawedit(PSTR("ARE YOU SURE? "), " NO");
        		}
    		}
    		if (LCD_CLICKED)
    		{
        		lcd_quick_feedback();
        		if (yesNo){
				lcd_sdcard_stop();
				LCD_MESSAGEPGM(MSG_STOPPED);
			}
        		currentMenu = lcd_status_screen;
        		encoderPosition = 0;
        		yesNo = false;
        		
    		}
    		
	

}
static void lcd_confirm_reset()
{
		
		if (encoderPosition != 0)
    		{	
				refresh_cmd_timeout();
				yesNo=!yesNo;
    				encoderPosition = 0;
						
        	lcdDrawUpdate = 1;	
    		}
		if (lcdDrawUpdate)
    		{
        		if (yesNo) {
        			lcd_implementation_drawedit(PSTR("RESET PARAM.? "), "YES");
        		} else {
        			lcd_implementation_drawedit(PSTR("RESET PARAM.? "), " NO");
        		}
    		}
    		if (LCD_CLICKED)
    		{
        		lcd_quick_feedback();
        		if (yesNo){
				Config_ResetDefault();
				LCD_MESSAGEPGM("Default ok");
			}
        		currentMenu = lcd_status_screen;
        		encoderPosition = 0;
        		yesNo = false;
        		
    		}
    		
	

}
static void lcd_info_sw()
{
		
	lcd.setCursor(0, 0);
	lcd.print(MSG_INFO_1L);
	lcd.setCursor(0, 1);
	lcd.print(MSG_INFO_2L);
	lcd.setCursor(0, 2);
	lcd.print(MSG_INFO_3L);
	lcd.setCursor(0, 3);
	lcd.print(LCD_STR_CROSS[0]);
	lcd.print(" Resurrection3     ");	
    		if (LCD_CLICKED)
    		{
        		lcd_quick_feedback();
        		lcd.clear();
        		currentMenu = lcd_status_screen;
        		encoderPosition = 0;
        			
    		}
    		
	

}

static void lcd_modify_z_max()
{
		

		if (encoderPosition != 0)
    		{	
				refresh_cmd_timeout();
				tmpZcorrect += float((int)encoderPosition)/2;
				if (tmpZcorrect>0.00) tmpZcorrect=0.00;
				add_homeing[2] = tmpZcorrect;
    				encoderPosition = 0;
    				
						
        	lcdDrawUpdate = 1;	
    		}
		if (lcdDrawUpdate)
    		{
        		
        		lcd_implementation_drawedit(PSTR("Z home "), ftostr52(Z_home_pos + tmpZcorrect));
			
    		}
    		if (LCD_CLICKED)
    		{
        		(clayMode)?Zcorrect=tmpZcorrect:ZcorNoClay=tmpZcorrect;	
        		Config_StoreSettings();
        		currentMenu = lcd_status_screen;
        		encoderPosition = 0;
        		lcd_quick_feedback();
        		
        		enquecommand_P(PSTR("G28"));
        		
    		}
    		
	

}

static void lcd_manual_calib()
{
	if (!homeCalib) {
		enquecommand_P(PSTR("G28"));
		homeCalib=true;
		}



		if (encoderPosition != 0)
    		{	
				refresh_cmd_timeout();
				if ((encoderPosition>4)||(encoderPosition<-4)){
					level_steps++;
    					encoderPosition = 0;
					delay(100);
				}
				
				if (level_steps>3) level_steps=0;
				
			
        		
				
        	lcdDrawUpdate = 1;	
    		}
		if (lcdDrawUpdate)
    		{
				switch (level_steps)
        			{
					case 0:
					
					enquecommand_P((PSTR("G1 Z10 F10000")));
					enquecommand_P((PSTR(MANUAL_LEVELING_CMD_0)));
						
						delay(100);
					break;
        				case 1:
						enquecommand_P((PSTR("G1 Z10 F10000")));
						enquecommand_P((PSTR(MANUAL_LEVELING_CMD_1)));
						
						delay(100);
					break;
					case 2:
						enquecommand_P((PSTR("G1 Z10 F10000")));
						enquecommand_P((PSTR(MANUAL_LEVELING_CMD_2)));
						
						delay(100);
						
					break;
					case 3:
						enquecommand_P((PSTR("G1 Z10 F10000")));
						enquecommand_P((PSTR(MANUAL_LEVELING_CMD_3)));
						
						delay(100);
				
					break;
				}
        		lcd_implementation_drawedit(PSTR(MSG_LCD_MAN_CAL), itostr2(level_steps));
				lcdDrawUpdate = 0;
    		}
    		if (LCD_CLICKED)
    		{
        		lcd_quick_feedback();
        		currentMenu = lcd_prepare_menu;
        		encoderPosition = 0;
        		level_steps = 0;
				homeCalib = false;
    		}
    		
	

}
static void lcd_move_e()
{
    if (encoderPosition != 0)
    {
        current_position[E_AXIS] += float((int)encoderPosition) * move_menu_scale;
        encoderPosition = 0;
        #ifdef DELTA
        calculate_delta(current_position);
        plan_buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS], manual_feedrate[E_AXIS]/60, active_extruder);
        #else
        plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], manual_feedrate[E_AXIS]/60, active_extruder);
        #endif
        lcdDrawUpdate = 1;
    }
    if (lcdDrawUpdate)
    {
        lcd_implementation_drawedit(PSTR("Extruder"), ftostr31(current_position[E_AXIS]));
    }
    if (LCD_CLICKED)
    {
        lcd_quick_feedback();
        currentMenu = lcd_move_menu_axis;
        encoderPosition = 0;
    }
}

static void lcd_move_menu_axis()
{
    START_MENU();
    MENU_ITEM(back, MSG_MOVE_AXIS, lcd_move_menu);
    MENU_ITEM(submenu, MSG_MOVE_X, lcd_move_x);
    MENU_ITEM(submenu, MSG_MOVE_Y, lcd_move_y);
    MENU_ITEM(submenu, MSG_MOVE_Z, lcd_move_z);
    if (move_menu_scale < 10.0)
    {
        
        MENU_ITEM(submenu, MSG_MOVE_E, lcd_move_e);
    }
    END_MENU();
}

static void lcd_move_menu_10mm()
{
    move_menu_scale = 10.0;
    lcd_move_menu_axis();
}
static void lcd_move_menu_1mm()
{
    move_menu_scale = 1.0;
    lcd_move_menu_axis();
}
static void lcd_move_menu_01mm()
{
    move_menu_scale = 0.1;
    lcd_move_menu_axis();
}

static void lcd_move_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_PREPARE, lcd_prepare_menu);
    MENU_ITEM(submenu, MSG_MOVE_10MM, lcd_move_menu_10mm);
    MENU_ITEM(submenu, MSG_MOVE_1MM, lcd_move_menu_1mm);
    MENU_ITEM(submenu, MSG_MOVE_01MM, lcd_move_menu_01mm);
    //TODO:X,Y,Z,E
    END_MENU();
}

static void lcd_control_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_MAIN, lcd_main_menu);
	if (!clayMode) MENU_ITEM(submenu, MSG_TEMPERATURE, lcd_control_temperature_menu);
    MENU_ITEM(submenu, MSG_MOTION, lcd_control_motion_menu);
    
#ifdef DOGLCD
//    MENU_ITEM_EDIT(int3, MSG_CONTRAST, &lcd_contrast, 0, 63);
    MENU_ITEM(submenu, MSG_CONTRAST, lcd_set_contrast);
#endif
#ifdef FWRETRACT
    MENU_ITEM(submenu, MSG_RETRACT, lcd_control_retract_menu);
#endif
if (!card.sdprinting) 
{
#ifdef EEPROM_SETTINGS
    MENU_ITEM(submenu, "Auto Resurrection", lcd_change_resurr);
    MENU_ITEM(submenu, MSG_CLAYMODE, lcd_change_mode);
    MENU_ITEM(function, MSG_STORE_EPROM, Config_StoreSettings);
    MENU_ITEM(function, MSG_LOAD_EPROM, Config_RetrieveSettings);
#endif
    MENU_ITEM(submenu, MSG_RESTORE_FAILSAFE, lcd_confirm_reset);
}
    END_MENU();
}

static void lcd_control_temperature_menu()
{
#ifdef PIDTEMP
    // set up temp variables - undo the default scaling
    raw_Ki = unscalePID_i(Ki);
    raw_Kd = unscalePID_d(Kd);
#endif

    START_MENU();
    MENU_ITEM(back, MSG_CONTROL, lcd_control_menu);
    MENU_ITEM_EDIT(int3, MSG_NOZZLE, &target_temperature[0], 0, HEATER_0_MAXTEMP - 15);
#if TEMP_SENSOR_1 != 0
    MENU_ITEM_EDIT(int3, MSG_NOZZLE1, &target_temperature[1], 0, HEATER_1_MAXTEMP - 15);
#endif
#if TEMP_SENSOR_2 != 0
    MENU_ITEM_EDIT(int3, MSG_NOZZLE2, &target_temperature[2], 0, HEATER_2_MAXTEMP - 15);
#endif
#if TEMP_SENSOR_BED != 0
    MENU_ITEM_EDIT(int3, MSG_BED, &target_temperature_bed, 0, BED_MAXTEMP - 15);
#endif
    MENU_ITEM_EDIT(int3, MSG_FAN_SPEED, &fanSpeed, defaultfan, MAXFAN);
#ifdef AUTOTEMP
    MENU_ITEM_EDIT(bool, MSG_AUTOTEMP, &autotemp_enabled);
    MENU_ITEM_EDIT(float3, MSG_MIN, &autotemp_min, 0, HEATER_0_MAXTEMP - 15);
    MENU_ITEM_EDIT(float3, MSG_MAX, &autotemp_max, 0, HEATER_0_MAXTEMP - 15);
    MENU_ITEM_EDIT(float32, MSG_FACTOR, &autotemp_factor, 0.0, 1.0);
#endif
#ifdef PIDTEMP
    MENU_ITEM_EDIT(float52, MSG_PID_P, &Kp, 1, 9990);
    // i is typically a small value so allows values below 1
    MENU_ITEM_EDIT_CALLBACK(float52, MSG_PID_I, &raw_Ki, 0.01, 9990, copy_and_scalePID_i);
    MENU_ITEM_EDIT_CALLBACK(float52, MSG_PID_D, &raw_Kd, 1, 9990, copy_and_scalePID_d);
# ifdef PID_ADD_EXTRUSION_RATE
    MENU_ITEM_EDIT(float3, MSG_PID_C, &Kc, 1, 9990);
# endif//PID_ADD_EXTRUSION_RATE
#endif//PIDTEMP
	MENU_ITEM(submenu, "FAN type", lcd_fan_type);
    MENU_ITEM(submenu, MSG_PREHEAT_PLA_SETTINGS, lcd_control_temperature_preheat_pla_settings_menu);
    MENU_ITEM(submenu, MSG_PREHEAT_ABS_SETTINGS, lcd_control_temperature_preheat_abs_settings_menu);
    END_MENU();
}

static void lcd_control_temperature_preheat_pla_settings_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_TEMPERATURE, lcd_control_temperature_menu);
    MENU_ITEM_EDIT(int3, MSG_FAN_SPEED, &plaPreheatFanSpeed, defaultfan, MAXFAN);
    MENU_ITEM_EDIT(int3, MSG_NOZZLE, &plaPreheatHotendTemp, 0, HEATER_0_MAXTEMP - 15);
#if TEMP_SENSOR_BED != 0
    MENU_ITEM_EDIT(int3, MSG_BED, &plaPreheatHPBTemp, 0, BED_MAXTEMP - 15);
#endif
#ifdef EEPROM_SETTINGS
    MENU_ITEM(function, MSG_STORE_EPROM, Config_StoreSettings);
#endif
    END_MENU();
}

static void lcd_control_temperature_preheat_abs_settings_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_TEMPERATURE, lcd_control_temperature_menu);
    MENU_ITEM_EDIT(int3, MSG_FAN_SPEED, &absPreheatFanSpeed, DEFAULTFAN, MAXFAN);
    MENU_ITEM_EDIT(int3, MSG_NOZZLE, &absPreheatHotendTemp, 0, HEATER_0_MAXTEMP - 15);
#if TEMP_SENSOR_BED != 0
    MENU_ITEM_EDIT(int3, MSG_BED, &absPreheatHPBTemp, 0, BED_MAXTEMP - 15);
#endif
#ifdef EEPROM_SETTINGS
    MENU_ITEM(function, MSG_STORE_EPROM, Config_StoreSettings);
#endif
    END_MENU();
}

static void lcd_control_motion_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_CONTROL, lcd_control_menu);
#ifdef ENABLE_AUTO_BED_LEVELING
    MENU_ITEM_EDIT(float32, MSG_ZPROBE_ZOFFSET, &zprobe_zoffset, 0.5, 50);
#endif
    MENU_ITEM_EDIT(float5, MSG_ACC, &acceleration, 500, 99000);
    MENU_ITEM_EDIT(float3, MSG_VXY_JERK, &max_xy_jerk, 1, 990);
    #ifndef DELTA
    MENU_ITEM_EDIT(float52, MSG_VZ_JERK, &max_z_jerk, 0.1, 990);
    #endif
    MENU_ITEM_EDIT(float3, MSG_VE_JERK, &max_e_jerk, 1, 990);
    MENU_ITEM_EDIT(float3, MSG_VMAX MSG_X, &max_feedrate[X_AXIS], 1, 999);
    MENU_ITEM_EDIT(float3, MSG_VMAX MSG_Y, &max_feedrate[Y_AXIS], 1, 999);
    MENU_ITEM_EDIT(float3, MSG_VMAX MSG_Z, &max_feedrate[Z_AXIS], 1, 999);
    MENU_ITEM_EDIT(float3, MSG_VMAX MSG_E, &max_feedrate[E_AXIS], 1, 999);
    MENU_ITEM_EDIT(float3, MSG_VMIN, &minimumfeedrate, 0, 999);
    MENU_ITEM_EDIT(float3, MSG_VTRAV_MIN, &mintravelfeedrate, 0, 999);
    MENU_ITEM_EDIT_CALLBACK(long5, MSG_AMAX MSG_X, &max_acceleration_units_per_sq_second[X_AXIS], 100, 99000, reset_acceleration_rates);
    MENU_ITEM_EDIT_CALLBACK(long5, MSG_AMAX MSG_Y, &max_acceleration_units_per_sq_second[Y_AXIS], 100, 99000, reset_acceleration_rates);
    MENU_ITEM_EDIT_CALLBACK(long5, MSG_AMAX MSG_Z, &max_acceleration_units_per_sq_second[Z_AXIS], 100, 99000, reset_acceleration_rates);
    MENU_ITEM_EDIT_CALLBACK(long5, MSG_AMAX MSG_E, &max_acceleration_units_per_sq_second[E_AXIS], 100, 99000, reset_acceleration_rates);
    MENU_ITEM_EDIT(float5, MSG_A_RETRACT, &retract_acceleration, 100, 99000);
    MENU_ITEM_EDIT(float52, MSG_XSTEPS, &axis_steps_per_unit[X_AXIS], 5, 9999);
    MENU_ITEM_EDIT(float52, MSG_YSTEPS, &axis_steps_per_unit[Y_AXIS], 5, 9999);
    MENU_ITEM_EDIT(float52, MSG_ZSTEPS, &axis_steps_per_unit[Z_AXIS], 5, 9999);
    MENU_ITEM_EDIT(float51, MSG_ESTEPS, &axis_steps_per_unit[E_AXIS], 5, 9999);
	MENU_ITEM_EDIT(int3, "Driver cur.", &currentControlDriver, 0, 120);
	MENU_ITEM_EDIT(bool, "Override BED", &overrideBed);
	
	#ifdef DELTA
	if (!card.sdprinting) {
	
	MENU_ITEM(submenu, "Delta settings", lcd_delta_adj);
	
	}
	#endif
	/*
#ifdef ABORT_ON_ENDSTOP_HIT_FEATURE_ENABLED
    MENU_ITEM_EDIT(bool, MSG_ENDSTOP_ABORT, &abort_on_endstop_hit);
#endif
*/
    END_MENU();
}
static void lcd_fan_type() {
	if (!conf) {
		(defaultfan>10)?step_adj=1:step_adj=0;
		conf=true;
	}
	
	if (encoderPosition != 0)
    {
	step_adj += (int)encoderPosition;
	if (step_adj>1) step_adj=1;
	if (step_adj<0) step_adj=0;
	encoderPosition = 0;
        
    lcdDrawUpdate = 1;
	}
	
if (LCD_CLICKED)
    {
		(step_adj==0)?menu_action_gcode(PSTR("M106 L10")):menu_action_gcode(PSTR("M106 L30"));
		lcdDrawUpdate = 1;
        lcd_quick_feedback();
		Config_StoreSettings();
        currentMenu = lcd_control_temperature_menu;
        encoderPosition = 0;	
		step_adj =0;
    }	
if (lcdDrawUpdate)
    {	
		lcd.setCursor(0,1);
		(step_adj==0)?lcd.print("FAN type A "):lcd.print("FAN type B ");
		
    }	
}
static void lcd_delta_adj()
{
    if (encoderPosition != 0)
    {
		refresh_cmd_timeout();
		if (!conf) {
			step_adj += (int)encoderPosition;
			if (step_adj>5) step_adj=0;
		}
			
		else {
		switch (step_adj) {
			case 0:
			delta_radius += float((int)encoderPosition) * 0.01;
			break;
			case 1:
			delta_diagonal_rod += float((int)encoderPosition) * 0.01;
			break;
			case 2:
			endstop_adj[0] += float((int)encoderPosition) * 0.01;
				if ((endstop_adj[0])>0) endstop_adj[0]=0;
			break;
			case 3:
			endstop_adj[1] += float((int)encoderPosition) * 0.01;
				if ((endstop_adj[1])>0) endstop_adj[0]=0;
			break;
			case 4:
			endstop_adj[2] += float((int)encoderPosition) * 0.01;
				if ((endstop_adj[2])>0) endstop_adj[0]=0;
			break;	
		} 
		}		
        encoderPosition = 0;
        
        lcdDrawUpdate = 1;
    }
    
    if (LCD_CLICKED)
    {
		conf=!conf;
		//if ((step_adj==4)&&(conf==false)) step_adj++;	
		lcdDrawUpdate = 1;
		//delay(100);
	for (int a=0; a<150;a++){
		WRITE(BEEPER,HIGH);
        	delay(1);
        	WRITE(BEEPER,LOW);
        	delay(1);
	}
	if (step_adj>4) {
		step_adj=0;
		//lcd.setCursor(10,3);
		//lcd.print("-OK-");
		delay(200);
        lcd_quick_feedback();
		recalc_delta_settings(delta_radius, delta_diagonal_rod);
        currentMenu = lcd_control_motion_menu;
        encoderPosition = 0;
		conf=false;
	}
		
		
	
		
    }
if (lcdDrawUpdate)
    {	lcd.setCursor(0,0);
		(step_adj==0)?lcd.print(">Radius       "):lcd.print(" Radius       ");
        lcd.print(ftostr32(delta_radius));
		lcd.setCursor(0,1);
		(step_adj==1)?lcd.print(">Diagonal Rod "):lcd.print(" Diagonal Rod ");
		lcd.print(ftostr32(delta_diagonal_rod));
		lcd.setCursor(0,2);
		(step_adj==2)?lcd.print(">X "):lcd.print(" X ");
		lcd.print(ftostr32(endstop_adj[0]));
		(step_adj==3)?lcd.print(">Y "):lcd.print(" Y ");
		lcd.print(ftostr32(endstop_adj[1]));
		lcd.setCursor(0,3);
		(step_adj==4)?lcd.print(">Z "):lcd.print(" Z ");
		lcd.print(ftostr32(endstop_adj[2]));
		lcd.setCursor(10,3);
		(step_adj==5)?lcd.print("-OK-"):lcd.print("    ");
    }
}


#ifdef DOGLCD
static void lcd_set_contrast()
{
    if (encoderPosition != 0)
    {
        lcd_contrast -= encoderPosition;
        if (lcd_contrast < 0) lcd_contrast = 0;
        else if (lcd_contrast > 63) lcd_contrast = 63;
        encoderPosition = 0;
        lcdDrawUpdate = 1;
        u8g.setContrast(lcd_contrast);
    }
    if (lcdDrawUpdate)
    {
        lcd_implementation_drawedit(PSTR(MSG_CONTRAST), itostr2(lcd_contrast));
    }
    if (LCD_CLICKED)
    {
        lcd_quick_feedback();
        currentMenu = lcd_control_menu;
        encoderPosition = 0;
    }
}
#endif

#ifdef FWRETRACT
static void lcd_control_retract_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_CONTROL, lcd_control_menu);
    MENU_ITEM_EDIT(bool, MSG_AUTORETRACT, &autoretract_enabled);
    MENU_ITEM_EDIT(float52, MSG_CONTROL_RETRACT, &retract_length, 0, 100);
    MENU_ITEM_EDIT(float3, MSG_CONTROL_RETRACTF, &retract_feedrate, 1, 999);
    MENU_ITEM_EDIT(float52, MSG_CONTROL_RETRACT_ZLIFT, &retract_zlift, 0, 999);
    MENU_ITEM_EDIT(float52, MSG_CONTROL_RETRACT_RECOVER, &retract_recover_length, 0, 100);
    MENU_ITEM_EDIT(float3, MSG_CONTROL_RETRACT_RECOVERF, &retract_recover_feedrate, 1, 999);
    END_MENU();
}
#endif

#if SDCARDDETECT == -1
static void lcd_sd_refresh()
{
    card.initsd();
    currentMenuViewOffset = 0;
}
#endif
static void lcd_sd_updir()
{
    card.updir();
    currentMenuViewOffset = 0;
}

void lcd_sdcard_menu()
{
    if (lcdDrawUpdate == 0 && LCD_CLICKED == 0)
        return;	// nothing to do (so don't thrash the SD card)
    uint16_t fileCnt = card.getnrfilenames();
    START_MENU();
    MENU_ITEM(back, MSG_MAIN, lcd_main_menu);
    card.getWorkDirName();
    if(card.filename[0]=='/')
    {
#if SDCARDDETECT == -1
        MENU_ITEM(function, LCD_STR_REFRESH MSG_REFRESH, lcd_sd_refresh);
#endif
    }else{
        MENU_ITEM(function, LCD_STR_FOLDER "..", lcd_sd_updir);
    }

    for(uint16_t i=0;i<fileCnt;i++)
    {
        if (_menuItemNr == _lineNr)
        {
            #ifndef SDCARD_RATHERRECENTFIRST
              card.getfilename(i);
            #else
              card.getfilename(fileCnt-1-i);
            #endif
            if (card.filenameIsDir)
            {
                MENU_ITEM(sddirectory, MSG_CARD_MENU, card.filename, card.longFilename);
            }else{
                MENU_ITEM(sdfile, MSG_CARD_MENU, card.filename, card.longFilename);
            }
        }else{
            MENU_ITEM_DUMMY();
        }
    }
    END_MENU();
}

#define menu_edit_type(_type, _name, _strFunc, scale) \
    void menu_edit_ ## _name () \
    { \
        if ((int32_t)encoderPosition < minEditValue) \
            encoderPosition = minEditValue; \
        if ((int32_t)encoderPosition > maxEditValue) \
            encoderPosition = maxEditValue; \
        if (lcdDrawUpdate) \
            lcd_implementation_drawedit(editLabel, _strFunc(((_type)encoderPosition) / scale)); \
        if (LCD_CLICKED) \
        { \
            *((_type*)editValue) = ((_type)encoderPosition) / scale; \
            lcd_quick_feedback(); \
            currentMenu = prevMenu; \
            encoderPosition = prevEncoderPosition; \
        } \
    } \
    void menu_edit_callback_ ## _name () \
    { \
        if ((int32_t)encoderPosition < minEditValue) \
            encoderPosition = minEditValue; \
        if ((int32_t)encoderPosition > maxEditValue) \
            encoderPosition = maxEditValue; \
        if (lcdDrawUpdate) \
            lcd_implementation_drawedit(editLabel, _strFunc(((_type)encoderPosition) / scale)); \
        if (LCD_CLICKED) \
        { \
            *((_type*)editValue) = ((_type)encoderPosition) / scale; \
            lcd_quick_feedback(); \
            currentMenu = prevMenu; \
            encoderPosition = prevEncoderPosition; \
            (*callbackFunc)();\
        } \
    } \
    static void menu_action_setting_edit_ ## _name (const char* pstr, _type* ptr, _type minValue, _type maxValue) \
    { \
        prevMenu = currentMenu; \
        prevEncoderPosition = encoderPosition; \
         \
        lcdDrawUpdate = 2; \
        currentMenu = menu_edit_ ## _name; \
         \
        editLabel = pstr; \
        editValue = ptr; \
        minEditValue = minValue * scale; \
        maxEditValue = maxValue * scale; \
        encoderPosition = (*ptr) * scale; \
    }\
    static void menu_action_setting_edit_callback_ ## _name (const char* pstr, _type* ptr, _type minValue, _type maxValue, menuFunc_t callback) \
    { \
        prevMenu = currentMenu; \
        prevEncoderPosition = encoderPosition; \
         \
        lcdDrawUpdate = 2; \
        currentMenu = menu_edit_callback_ ## _name; \
         \
        editLabel = pstr; \
        editValue = ptr; \
        minEditValue = minValue * scale; \
        maxEditValue = maxValue * scale; \
        encoderPosition = (*ptr) * scale; \
        callbackFunc = callback;\
    }
menu_edit_type(int, int3, itostr3, 1)
menu_edit_type(float, float3, ftostr3, 1)
menu_edit_type(float, float32, ftostr32, 100)
menu_edit_type(float, float5, ftostr5, 0.01)
menu_edit_type(float, float51, ftostr51, 10)
menu_edit_type(float, float52, ftostr52, 100)
menu_edit_type(unsigned long, long5, ftostr5, 0.01)

#ifdef REPRAPWORLD_KEYPAD
	static void reprapworld_keypad_move_z_up() {
    encoderPosition = 1;
    move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
		lcd_move_z();
  }
	static void reprapworld_keypad_move_z_down() {
    encoderPosition = -1;
    move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
		lcd_move_z();
  }
	static void reprapworld_keypad_move_x_left() {
    encoderPosition = -1;
    move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
		lcd_move_x();
  }
	static void reprapworld_keypad_move_x_right() {
    encoderPosition = 1;
    move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
		lcd_move_x();
	}
	static void reprapworld_keypad_move_y_down() {
    encoderPosition = 1;
    move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
		lcd_move_y();
	}
	static void reprapworld_keypad_move_y_up() {
		encoderPosition = -1;
		move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
    lcd_move_y();
	}
	static void reprapworld_keypad_move_home() {
		enquecommand_P((PSTR("G28"))); // move all axis home
	}
#endif

/** End of menus **/

static void lcd_quick_feedback()
{
    lcdDrawUpdate = 2;
    blocking_enc = millis() + 500;
    lcd_implementation_quick_feedback();
}

/** Menu action functions **/
static void menu_action_back(menuFunc_t data)
{
    currentMenu = data;
    encoderPosition = 0;
}
static void menu_action_submenu(menuFunc_t data)
{
    currentMenu = data;
    encoderPosition = 0;
}
static void menu_action_gcode(const char* pgcode)
{
    enquecommand_P(pgcode);
         
}
static void menu_action_function(menuFunc_t data)
{
    (*data)();
}
static void menu_action_sdfile(const char* filename, char* longFilename)
{
    char cmd[30];
    char* c;
    sprintf_P(cmd, PSTR("M23 %s"), filename);
    for(c = &cmd[4]; *c; c++)
        *c = tolower(*c);
    enquecommand(cmd);
    enquecommand_P(PSTR("M24"));
    Estopped=false;
    firstMove=true;
    lcd_return_to_status();
}
static void menu_action_sddirectory(const char* filename, char* longFilename)
{
	sprintf(resurrectionData.nameDir,filename);
	resurrectionData.nameDir[strlen(filename)]=0;
    card.chdir(filename);
    encoderPosition = 0;
}
static void menu_action_setting_edit_bool(const char* pstr, bool* ptr)
{
    *ptr = !(*ptr);
}
#endif//ULTIPANEL

/** LCD API **/
void lcd_init()
{
    lcd_implementation_init();

#ifdef NEWPANEL
    pinMode(BTN_EN1,INPUT);
    pinMode(BTN_EN2,INPUT);
    WRITE(BTN_EN1,HIGH);
    WRITE(BTN_EN2,HIGH);
  #if BTN_ENC > 0
    pinMode(BTN_ENC,INPUT);
    WRITE(BTN_ENC,HIGH);
  #endif
  #ifdef REPRAPWORLD_KEYPAD
    pinMode(SHIFT_CLK,OUTPUT);
    pinMode(SHIFT_LD,OUTPUT);
    pinMode(SHIFT_OUT,INPUT);
    WRITE(SHIFT_OUT,HIGH);
    WRITE(SHIFT_LD,HIGH);
  #endif
#else  // Not NEWPANEL
  #ifdef SR_LCD_2W_NL // Non latching 2 wire shift register
     pinMode (SR_DATA_PIN, OUTPUT);
     pinMode (SR_CLK_PIN, OUTPUT);
  #elif defined(SHIFT_CLK) 
     pinMode(SHIFT_CLK,OUTPUT);
     pinMode(SHIFT_LD,OUTPUT);
     pinMode(SHIFT_EN,OUTPUT);
     pinMode(SHIFT_OUT,INPUT);
     WRITE(SHIFT_OUT,HIGH);
     WRITE(SHIFT_LD,HIGH);
     WRITE(SHIFT_EN,LOW);
  #else
     #ifdef ULTIPANEL
     #error ULTIPANEL requires an encoder
     #endif
  #endif // SR_LCD_2W_NL
#endif//!NEWPANEL

#if defined (SDSUPPORT) && defined(SDCARDDETECT) && (SDCARDDETECT > 0)
    pinMode(SDCARDDETECT,INPUT);
    WRITE(SDCARDDETECT, HIGH);
    lcd_oldcardstatus = IS_SD_INSERTED;
#endif//(SDCARDDETECT > 0)
#ifdef LCD_HAS_SLOW_BUTTONS
    slow_buttons = 0;
#endif
    lcd_buttons_update();
#ifdef ULTIPANEL
    encoderDiff = 0;
#endif
if (resurrectionData.autoResurr && resurrectionData.resurrActive) lcd_splash_screen();
}

void lcd_update()
{
    static unsigned long timeoutToStatus = 0;

    #ifdef LCD_HAS_SLOW_BUTTONS
    slow_buttons = lcd_implementation_read_slow_buttons(); // buttons which take too long to read in interrupt context
    #endif

    lcd_buttons_update();

    #if (SDCARDDETECT > 0)
    if((IS_SD_INSERTED != lcd_oldcardstatus))
    {
        lcdDrawUpdate = 2;
        lcd_oldcardstatus = IS_SD_INSERTED;
        lcd_implementation_init(); // to maybe revive the LCD if static electricity killed it.

        if(lcd_oldcardstatus)
        {
            card.initsd();
            LCD_MESSAGEPGM(MSG_SD_INSERTED);
        }
        else
        {
            card.release();
            LCD_MESSAGEPGM(MSG_SD_REMOVED);
        }
    }
    #endif//CARDINSERTED

    if (lcd_next_update_millis < millis())
    {
#ifdef ULTIPANEL
		#ifdef REPRAPWORLD_KEYPAD
        	if (REPRAPWORLD_KEYPAD_MOVE_Z_UP) {
        		reprapworld_keypad_move_z_up();
        	}
        	if (REPRAPWORLD_KEYPAD_MOVE_Z_DOWN) {
        		reprapworld_keypad_move_z_down();
        	}
        	if (REPRAPWORLD_KEYPAD_MOVE_X_LEFT) {
        		reprapworld_keypad_move_x_left();
        	}
        	if (REPRAPWORLD_KEYPAD_MOVE_X_RIGHT) {
        		reprapworld_keypad_move_x_right();
        	}
        	if (REPRAPWORLD_KEYPAD_MOVE_Y_DOWN) {
        		reprapworld_keypad_move_y_down();
        	}
        	if (REPRAPWORLD_KEYPAD_MOVE_Y_UP) {
        		reprapworld_keypad_move_y_up();
        	}
        	if (REPRAPWORLD_KEYPAD_MOVE_HOME) {
        		reprapworld_keypad_move_home();
        	}
		#endif
        if (abs(encoderDiff) >= ENCODER_PULSES_PER_STEP)
        {
            lcdDrawUpdate = 1;
            encoderPosition += encoderDiff / ENCODER_PULSES_PER_STEP;
            encoderDiff = 0;
            timeoutToStatus = millis() + LCD_TIMEOUT_TO_STATUS;
        }
        if (LCD_CLICKED)
            timeoutToStatus = millis() + LCD_TIMEOUT_TO_STATUS;
#endif//ULTIPANEL

#ifdef DOGLCD        // Changes due to different driver architecture of the DOGM display
        blink++;     // Variable for fan animation and alive dot
        u8g.firstPage();
        do
        {
            u8g.setFont(u8g_font_6x10_marlin);
            u8g.setPrintPos(125,0);
            if (blink % 2) u8g.setColorIndex(1); else u8g.setColorIndex(0); // Set color for the alive dot
            u8g.drawPixel(127,63); // draw alive dot
            u8g.setColorIndex(1); // black on white
            (*currentMenu)();
            if (!lcdDrawUpdate)  break; // Terminate display update, when nothing new to draw. This must be done before the last dogm.next()
        } while( u8g.nextPage() );
#else
        (*currentMenu)();
#endif

#ifdef LCD_HAS_STATUS_INDICATORS
        lcd_implementation_update_indicators();
#endif

#ifdef ULTIPANEL
        if(timeoutToStatus < millis() && currentMenu != lcd_status_screen)
        {
            lcd_return_to_status();
            lcdDrawUpdate = 2;
        }
#endif//ULTIPANEL
        if (lcdDrawUpdate == 2)
            lcd_implementation_clear();
        if (lcdDrawUpdate)
            lcdDrawUpdate--;
        lcd_next_update_millis = millis() + 100;
    }
}

void lcd_setstatus(const char* message)
{
    if (lcd_status_message_level > 0)
        return;
    strncpy(lcd_status_message, message, LCD_WIDTH);
    lcdDrawUpdate = 2;
}
void lcd_setstatuspgm(const char* message)
{
    if (lcd_status_message_level > 0)
        return;
    strncpy_P(lcd_status_message, message, LCD_WIDTH);
    lcdDrawUpdate = 2;
}
void lcd_setalertstatuspgm(const char* message)
{
    lcd_setstatuspgm(message);
    lcd_status_message_level = 1;
#ifdef ULTIPANEL
    lcd_return_to_status();
#endif//ULTIPANEL
}
void lcd_reset_alert_level()
{
    lcd_status_message_level = 0;
}

#ifdef DOGLCD
void lcd_setcontrast(uint8_t value)
{
    lcd_contrast = value & 63;
    u8g.setContrast(lcd_contrast);
}
#endif

#ifdef ULTIPANEL
/* Warning: This function is called from interrupt context */
void lcd_buttons_update()
{
#ifdef NEWPANEL
    uint8_t newbutton=0;
    if(READ(BTN_EN1)==0)  newbutton|=EN_A;
    if(READ(BTN_EN2)==0)  newbutton|=EN_B;
  #if BTN_ENC > 0
    if((blocking_enc<millis()) && (READ(BTN_ENC)==0))
        newbutton |= EN_C;
  #endif
    buttons = newbutton;
    #ifdef LCD_HAS_SLOW_BUTTONS
    buttons |= slow_buttons;
    #endif
    #ifdef REPRAPWORLD_KEYPAD
      // for the reprapworld_keypad
      uint8_t newbutton_reprapworld_keypad=0;
      WRITE(SHIFT_LD,LOW);
      WRITE(SHIFT_LD,HIGH);
      for(int8_t i=0;i<8;i++) {
          newbutton_reprapworld_keypad = newbutton_reprapworld_keypad>>1;
          if(READ(SHIFT_OUT))
              newbutton_reprapworld_keypad|=(1<<7);
          WRITE(SHIFT_CLK,HIGH);
          WRITE(SHIFT_CLK,LOW);
      }
      buttons_reprapworld_keypad=~newbutton_reprapworld_keypad; //invert it, because a pressed switch produces a logical 0
	#endif
#else   //read it from the shift register
    uint8_t newbutton=0;
    WRITE(SHIFT_LD,LOW);
    WRITE(SHIFT_LD,HIGH);
    unsigned char tmp_buttons=0;
    for(int8_t i=0;i<8;i++)
    {
        newbutton = newbutton>>1;
        if(READ(SHIFT_OUT))
            newbutton|=(1<<7);
        WRITE(SHIFT_CLK,HIGH);
        WRITE(SHIFT_CLK,LOW);
    }
    buttons=~newbutton; //invert it, because a pressed switch produces a logical 0
#endif//!NEWPANEL

    //manage encoder rotation
    uint8_t enc=0;
    if(buttons&EN_A)
        enc|=(1<<0);
    if(buttons&EN_B)
        enc|=(1<<1);
    if(enc != lastEncoderBits)
    {
        switch(enc)
        {
        case encrot0:
            if(lastEncoderBits==encrot3)
                encoderDiff++;
            else if(lastEncoderBits==encrot1)
                encoderDiff--;
            break;
        case encrot1:
            if(lastEncoderBits==encrot0)
                encoderDiff++;
            else if(lastEncoderBits==encrot2)
                encoderDiff--;
            break;
        case encrot2:
            if(lastEncoderBits==encrot1)
                encoderDiff++;
            else if(lastEncoderBits==encrot3)
                encoderDiff--;
            break;
        case encrot3:
            if(lastEncoderBits==encrot2)
                encoderDiff++;
            else if(lastEncoderBits==encrot0)
                encoderDiff--;
            break;
        }
    }
    lastEncoderBits = enc;
}

void lcd_buzz(long duration, uint16_t freq)
{
#ifdef LCD_USE_I2C_BUZZER
  lcd.buzz(duration,freq);
#endif
}

bool lcd_clicked()
{
  return LCD_CLICKED;
}
#endif//ULTIPANEL

/********************************/
/** Float conversion utilities **/
/********************************/
//  convert float to string with +123.4 format
char conv[8];
char *ftostr3(const float &x)
{
  return itostr3((int)x);
}

char *itostr2(const uint8_t &x)
{
  //sprintf(conv,"%5.1f",x);
  int xx=x;
  conv[0]=(xx/10)%10+'0';
  conv[1]=(xx)%10+'0';
  conv[2]=0;
  return conv;
}

//  convert float to string with +123.4 format
char *ftostr31(const float &x)
{
  int xx=x*10;
  conv[0]=(xx>=0)?'+':'-';
  xx=abs(xx);
  conv[1]=(xx/1000)%10+'0';
  conv[2]=(xx/100)%10+'0';
  conv[3]=(xx/10)%10+'0';
  conv[4]='.';
  conv[5]=(xx)%10+'0';
  conv[6]=0;
  return conv;
}

//  convert float to string with 123.4 format
char *ftostr31ns(const float &x)
{
  int xx=x*10;
  //conv[0]=(xx>=0)?'+':'-';
  xx=abs(xx);
  conv[0]=(xx/1000)%10+'0';
  conv[1]=(xx/100)%10+'0';
  conv[2]=(xx/10)%10+'0';
  conv[3]='.';
  conv[4]=(xx)%10+'0';
  conv[5]=0;
  return conv;
}

char *ftostr32(const float &x)
{
  long xx=x*100;
  if (xx >= 0)
    conv[0]=(xx/10000)%10+'0';
  else
    conv[0]='-';
  xx=abs(xx);
  conv[1]=(xx/1000)%10+'0';
  conv[2]=(xx/100)%10+'0';
  conv[3]='.';
  conv[4]=(xx/10)%10+'0';
  conv[5]=(xx)%10+'0';
  conv[6]=0;
  return conv;
}

char *itostr31(const int &xx)
{
  conv[0]=(xx>=0)?'+':'-';
  conv[1]=(xx/1000)%10+'0';
  conv[2]=(xx/100)%10+'0';
  conv[3]=(xx/10)%10+'0';
  conv[4]='.';
  conv[5]=(xx)%10+'0';
  conv[6]=0;
  return conv;
}

char *itostr3(const int &xx)
{
  if (xx >= 100)
    conv[0]=(xx/100)%10+'0';
  else
    conv[0]=' ';
  if (xx >= 10)
    conv[1]=(xx/10)%10+'0';
  else
    conv[1]=' ';
  conv[2]=(xx)%10+'0';
  conv[3]=0;
  return conv;
}

char *itostr3left(const int &xx)
{
  if (xx >= 100)
  {
    conv[0]=(xx/100)%10+'0';
    conv[1]=(xx/10)%10+'0';
    conv[2]=(xx)%10+'0';
    conv[3]=0;
  }
  else if (xx >= 10)
  {
    conv[0]=(xx/10)%10+'0';
    conv[1]=(xx)%10+'0';
    conv[2]=0;
  }
  else
  {
    conv[0]=(xx)%10+'0';
    conv[1]=0;
  }
  return conv;
}

char *itostr4(const int &xx)
{
  if (xx >= 1000)
    conv[0]=(xx/1000)%10+'0';
  else
    conv[0]=' ';
  if (xx >= 100)
    conv[1]=(xx/100)%10+'0';
  else
    conv[1]=' ';
  if (xx >= 10)
    conv[2]=(xx/10)%10+'0';
  else
    conv[2]=' ';
  conv[3]=(xx)%10+'0';
  conv[4]=0;
  return conv;
}

//  convert float to string with 12345 format
char *ftostr5(const float &x)
{
  long xx=abs(x);
  if (xx >= 10000)
    conv[0]=(xx/10000)%10+'0';
  else
    conv[0]=' ';
  if (xx >= 1000)
    conv[1]=(xx/1000)%10+'0';
  else
    conv[1]=' ';
  if (xx >= 100)
    conv[2]=(xx/100)%10+'0';
  else
    conv[2]=' ';
  if (xx >= 10)
    conv[3]=(xx/10)%10+'0';
  else
    conv[3]=' ';
  conv[4]=(xx)%10+'0';
  conv[5]=0;
  return conv;
}

//  convert float to string with +1234.5 format
char *ftostr51(const float &x)
{
  long xx=x*10;
  conv[0]=(xx>=0)?'+':'-';
  xx=abs(xx);
  conv[1]=(xx/10000)%10+'0';
  conv[2]=(xx/1000)%10+'0';
  conv[3]=(xx/100)%10+'0';
  conv[4]=(xx/10)%10+'0';
  conv[5]='.';
  conv[6]=(xx)%10+'0';
  conv[7]=0;
  return conv;
}

//  convert float to string with +123.45 format
char *ftostr52(const float &x)
{
  long xx=x*100;
  conv[0]=(xx>=0)?'+':'-';
  xx=abs(xx);
  conv[1]=(xx/10000)%10+'0';
  conv[2]=(xx/1000)%10+'0';
  conv[3]=(xx/100)%10+'0';
  conv[4]='.';
  conv[5]=(xx/10)%10+'0';
  conv[6]=(xx)%10+'0';
  conv[7]=0;
  return conv;
}

// Callback for after editing PID i value
// grab the PID i value out of the temp variable; scale it; then update the PID driver
void copy_and_scalePID_i()
{
#ifdef PIDTEMP
  Ki = scalePID_i(raw_Ki);
  updatePID();
#endif
}

// Callback for after editing PID d value
// grab the PID d value out of the temp variable; scale it; then update the PID driver
void copy_and_scalePID_d()
{
#ifdef PIDTEMP
  Kd = scalePID_d(raw_Kd);
  updatePID();
#endif
}

#endif //ULTRA_LCD
