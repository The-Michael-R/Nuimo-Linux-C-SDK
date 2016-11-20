# Nuimo-SDK
## What is this code for?
The Nuimo is a nice looking and from the feeling well build gadget. It can be used to control your IoT or just your T. As [Senic](http://www.senic.com/) (the company behind the Nuimo) is so kind and published the [Nuimo GATT profile](https://files.senic.com/nuimo-gatt-profile.pdf) I decided to creade a C based SDK/Library to make the creation of new usages much easier.

The Nuimo uses BLE to connect with the environment. This SDK (or library) uses the GLIB DBus interface to connect to the Nuimo. No worries, you don't need to get in touch with the BLE protocol, the DBus handling. Everything is encapsulated.

A simple example is included (example.c) and a more sophisticated usage of this code can be found [here](https://github.com/The-Michael-R/nuimod).

Note: I'm not related to Senic. I just happen to own one of such gadget.


## make options
How to build something
- `make` builds the example
- `make debug` builds the example with enabled debug printing 
- `make doc` builds the example and the documentation (./doc)
- `make clean` removes all binarys


## Requirements
The code requires beside the usual components glib2 and bluez 5.40 (or newer until the BlueZ team decides to change the interface) installed.
To generate the documenation doxygen is required

The code was tested on a RasPi with ArchLinux and should build without any warnings.


## Usage
The following code shows the complete, but minimal, usage of the SDK. The example uses all(!) public functions. Yes, it's very useless but what do you expect for such a short example. There is little else you need to learn. Hmm, you might have a look into the example.c if you would like to check for some minor details. Especially a more complete `my_cb_function()`.

```c
#include <glib-unix.h>
#include "nuimo.h"

static gboolean cb_termination(gpointer data) {
  // To terminate the main loop
  g_main_loop_quit(data);
  return(FALSE);
}

void my_cb_function(unsigned int characteristic, int value, unsigned int dir, void *user_data) {
  // Use the characteristic, value and dir to get the user action on Nuimo
  unsigned char img[11] = "<insert bitpattern>";

  if (characteristic == NUIMO_BUTTON && dir == NUIMO_BUTTON_PRESS) {
    nuimo_read_value(NUIMO_BATTERY);                 // The next my_cb_function call will receive the result!
    nuimo_set_led(img, 0x80, 50);                    // Write bitpattern to LED-Matrix
  }
}

int main (int argc, char **argv) {
  GMainLoop *loop;
 
  nuimo_init_status();                               // Internal housekeeping
  nuimo_init_search("Address", "DB:3B:2B:xx:xx:xx"); // Optional: Wait for the Nuimo with the right Key/Value pair (insert your Nuimo MAC)
  nuimo_init_cb_function(my_cb_function, NULL);      // Attach callback function
  nuimo_init_bt();                                   // Initialize Bluetooth-Stack and start searching Nuimo

  loop = g_main_loop_new(NULL, FALSE);               // Initialize background main loop (required to receive signals!)
  g_unix_signal_add (SIGINT,  cb_termination, loop); // Attach callback function to terminate the loop and stop programm
  g_main_loop_run(loop);                             // Program enters background and is waiting for messages from Nuimo. Hit ctrl-c to stop

  nuimo_print_status();                              // Just to use this function, might useless in final code
  
  nuimo_disconnect();                                // Disconnect and clean-up internal structures

  return (EXIT_SUCCESS);                             // Bye!
}
```
For additional explanation of the functions, see the nuimo.c and the defines nuimo.h. Use `make doc` to create a nice doxygen documentation.


## ToDo
The list has grown rather short. And I actually don't know if I'll implement this (anytime soon):

1. Multiple Nuimo support. Small workaround to make sure everytime the same Nuimo connects: Use the `nuimo_init_search()` function to select the correct device

