#include "nuimo.h"

const char NUIMO_UUID[NUIMO_ENTRIES_LEN][37] = {
  "                                    ", // Blank for BT-Adapter
  "00001800-0000-1000-8000-00805f9b34fb", // NUIMO
  "00002a19-0000-1000-8000-00805f9b34fb", // NUIMO_BATTERY 
  "f29b1524-cb19-40f3-be5c-7241ecb82fd1", // NUIMO_LED     
  "f29b1529-cb19-40f3-be5c-7241ecb82fd2", // NUIMO_BUTTON  
  "f29b1526-cb19-40f3-be5c-7241ecb82fd2", // NUIMO_FLY     
  "f29b1527-cb19-40f3-be5c-7241ecb82fd2", // NUIMO_SWIPE   
  "f29b1528-cb19-40f3-be5c-7241ecb82fd2"  // NUIMO_ROTATION
};


typedef struct {
  char       *path;
  gboolean    connected;
  GDBusProxy *proxy;
  gulong      char_sig_hdl;
}characteristic_s;


struct nuimo_status_s {
  char               *keyword;  // e.g. "Address"
  char               *value;    // e.g. "xx:xx:xx:xx:xx:xx"
  gulong              object_added_sig_hdl;
  GDBusObjectManager *manager;
  gboolean            connected;
  void              (*cb_function)(uint, int, uint);
  characteristic_s    characteristic[NUIMO_ENTRIES_LEN];
};


static struct nuimo_status_s *my_nuimo;


// private function
// call back routine preformats the received change and call the user call back function
static void cb_change_val_notify (GDBusProxy *proxy, GVariant *changed_properties, GStrv invalidated_properties, gpointer user_data) {
  GVariant   *v2;
  const char *value;
  gsize       len;
  gint16       number;
  uint        direction = 0;

  DEBUG_PRINT(("cb_change_val_notify\n"));
  
  v2 = g_variant_lookup_value(changed_properties, "Value", NULL);
  
  if (!v2) {
    return;
  }
  value = g_variant_get_fixed_array(v2, &len, 1);


  switch (GPOINTER_TO_INT(user_data)) {
  case NUIMO_BATTERY :
  case NUIMO_BUTTON :
    number = value[0];
    break;
  case NUIMO_SWIPE :
    number = 0;
    direction = value[0];
    break;
  case NUIMO_FLY :
    number = value[1];
    direction = value[0];
    break;
    
  case NUIMO_ROTATION :
    number = value[1] * 256 + value[0];
    direction = number > 0 ? NUIMO_ROTATION_LEFT : NUIMO_ROTATION_RIGHT;
    break;
    
  default:
    // unexpected call.
    DEBUG_PRINT(("  Unexpected call of cb_change_val_notify!\n"));
    return;
  }

  my_nuimo->cb_function(GPOINTER_TO_INT(user_data), number, direction);
}


// private function
// uses the bt_adapter to connect to the Nuimo (after checking that the Nuimo matches the
// key/value pair if given)
static void connect_nuimo (GDBusObjectManager *manager, GDBusObject *object) {
  GVariant    *variant = NULL;
  GList       *if_list, *interfaces;
  GError      *DBerror;
  const gchar *path;
  uint         length;
  
  DEBUG_PRINT(("connect_nuimo\n"));

  path       = g_dbus_object_get_object_path(object);
  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));
      
  for (if_list = interfaces; if_list != NULL; if_list = if_list->next) {

    // Search for Nuimo
    variant = g_dbus_proxy_get_cached_property (if_list->data, "Name");
    if (!my_nuimo->characteristic[BT_ADAPTER].path &&
	variant && !strcmp(NUIMO_NAME, g_variant_get_string(variant, NULL))) {

      // If keyword is set check if the value matches. if not continue
      if (my_nuimo->keyword) {
	variant = g_dbus_proxy_get_cached_property (if_list->data, my_nuimo->keyword);
	if (!variant || strcmp(my_nuimo->value, g_variant_get_string(variant, NULL))) {
	  continue;
	}
      }
	
      // Just found the Nuimo I was looking for. So connect to it
      length = strlen(path) + 1;
      my_nuimo->characteristic[NUIMO].path = malloc(length);
      memset(my_nuimo->characteristic[NUIMO].path, '\0', length);
      strncpy(my_nuimo->characteristic[NUIMO].path, path, length);
      
      my_nuimo->characteristic[NUIMO].proxy = (GDBusProxy*) g_dbus_object_manager_get_interface(manager,
												my_nuimo->characteristic[NUIMO].path,
												BT_DEVICE_NAME);
      DBerror = NULL;
      g_dbus_proxy_call_sync(my_nuimo->characteristic[NUIMO].proxy,
			     "Connect",
			     NULL,
			     G_DBUS_CALL_FLAGS_NONE,
			     -1,
			     NULL,
			     &DBerror);

      if(DBerror) {
	fprintf(stderr, "*EE* Error connecting: %s\n", DBerror->message);
	free(my_nuimo->characteristic[BT_ADAPTER].path);
	g_variant_unref(variant);
	g_error_free(DBerror);
	return;
      }

      // As I'm connected now start looking for Characteristics
      DBerror = NULL;
      g_dbus_proxy_call_sync( my_nuimo->characteristic[BT_ADAPTER].proxy,
			      "StopDiscovery",
			      NULL,
			      G_DBUS_CALL_FLAGS_NONE,
			      -1,
			      NULL,
			      &DBerror);

      if(DBerror) {
	fprintf(stderr, "*EE* Error StopDiscovery: %s\n", DBerror->message);
	g_variant_unref(variant);
	g_error_free(DBerror);
	return;
      }
      
      my_nuimo->connected = TRUE;
      break;
    }
  }
  
  if (variant) {
    g_variant_unref(variant);
  }
}


