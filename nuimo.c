#include "nuimo.h"

// prototypes for private functions
static void cb_change_val_notify (GDBusProxy *proxy, GVariant *changed_properties, GStrv invalidated_properties, gpointer user_data);
static void connect_nuimo (GDBusObjectManager *manager, GDBusObject *object);
static void get_characteristics(GDBusObjectManager *manager, GDBusObject *object);
static void cb_object_added (GDBusObjectManager *manager, GDBusObject *object, gpointer user_data);
static void cb_object_removed (GDBusObjectManager *manager, GDBusObject *object, gpointer user_data);


/**
 * List of individual UUIDs of the Nuimo and the individual characteristics.
 * The order must be the same like in ::nuimo_chars_e.
 */
const char NUIMO_UUID[NUIMO_ENTRIES_LEN][37] = {
  "                                    ", /// Blank for BT-Adapter
  "00001800-0000-1000-8000-00805f9b34fb", /// NUIMO
  "00002a19-0000-1000-8000-00805f9b34fb", /// NUIMO_BATTERY 
  "f29b1524-cb19-40f3-be5c-7241ecb82fd1", /// NUIMO_LED     
  "f29b1529-cb19-40f3-be5c-7241ecb82fd2", /// NUIMO_BUTTON  
  "f29b1526-cb19-40f3-be5c-7241ecb82fd2", /// NUIMO_FLY     
  "f29b1527-cb19-40f3-be5c-7241ecb82fd2", /// NUIMO_SWIPE   
  "f29b1528-cb19-40f3-be5c-7241ecb82fd2"  /// NUIMO_ROTATION
};


/**
 * Structure used to manage the individual Characteristics and devices (BT-Adapter and the Nuimo itself).
 * The structure will be used in the ::nuimo_status_s only.
 *
 * \warning This is private stuff. No need to access from the user!
 */
typedef struct {
  char       *path;
  gboolean    connected;
  GDBusProxy *proxy;
  gulong      char_sig_hdl;
}characteristic_s;


/**
 * Defines the structure of the structure that holds all required information about 
 * the BT-Adapter, the Nuimo and its characteristics.
 *
 * \warning This is private stuff. No need to access from the user!
 */
struct nuimo_status_s {
  char               *keyword;                           /// Used for search a specific Nuimo (e.g. "Address")
  char               *value;                             /// Used for search a specific Nuimo (e.g. "xx:xx:xx:xx:xx:xx")
  gulong              object_added_sig_hdl;              /// Holds the handler for 'BT found new device' events
  gulong              object_removed_sig_hdl;            /// Holds the handler for 'BT lost a conneted device'
  GDBusObjectManager *manager;                           /// GDbus manager.
  gboolean            active_discovery;                  /// Just to remember that the code started a discovery
  void              (*cb_function)(unsigned int, int, unsigned int, void*);     /// This is the pointer to the user callback function
  void               *user_data;                         /// Pointer to userdata. Can be a pointer to a struct.
  characteristic_s    characteristic[NUIMO_ENTRIES_LEN]; /// An array of structs to manage all required information for each characteristic and devices
};


/**
 * Private global (sorry) variable holding all information about the connected Nuimo
 */
static struct nuimo_status_s *my_nuimo;

