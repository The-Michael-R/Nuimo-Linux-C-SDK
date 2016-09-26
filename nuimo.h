#ifndef _NUIMO_H
#define _NUIMO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gio/gio.h>


/**
 * Debug-printouts in case the compiler has the option -DDEBUG or 'make debug' is used.
 * It mainly prints out the function name. Should not used for productiion executable
 */
#ifdef DEBUG
# define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif


/**
 * @defgroup NUIMO_NAMES Some names needs to be hardcoded. 
 * @{
 */
#define NUIMO_NAME             "Nuimo"
#define BT_STACK               "org.bluez"
#define BT_ADAPTER_NAME        "org.bluez.Adapter1"
#define BT_DEVICE_NAME         "org.bluez.Device1"
#define BT_CHARACTERISTIC_NAME "org.bluez.GattCharacteristic1"
/** @} */

/**
 * @defgroup NUIMO_DIRECTIONS Nuimo directions
 * Defines human redable keywords for the different Nuimo events.
 * See the official GATT documentation https://files.senic.com/nuimo-gatt-profile.pdf for changes
 * @{
 */
#define NUIMO_ROTATION_LEFT  0
#define NUIMO_ROTATION_RIGHT 1


#define NUIMO_FLY_LEFT      0
#define NUIMO_FLY_RIGHT     1
#define NUIMO_FLY_UPDOWN    4


#define NUIMO_BUTTON_RELEASE 0
#define NUIMO_BUTTON_PRESS   1


#define NUIMO_SWIPE_LEFT   0
#define NUIMO_SWIPE_RIGHT  1
#define NUIMO_SWIPE_UP     2
#define NUIMO_SWIPE_DOWN   3
#define NUIMO_TOUCH_LEFT   4
#define NUIMO_TOUCH_RIGHT  5
#define NUIMO_TOUCH_TOP    6
#define NUIMO_TOUCH_BOTTOM 7
/** @} */


/**
 * This is the master order of the individual devices/characteristics.
 * All arrays are based on this order. Please use it instead of fixed values.
 */
enum nuimo_chars_e {
  BT_ADAPTER = 0,
  NUIMO,
  NUIMO_BATTERY,
  NUIMO_LED,
  NUIMO_BUTTON, 
  NUIMO_FLY,
  NUIMO_SWIPE,  
  NUIMO_ROTATION,
  NUIMO_ENTRIES_LEN  /// No Characteristic, but can be used for loops
};


// public functions
void nuimo_print_status ();
int  nuimo_init_bt ();
int  nuimo_init_search (const char* key, const char* val);
void nuimo_init_cb_function(void *cb_function);
int  nuimo_init_status ();
void nuimo_disconnect ();
int  nuimo_set_led(const unsigned char* bitmap, const unsigned char brightness, const unsigned char timeout);
int  nuimo_read_value(const unsigned char characteristic);


#endif