// private function
// Gather all characteristics and setup change notification for all of them
static void get_characteristics(GDBusObjectManager *manager, GDBusObject *object) {
  GVariant    *variant = NULL;
  GList       *if_list, *interfaces;
  const gchar *path;
  uint         length;
  uint         i;
  GError      *DBerror;

  DEBUG_PRINT(("get_characteristics\n"));

  path = g_dbus_object_get_object_path(object);
  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));

  for (if_list = interfaces; if_list != NULL; if_list = if_list->next) {
    variant = g_dbus_proxy_get_cached_property (if_list->data, "UUID");

    if (variant) {
      length = strlen(path) + 1;

      for (i = NUIMO_BATTERY; i < NUIMO_ENTRIES_LEN; i++) {
	if (!my_nuimo->characteristic[i].path && !strcmp(NUIMO_UUID[i], g_variant_get_string(variant, NULL))) {
	  
	  my_nuimo->characteristic[i].path = malloc(length);
	  memset(my_nuimo->characteristic[i].path, 0, length);
	  strncpy(my_nuimo->characteristic[i].path, path, length);
	  
	  my_nuimo->characteristic[i].proxy = (GDBusProxy*) g_dbus_object_manager_get_interface(my_nuimo->manager,
												my_nuimo->characteristic[i].path,
												BT_CHARACTERISTIC_NAME);
	  DEBUG_PRINT(("UUID = %s\n", NUIMO_UUID[i]));

	  //The LED characteristic has no notify function; skip this 
	  if (i != NUIMO_LED) {

	    DBerror = NULL;
	    g_dbus_proxy_call_sync(my_nuimo->characteristic[i].proxy,
				   "StartNotify",
				   NULL,
				   G_DBUS_CALL_FLAGS_NONE,
				   -1,
				   NULL,
				   &DBerror);

	    if(DBerror) {
	      fprintf(stderr, "*EE* Error StartNotify (UUID: %s): %s\n", NUIMO_UUID[i], DBerror->message);
	      g_variant_unref(variant);
	      g_error_free(DBerror);
	      return;
	    }
	  
	    my_nuimo->characteristic[i].char_sig_hdl = g_signal_connect (my_nuimo->characteristic[i].proxy,
									 "g-properties-changed",
									 G_CALLBACK (cb_change_val_notify),
									 GINT_TO_POINTER(i));
	  }

	  break;
	}
      }
    }
  }
  
  if (variant) {
    g_variant_unref(variant);
  }
}


// private function
// Receives a signal in case a object (Nuimo or characteristic) is newly found
static void cb_object_added (GDBusObjectManager *manager, GDBusObject *object, gpointer user_data) {
  DEBUG_PRINT(("cb_object_added\n"));

  if (!my_nuimo->connected) {
    connect_nuimo(manager, object);
  } else {
    get_characteristics(manager, object);
  }
}