/**
 * Callback routine preformats the received change and call the user call back function
 * It also catches the Nuimo related messages (including disconnct). Currently this get not exposed to user
 *
 * @param proxy
 * @param changed_properties 
 * @param invalidated_properties
 * @param user_data
*/
static void cb_change_val_notify (GDBusProxy *proxy, GVariant *changed_properties, GStrv invalidated_properties, gpointer user_data) {
  GVariant    *v2;
  const unsigned char  *value;
  gsize        len;
  gint16       number;
  unsigned int direction = 0;

  DEBUG_PRINT(("cb_change_val_notify\n"));


  // Check if te Nuimo just got disconnected
  if (GPOINTER_TO_INT(user_data) == NUIMO) {
    v2 = g_variant_lookup_value(changed_properties, "Connected", NULL);
    if (v2 && !g_variant_get_boolean(v2)) {
      // Do the hard way: remove everything and start from the beginning
      nuimo_disconnect();
      nuimo_init_bt();
    }
  } 

  v2 = g_variant_lookup_value(changed_properties, "Value", NULL);
  
  if (!v2) {
    return;
  }
  value = g_variant_get_fixed_array(v2, &len, 1);


  switch (GPOINTER_TO_INT(user_data)) {
  case NUIMO_BATTERY :
    number = value[0];
    break;
    
  case NUIMO_BUTTON :
  case NUIMO_SWIPE :
    number    = 0;
    direction = value[0];
    break;
    
  case NUIMO_FLY :
    number    = value[1];
    direction = value[0];
    break;
    
  case NUIMO_ROTATION :
    number    = ((value[1] & 255) << 8) + (value[0] & 255);
    direction = number > 0 ? NUIMO_ROTATION_LEFT : NUIMO_ROTATION_RIGHT;
    break;
    
  default:
    // unexpected call.
    DEBUG_PRINT(("  Unexpected call of cb_change_val_notify!\n"));
    return;
  }

  my_nuimo->cb_function(GPOINTER_TO_INT(user_data), number, direction, my_nuimo->user_data);
}


/**
 * uses the bt_adapter to connect to the Nuimo (after checking that the Nuimo matches the
 * key/value pair if given)
 *
 * @param manager
 * @param object
*/
static void connect_nuimo (GDBusObjectManager *manager, GDBusObject *object) {
  GVariant    *variant = NULL;
  GList       *if_list, *interfaces;
  GError      *DBerror;
  const gchar *path;
  
  DEBUG_PRINT(("connect_nuimo\n"));
 
  if (!my_nuimo->characteristic[BT_ADAPTER].proxy) {
    return;
  }

  path       = g_dbus_object_get_object_path(object);
  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));
      
  for (if_list = interfaces; if_list != NULL; if_list = if_list->next) {

    // Search for Nuimo
    variant = g_dbus_proxy_get_cached_property (if_list->data, "Name");
    if (variant && !strcmp(NUIMO_NAME, g_variant_get_string(variant, NULL))) {

      // If keyword is set check if the value matches. if not continue
      if (my_nuimo->keyword) {
	variant = g_dbus_proxy_get_cached_property (if_list->data, my_nuimo->keyword);
	if (!variant || strcmp(my_nuimo->value, g_variant_get_string(variant, NULL))) {
	  continue;
	}
      }
      
      // Just found the Nuimo I was looking for. So connect to it
      my_nuimo->characteristic[NUIMO].path = strdup(path);

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

      if (DBerror) {
	fprintf(stderr, "*EE* Error connecting: %s\n", DBerror->message);
	free(my_nuimo->characteristic[NUIMO].path);
	my_nuimo->characteristic[NUIMO].path = NULL;
	g_variant_unref(variant);
	g_error_free(DBerror);
	return;
      }

      if (my_nuimo->active_discovery) {
	// As I'm connected now start looking for Characteristics
	DBerror = NULL;
	g_dbus_proxy_call_sync( my_nuimo->characteristic[BT_ADAPTER].proxy,
				"StopDiscovery",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&DBerror);

	if (DBerror) {
	  fprintf(stderr, "*EE* Error StopDiscovery: %s\n", DBerror->message);
	  g_variant_unref(variant);
	  g_error_free(DBerror);
	  return;
	}
	my_nuimo->active_discovery = FALSE;
      }

      // Connect to signals from Nuimo; including if it gets disconnected	  
      my_nuimo->characteristic[NUIMO].char_sig_hdl = g_signal_connect (my_nuimo->characteristic[NUIMO].proxy,
								       "g-properties-changed",
								       G_CALLBACK (cb_change_val_notify),
								       GINT_TO_POINTER(NUIMO));

      // Connect to object-removed signal to see if the Nuimo disappears
      my_nuimo->object_removed_sig_hdl = g_signal_connect (my_nuimo->manager,
							   "object-removed",
							   G_CALLBACK (cb_object_removed),
							   NULL);


      
      my_nuimo->characteristic[NUIMO].connected = TRUE;
      break;
    }
  }

  if (variant) {
    g_variant_unref(variant);
  }
}


