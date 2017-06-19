/* -*- c++ -*- */

/*
   This firmware is UNOFFICIAL version of the original Marlin
Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm.
This is a modified version partially rewrited by Dennis Patella (Ðen) WASProject on 2014/2015 
http://www.wasproject.it
and adapted for WASP DELTA PRINTERS and WASP HARDWARE
The use with different hardware is absolutely not tested and not recommended
Some improvements: 
- full compatibility with DELTA WASP MACHINES
- Integrated recovery print
- Emergency stop with print rescue
- Manual levelling delta
- improvement in changing filament (M600)
- pause M25 with extruder lifting
- some additional lcd menu
- for NEW WASP ELECTRONICS with current control motors
- ...and much more
- Rev 1 fix resurrection bug
- Rev4 modify Z home for FDM and Clay mode 
  stop and save enhancement and implemented with M2 command
- Rev5 adjust extruder temp and limit preheating time
-rev6 correct bed temp visualization for some boards 
-rev6.2 return to old temp values, PID in bed heating, MAXTEMP extruder 300
-rev6.4 Aug 2016 new resurrection system
-rev6.5 possibility to override bed limits and DEFAULT FAN. M665 set everithing for DELTA. M106 set DEFAULT FAN 
 
Some features like Resurrectio system are in CREATIVE COMMONS LICENSE

 This program is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 
 */


#include "Configuration.h"
#include "pins.h"

#ifdef ULTRA_LCD
  #if defined(LCD_I2C_TYPE_PCF8575)
    #include <Wire.h>
    #include <LiquidCrystal_I2C.h>
  #elif defined(LCD_I2C_TYPE_MCP23017) || defined(LCD_I2C_TYPE_MCP23008)
    #include <Wire.h>
    #include <LiquidTWI2.h>
  #elif defined(DOGLCD)
    #include <U8glib.h> // library for graphics LCD by Oli Kraus (https://code.google.com/p/u8glib/)
  #else
    #include <LiquidCrystal.h> // library for character LCD
  #endif
#endif

#if defined(DIGIPOTSS_PIN) && DIGIPOTSS_PIN > -1
#include <SPI.h>
#endif

#if defined(DIGIPOT_I2C)
  #include <Wire.h>
#endif
