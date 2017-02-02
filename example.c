#include "example.h"

/**
 * Stops the g_main_loop; call this function to stop the programm
 *
 * @param data Is the g_main_loop handle returnes by ::g_main_loop_new
 */
static gboolean cb_termination(gpointer data) {
  DEBUG_PRINT(("cb_termination\n"));
  
  g_main_loop_quit(data);

  return(FALSE);
}


/**
 * Convert a bitmap-string to array
 * Useful for painting LED-Matrix. But the function is written to be not specific
 * for this functin. So please notice: As the 9x9 bitmap for the Nuimo LED matrix
 * does not fit well into the 8-Bit array the marix must modified before send to Nuimo
 * by using img[10] >>= 7;
 *
 * @param bmp   In case for Nuimo usage: This is a srring of 82 chars (81 for pattern + \0)
 * @param array Return of the char pattern. No malloc/free is performed in this function!
 */
void bmp_to_array(const unsigned char *bmp, unsigned char *array) {
  unsigned int bmp_pos;
  unsigned int array_pos;
  unsigned int bit;

  bmp_pos = 0;
  array_pos = 0;
  bit = 0;
  
  while(bmp[bmp_pos]) {
    array[array_pos] >>= 1;

    if (bmp[bmp_pos] == '1' || bmp[bmp_pos] == '*') {
      array[array_pos] = array[array_pos] + 0x80;
    }

    if (bit == 7) {
      bit = 0;
      array_pos++;
      array[array_pos] = 0;
    } else {
      bit++;
    }
    bmp_pos++;
  }
}

/**
 * Main example callback function used to execute user actions in case the Nuimo characteristics
 * send a change-value notification. To install this function use ::nuimo_init_cb_function
 *
 * @param characteristic The characteristic based on ::nuimo_chars_e
 * @param value          Any decimal returnvalue from the Nuimo (in case of SWIPE/TOUCH events the value is 0)
 * @param dir            Indicated the direction of the received event. based on \ref NUIMO_DIRECTIONS
 * @param user_data      Pointer to user data. Can be a struct holding information what to do for each event.
 * @see cb_change_val_notify
 * @see nuimo_init_cb_function
 */
void my_cb_function(unsigned int characteristic, int value, unsigned int dir, void *user_data) {
  unsigned char bmp[] =
    "........."
    ".*..*..*."
    ".*..*...."
    ".*..*..*."
    ".****..*."
    ".*..*..*."
    ".*..*..*."
    ".*..*..*."
    "........."; 
  unsigned char img[11];

  DEBUG_PRINT(("my_cb_function\n"));

    
  switch (characteristic) {
  case NUIMO_BATTERY:
    printf("BATTERY %d%%\n", value);
    break;
    
  case NUIMO_BUTTON:
    printf("BUTTON %s\n", dir ==  NUIMO_BUTTON_PRESS ? "pressed" : "released" );
    if (dir == NUIMO_BUTTON_PRESS) {
      bmp_to_array(bmp, img);
      // This right-shift is required as the bmp_to_array is written general, but the Nuimo
      // expectes this single bit on the 'other' end
      img[10] >>= 7;
      nuimo_set_led(img, 0x80, 50, 1);
    } else {
      nuimo_set_icon(01, 0x80, 50, 1);
    }
    break;
    
  case NUIMO_FLY:
    printf("FLY ");
    if (dir == NUIMO_FLY_LEFT) {
      printf("left\n");
    } else if (dir == NUIMO_FLY_RIGHT) {
      printf("right\n");
    }  else if (dir == NUIMO_FLY_UPDOWN) {
      printf("up/down %d\n", value);
    }
    break;
    
  case NUIMO_SWIPE:
    if (dir == NUIMO_SWIPE_LEFT) {
      printf("SWIPE left\n");
    } else if (dir == NUIMO_SWIPE_RIGHT) {
      printf("SWIPE right\n");
    } else if (dir == NUIMO_SWIPE_UP) {
      printf("SWIPE up\n");
    }  else if (dir == NUIMO_SWIPE_DOWN) {
      printf("SWIPE down\n");
      // issue a read-value. This function here will be called after the result reaches this computer
      nuimo_read_value(NUIMO_BATTERY);
    } else if (dir == NUIMO_TOUCH_LEFT) {
      printf("TOUCH left\n");
    } else if (dir == NUIMO_TOUCH_RIGHT) {
      printf("TOUCH right\n");
    } else if (dir == NUIMO_TOUCH_TOP) {
      printf("TOUCH top\n");
    } else if (dir == NUIMO_TOUCH_BOTTOM) {
      printf("TOUCH bottom\n");
    } else if (dir == NUIMO_LONG_TOUCH_LEFT) {
      printf("LONG TOUCH left\n");
    } else if (dir == NUIMO_LONG_TOUCH_RIGHT) {
      printf("LONG TOUCH right\n");
    } else if (dir == NUIMO_LONG_TOUCH_TOP) {
      printf("LONG TOUCH top\n");
    } else if (dir == NUIMO_LONG_TOUCH_BOTTOM) {
      printf("LONG TOUCH bottom\n");      
    }
    break;
    
  case NUIMO_ROTATION:
    printf("ROTATE %s %d steps\n", dir == NUIMO_ROTATION_LEFT ? "->" : "<-", value);
    
    break;
  default:
    printf("An error happened...");
  }
}

/**
 * Default main function. It ignores any given parameter.
 * To initialize, use and close the connection to the Nuimo please follow the
 * code here. It's very simple.
 *
 * @param argc Ignored
 * @param argv Ignored
 */
int main (int argc, char **argv) {
  GMainLoop *loop;
 
  nuimo_init_status();
  // Additionaly you can add a filter:
  // nuimo_init_search("Address", "DB:3B:2B:xx:xx:xx");
  nuimo_init_cb_function(my_cb_function, NULL);
  nuimo_init_bt();  // Not much will happen until the g_main_loop is started

  loop = g_main_loop_new(NULL, FALSE);
  g_unix_signal_add (SIGINT,  cb_termination, loop); // Calling a private FKT!
  g_main_loop_run(loop);

  // Programm terminated, clean up after print the old status info!
  nuimo_print_status();
  
  nuimo_disconnect();

  return (EXIT_SUCCESS);
}
