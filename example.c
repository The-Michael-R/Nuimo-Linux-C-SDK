#include "example.h"

// Stops the g_main_loop; call this function to stop the programm
static gboolean cb_termination(gpointer data) {
  DEBUG_PRINT(("cb_termination\n"));
  
  g_main_loop_quit(data);

  return(FALSE);
}



void my_cb_function(uint chr, int value, uint dir) {
  unsigned char img1[] = {0x04, 0x18, 0x70, 0xe0, 0xc1, 0x87, 0x07, 0x07, 0x06, 0x04, 0x00}; // image bitmap

  DEBUG_PRINT(("my_cb_function\n"));

  switch (chr) {
  case NUIMO_BATTERY:
    printf("BATTERY %d%%\n", value);
    break;
    
  case NUIMO_BUTTON:
    printf("BUTTON %s\n", value ==  NUIMO_BUTTON_PRESS ? "pressed" : "released" );
    if (value == NUIMO_BUTTON_PRESS) {
      nuimo_set_led(img1, 0x80, 50);
    }
    break;
    
  case NUIMO_FLY:
    printf("FLY ");
    if (dir == NUIMO_FLY_LEFT) {
      printf("left\n");
    } else if (dir == NUIMO_FLY_RIGHT) {
      printf("right\n");
    } else if (dir == NUIMO_FLY_BACKWARDS) {
      printf("backwards\n");
    }  else if (dir == NUIMO_FLY_TORWARDS) {
      printf("torwards\n");
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
    }
    break;
    
  case NUIMO_ROTATION:
    printf("ROTATE %s %d steps\n", dir == NUIMO_ROTATION_LEFT ? "->" : "<-", value);
    
    break;
  default:
    printf("An error happened...");
  }
}


int main (int argc, char **argv) {
  GMainLoop *loop;
 
  nuimo_init_status();
  // Additionaly you can add a filter:
  // nuimo_init_search("Address", "DB:3B:2B:xx:xx:xx");
  nuimo_init_cb_function(my_cb_function);
  nuimo_init_bt();  // Not much will happen until the g_main_loop is started

  loop = g_main_loop_new(NULL, FALSE);
  g_unix_signal_add (SIGINT,  cb_termination, loop); // Calling a private FKT!
  g_main_loop_run(loop);

  // Programm terminated, clean up after print the old status info!
  nuimo_print_status();
  
  nuimo_disconnect();

  return (EXIT_SUCCESS);
}