// public function
// During debugging this may print some helpful information
void nuimo_print_status() {
  printf("\nCurrent Nuimo Status\n");
  printf("====================\n");
  printf("  Nuimo is%s connected\n"      , my_nuimo->connected ? "" : " not");
  printf("  Got Nuimo proxy %s\n"        , my_nuimo->characteristic[NUIMO].proxy ? "yes" : " no");
  printf("  status->device_path   = %s\n", my_nuimo->characteristic[NUIMO].path);
  printf("  status->battery_path  = %s\n", my_nuimo->characteristic[NUIMO_BATTERY].path);
  printf("  status->led_path      = %s\n", my_nuimo->characteristic[NUIMO_LED].path);
  printf("  status->button_path   = %s\n", my_nuimo->characteristic[NUIMO_BUTTON].path);
  printf("  status->fly_path      = %s\n", my_nuimo->characteristic[NUIMO_FLY].path);
  printf("  status->swipe_path    = %s\n", my_nuimo->characteristic[NUIMO_SWIPE].path);
  printf("  status->rotation_path = %s\n", my_nuimo->characteristic[NUIMO_ROTATION].path);
  printf("\n");
}


// public function
// Sends the provided bitpattern to the connected Nuimo.
// bitmap must be an array of 11 Bytes representing the 9x9 bittmap
// brightness is the brightness of the LED
// timeout the time the bitmap is displayed (0...25.5 seconds)
int  nuimo_set_led(const unsigned char* bitmap, const unsigned char brightness, const unsigned char timeout) {
  unsigned char  pattern[13];
  GVariant      *varled;
  GVariant      *vtest[2];
  GError        *DBerror;

  DEBUG_PRINT(("nuimo_set_led\n"));

  memcpy(pattern, bitmap, 11);
  pattern[11] = brightness;
  pattern[12] = timeout;

 
  vtest[0] = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, pattern, 13, 1);
  vtest[1] = g_variant_new ("a{sv}", NULL);
  varled = g_variant_new_tuple(vtest, 2);

  DBerror = NULL;
  g_dbus_proxy_call_sync(my_nuimo->characteristic[NUIMO_LED].proxy,
			 "WriteValue",
			 varled,
			 G_DBUS_CALL_FLAGS_NONE,
			 -1,
			 NULL,
			 &DBerror);
  
  if(DBerror) {
    fprintf(stderr, "*EE* Error WriteValue: %s\n", DBerror->message);
    g_variant_unref(varled);
    g_variant_unref(vtest[0]);
    g_variant_unref(vtest[1]);
    g_error_free(DBerror);
    return(EXIT_FAILURE);
  }

  return(EXIT_SUCCESS);
}  


// public function
// Issue a read value. After the read is done the callback function is issued and returning the requestedd value
// characteristic indicates wich walue is requested; see nuimo_chars_e
int  nuimo_read_value(const unsigned char characteristic) {
  GVariant *sendvar;
  GError   *DBerror;

  DEBUG_PRINT(("nuimo_read_value\n"));
 
  // Adding no flags, but build the structure
  sendvar = g_variant_new ("(a{sv})", NULL);

  DBerror = NULL;
  g_dbus_proxy_call_sync(my_nuimo->characteristic[characteristic].proxy,
			 "ReadValue",
			 sendvar,
			 G_DBUS_CALL_FLAGS_NONE,
			 -1,
			 NULL,
			 &DBerror);
  
  if(DBerror) {
    fprintf(stderr, "*EE* Error RedadValue: %s\n", DBerror->message);
    g_variant_unref(sendvar);
    g_error_free(DBerror);
    return(EXIT_FAILURE);
  }

  return(EXIT_SUCCESS);
}


// public function
// Initializes the my_nuimo structure
int nuimo_init_status() {
  uint i;

  DEBUG_PRINT(("nuimo_init_status\n"));

  my_nuimo = (struct nuimo_status_s *) malloc(sizeof(struct nuimo_status_s));

  if (!my_nuimo) {
    return(EXIT_FAILURE);
  }
  
  i = 0;
  while (i < NUIMO_ENTRIES_LEN) {
    my_nuimo->characteristic[i].connected = FALSE;
    i++;
  }

  return(EXIT_SUCCESS);
}


// public function
// Sets Keyword and Value to search for. Useful if more than one Nuimo is in the area
// In case the Keyword or value is already set it will be deleted
// key   is the keyword (e.g. "Address")
// value is the value you're looking for (e.g. "xx:xx:xx:xx:xx:xx")
int nuimo_init_search(const char* key, const char* val) {
  DEBUG_PRINT(("nuimo_init_search\n"));
  
  if (key && val) {
    if (my_nuimo->keyword) {
      free(my_nuimo->keyword);
    }
    if (my_nuimo->value) {
      free(my_nuimo->value);
    }
    
    my_nuimo->keyword = malloc(strlen(key) + 1);
    my_nuimo->value   = malloc(strlen(val) + 1);
    if (!my_nuimo->keyword || !my_nuimo->value) {
      return(EXIT_FAILURE);
    }
    
    strcpy(my_nuimo->keyword, key);
    strcpy(my_nuimo->value,   val);
  }
  
  return(EXIT_SUCCESS);
}