/**
 * Gather all characteristics and setup change notification for all of them
 *
 * @param manager
 * @param object
 */
static void get_characteristics(GDBusObjectManager *manager, GDBusObject *object) {
  GVariant    *variant = NULL;
  GList       *if_list, *interfaces;
  const gchar *path;
  unsigned int i;
  GError      *DBerror;

  DEBUG_PRINT(("get_characteristics\n"));

  path = g_dbus_object_get_object_path(object);
  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));

  for (if_list = interfaces; if_list != NULL; if_list = if_list->next) {
    variant = g_dbus_proxy_get_cached_property (if_list->data, "UUID");

    if (variant) {
      for (i = NUIMO_BATTERY; i < NUIMO_ENTRIES_LEN; i++) {
	if (!my_nuimo->characteristic[i].path && !strcmp(NUIMO_UUID[i], g_variant_get_string(variant, NULL))) {

	  my_nuimo->characteristic[i].path = strdup(path);
	  
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


/**
 * Receives a signal in case a object (Nuimo or characteristic) is newly found
 *
 * @param manager
 * @param object
 * @param user_data
 */
static void cb_object_added (GDBusObjectManager *manager, GDBusObject *object, gpointer user_data) {
  DEBUG_PRINT(("cb_object_added\n"));

  if (!my_nuimo->characteristic[NUIMO].connected) {
    connect_nuimo(manager, object);
  } else {
    get_characteristics(manager, object);
  }
}


/**
 * Receives a signal in case the Nuimo gets disconnected
 * First all handles must released and thn the search needs to be re-initated
 *
 * @param manager
 * @param object
 * @param user_data
 */
static void cb_object_removed (GDBusObjectManager *manager, GDBusObject *object, gpointer user_data) {
  DEBUG_PRINT(("cb_object_removed\n"));

  GVariant *variant = NULL;
  GList    *if_list, *interfaces;

  if (!my_nuimo->characteristic[BT_ADAPTER].proxy) {
    return;
  }
  
  // Check if the Nuimo triggered the object-removed event
  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));
      
  for (if_list = interfaces; if_list != NULL; if_list = if_list->next) {

    // Search for Nuimo
    variant = g_dbus_proxy_get_cached_property (if_list->data, "Name");
    if (variant && !strcmp(NUIMO_NAME, g_variant_get_string(variant, NULL))) {

      // If keyword is set check if the value matches. if not continue
      if (my_nuimo->keyword) {
	variant = g_dbus_proxy_get_cached_property (if_list->data, my_nuimo->keyword);
	if (!variant || strcmp(my_nuimo->value, g_variant_get_string(variant, NULL))) {
	  continue;
	}
      }
      // Do the hard way: remove everything and start from the beginning
      nuimo_disconnect();
      nuimo_init_bt();
    }
  }
}


/**
 * During debugging this may print some helpful information. Might not be used
 * in production code.
*/
void nuimo_print_status() {
  printf("\nCurrent Nuimo Status\n");
  printf("====================\n");
  printf("  Got BT_ADAPTER proxy %s\n"   , my_nuimo->characteristic[BT_ADAPTER].proxy ? "yes" : " no");
  printf("  Nuimo is%s connected\n"      , my_nuimo->characteristic[NUIMO].connected ? "" : " not");
  printf("  Got Nuimo proxy %s\n"        , my_nuimo->characteristic[NUIMO].proxy ? "yes" : " no");
  printf("  status->device_path   = %s\n", my_nuimo->characteristic[NUIMO].path);
  if (my_nuimo->characteristic[NUIMO].path) {
    printf("  status->battery_path  = %s\n", my_nuimo->characteristic[NUIMO_BATTERY].path);
    printf("  status->led_path      = %s\n", my_nuimo->characteristic[NUIMO_LED].path);
    printf("  status->button_path   = %s\n", my_nuimo->characteristic[NUIMO_BUTTON].path);
    printf("  status->fly_path      = %s\n", my_nuimo->characteristic[NUIMO_FLY].path);
    printf("  status->swipe_path    = %s\n", my_nuimo->characteristic[NUIMO_SWIPE].path);
    printf("  status->rotation_path = %s\n", my_nuimo->characteristic[NUIMO_ROTATION].path);
  }
  printf("\n");
}


/**
 * Sends the provided bit pattern to the connected Nuimo LED matrix. Format of the bitmap is the upper left
 * LED is in bitmap[0],bit 0; while the lower right LED is in bitmap[10], bit 0
 *
 * @param bitmap     Must be an array of 11 Bytes representing the 9x9 bitmap
 * @param brightness Is the brightness of the LED
 * @param timeout    The time the bitmap is displayed (0...25.5 seconds)
 * @param mode       Selects the transition mode of (0 = fade in, else fast transition between patterns)
 * @return Returns EXIT_SUCCESS or EXIT_FAILURE depending if the request was successful or not
*/
int  nuimo_set_led(const unsigned char* bitmap, const unsigned char brightness, const unsigned char timeout, const unsigned char mode) {
  unsigned char  pattern[13];
  GVariant      *varled;
  GVariant      *vtest[2];
  GError        *DBerror;

  DEBUG_PRINT(("nuimo_set_led\n"));

  memcpy(pattern, bitmap, 10);
  pattern[10] = (bitmap[10] & 0x01) | (mode == 0 ? 0x00 : 0x10); 
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


/**
 * Displays the selected icon on the LED matrix. Dependin on the FW of the Nuimo you can
 * select one icon out of 255(?) 
 *
 * @param icon       Icon to be displayed (e.g. 0 = epty, 1 = scan-animation, 2 = Yin&Yang, ...)
 * @param brightness Is the brightness of the LED
 * @param timeout    The time the bitmap is displayed (0...25.5 seconds)
 * @param mode       Selects the transition mode of (0 = fade in, else fast transition between patterns)
 * @return Returns EXIT_SUCCESS or EXIT_FAILURE depending if the request was successful or not
*/
int nuimo_set_icon(const unsigned char icon, const unsigned char brightness, const unsigned char timeout, const unsigned char mode)
{
  unsigned char  pattern[13] = { 0 };
  GVariant      *varled;
  GVariant      *vtest[2];
  GError        *DBerror;

  DEBUG_PRINT(("nuimo_set_icon\n"));

  pattern[0]  = icon;
  pattern[10] = 0x20 | (mode == 0 ? 0x00 : 0x10); 
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


/**
 * Issue a read value. After the read is done the callback function is issued and returning the requestedd
 * value characteristic indicates wich walue is requested;
 *
 * @param characteristic Defines the characteristic to read from ::nuimo_chars_e
 * @return Returns EXIT_SUCCESS or EXIT_FAILURE depending if the request was successful or not
*/
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


/**
 * Initializes the my_nuimo structure
 *
 * @return Returns EXIT_SUCCESS or EXIT_FAILURE depending if the request was successful or not
 */
int nuimo_init_status() {
  unsigned int i;

  DEBUG_PRINT(("nuimo_init_status\n"));

  my_nuimo = malloc(sizeof(struct nuimo_status_s));

  if (!my_nuimo) {
    return(EXIT_FAILURE);
  }

  my_nuimo->keyword     = NULL;
  my_nuimo->value       = NULL;
  my_nuimo->manager     = NULL;
  my_nuimo->cb_function = NULL;
  my_nuimo->user_data   = NULL;

  my_nuimo->object_added_sig_hdl   = 0;
  my_nuimo->object_removed_sig_hdl = 0;
  
  my_nuimo->active_discovery = FALSE;

  i = 0;
  while (i < NUIMO_ENTRIES_LEN) {
    my_nuimo->characteristic[i].connected = FALSE;
    my_nuimo->characteristic[i].path      = NULL;
    my_nuimo->characteristic[i].proxy     = NULL;
    i++;
  }

  return(EXIT_SUCCESS);
}


/**
 * Sets Keyword and Value to search for. Useful if more than one Nuimo is in the area
 * In case the Keyword or value is already set it will be deleted
 *
 * @param key The keyword (e.g. "Address")
 * @param val The value you're looking for (e.g. "xx:xx:xx:xx:xx:xx")
*/
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

/**
 * Assigns the callback function for the user. The function must receive three values:
 * void cb_function(uint characteristic, int value, uint direction) with the following parameters:
 * \n \n 
 * characteristic The identifier (see ::characteristic_s) \n 
 * value          The value of movement. In case of SWIPE/TOUCH the value is 0 \n 
 * direction      Informs you about direction of movement. In case of BUTTON and BATTERY events the value is 0
 */
void nuimo_init_cb_function(void *cb_function, void *user_data) {
  DEBUG_PRINT(("nuimo_init_cb_function\n"));
  
  my_nuimo->cb_function = cb_function;
  my_nuimo->user_data = user_data;
}


/**
 * Disconnects from Nuimo and all characteristics and do some cleanup.
 */
void nuimo_disconnect () {
  unsigned int i = NUIMO_ENTRIES_LEN;

  DEBUG_PRINT(("nuimo_disconnect\n"));

  if (my_nuimo->object_added_sig_hdl) {
    g_signal_handler_disconnect(my_nuimo->manager,
				my_nuimo->object_added_sig_hdl);
    my_nuimo->object_added_sig_hdl = 0;
  }
  
  if (my_nuimo->object_removed_sig_hdl) {
    g_signal_handler_disconnect(my_nuimo->manager,
				my_nuimo->object_removed_sig_hdl);
    my_nuimo->object_removed_sig_hdl = 0;
  }
  
  // In case I'm still looking for the Nuimo
  if (my_nuimo->manager &&  my_nuimo->active_discovery) {
    g_dbus_proxy_call_sync(my_nuimo->characteristic[BT_ADAPTER].proxy,
			   "StopDiscovery",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   NULL,
			   NULL);
  }
  my_nuimo->active_discovery = FALSE;
  
  while(i > NUIMO) {
    i--;
    
    if (my_nuimo->characteristic[i].path) {
      free(my_nuimo->characteristic[i].path);
      my_nuimo->characteristic[i].path = NULL;
    }
    
    if (my_nuimo->characteristic[i].proxy) {
      if (my_nuimo->characteristic[i].char_sig_hdl) {

	g_dbus_proxy_call_sync(my_nuimo->characteristic[i].proxy,
			       "StopNotify",
			       NULL,
			       G_DBUS_CALL_FLAGS_NONE,
			       -1,
			       NULL,
			       NULL);
	
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
      my_nuimo->characteristic[i].proxy = NULL;
    }
  }

  if (my_nuimo->manager) {
    g_object_unref(my_nuimo->manager);
    my_nuimo->manager = NULL;
  } 
  my_nuimo->characteristic[NUIMO].connected = FALSE;
}


/**
 * Initializes the BT stack and start looking for devices like the Nuimo 
 *
 * @return Returns EXIT_SUCCESS or EXIT_FAILURE depending if the request was successful or not
 */
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

  // First look for the BT-Adapter.
  for (ob_list = objects; ob_list != NULL; ob_list = ob_list->next) {
    
    interface = g_dbus_object_get_interface (ob_list->data, BT_ADAPTER_NAME);
    if(interface) {
      my_nuimo->characteristic[BT_ADAPTER].proxy = G_DBUS_PROXY (interface);

      break;
    }
  }

  if (!my_nuimo->characteristic[BT_ADAPTER].proxy) {
    return EXIT_FAILURE;
  }

  // If successful, run a second loop and check if the Nuimo is already known
  for (ob_list = objects; ob_list != NULL; ob_list = ob_list->next) {
    connect_nuimo(my_nuimo->manager, ob_list->data);
    if (my_nuimo->characteristic[NUIMO].connected) {
      break;
    }
  }
  
  if (my_nuimo->characteristic[NUIMO].connected) {
    for (ob_list = objects; ob_list != NULL; ob_list = ob_list->next) {
      get_characteristics(my_nuimo->manager, ob_list->data);
    }
  }

  
  // if the Nuimo is not in the list, start looking activley
  if (!my_nuimo->characteristic[NUIMO].connected && !my_nuimo->active_discovery) {
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
      return (EXIT_FAILURE);
    }
    my_nuimo->active_discovery = TRUE;
  }
  
  return EXIT_SUCCESS;
}