// public function
// Assigns the callback function for the user. The function must receive three values:
// void cb_function(uint characteristic, int value, uint direction)
//
// characteristic is the identifier (see characteristic_s)
// value          is the value of movement
// direction      is (if applicable) informs you about direction of movement
//                see defines in nuimo.h
int nuimo_init_cb_function(void *cb_function) {
  my_nuimo->cb_function = cb_function;
  
  return(EXIT_SUCCESS);
}


// public function
// Disconnects from Nuimo and all characteristics and do some cleanup
void nuimo_disconnect () {
  uint i = NUIMO_ENTRIES_LEN;

  DEBUG_PRINT(("disconnect_nuimo\n"));

  if (my_nuimo->object_added_sig_hdl) {
    g_signal_handler_disconnect(my_nuimo->manager,
				my_nuimo->object_added_sig_hdl);
    my_nuimo->object_added_sig_hdl = 0;
  }
  
  // In case I'm still looking for the Nuimo
  if (my_nuimo->manager) {
    g_dbus_proxy_call_sync(my_nuimo->characteristic[BT_ADAPTER].proxy,
			   "StopDiscovery",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   NULL,
			   NULL);
  }
  
  while(i > NUIMO) {
    i--;
    
    if (my_nuimo->characteristic[i].path) {
      free(my_nuimo->characteristic[i].path);
      my_nuimo->characteristic[i].path = NULL;
    }
    
    if (my_nuimo->characteristic[i].proxy) {
      if (my_nuimo->characteristic[i].char_sig_hdl) {
	g_signal_handler_disconnect(my_nuimo->characteristic[i].proxy,
				    my_nuimo->characteristic[i].char_sig_hdl);
	my_nuimo->characteristic[i].char_sig_hdl = 0;
      }

      g_dbus_proxy_call_sync(my_nuimo->characteristic[i].proxy,
			     "Disconnect",
			     NULL,
			     G_DBUS_CALL_FLAGS_NONE,
			     -1,
			     NULL,
			     NULL);
    }
  }

  if (my_nuimo->manager) {
    g_object_unref(my_nuimo->manager);
    my_nuimo->manager = NULL;
  } 
  my_nuimo->connected = FALSE;
}



// public function
// Initializes the BT stack.
// It also starts looking for devices
int nuimo_init_bt() {
  GError         *DBerror = NULL;
  GList          *objects;
  GList          *ob_list;
  GDBusInterface *interface;
  
  DEBUG_PRINT(("nuimo_init_bt\n"));
  
  my_nuimo->manager = g_dbus_object_manager_client_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
								    G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
								    BT_STACK,
								    "/",
								    NULL,
								    NULL,
								    NULL,
								    NULL,
								    &DBerror);
  if (!my_nuimo->manager) {
    fprintf(stderr, "*EE* Error getting object manager client: %s\n", DBerror->message);
    g_error_free(DBerror);
    return (EXIT_FAILURE);
  }
  
  my_nuimo->object_added_sig_hdl = g_signal_connect (my_nuimo->manager,
						     "object-added",
						     G_CALLBACK (cb_object_added),
						     NULL);

  objects = g_dbus_object_manager_get_objects(my_nuimo->manager);
  for (ob_list = objects; ob_list != NULL; ob_list = ob_list->next) {
    
    interface = g_dbus_object_get_interface (ob_list->data, BT_ADAPTER_NAME);
    if(interface) {
      my_nuimo->characteristic[BT_ADAPTER].proxy = G_DBUS_PROXY (interface);

      DBerror = NULL;
      g_dbus_proxy_call_sync( my_nuimo->characteristic[BT_ADAPTER].proxy,
			      "StartDiscovery",
			      NULL,
			      G_DBUS_CALL_FLAGS_NONE,
			      -1,
			      NULL,
			      &DBerror);

      if(DBerror) {
	fprintf(stderr, "*EE* Error StartDiscovery: %s\n", DBerror->message);
	g_error_free(DBerror);
	return (EXIT_SUCCESS);
      }
      break;
    }
  }
    
  return (EXIT_SUCCESS);
}
