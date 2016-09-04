# Nuimo-SDK
Nuimo SDK using the GLIB DBus library

As there is a lack of a C-Based SDK for the Nuimo (http://www.senic.com/) I decided to see if I can fill this hole.

Note: I'm not related to Senic. I just happen to own one of the gadget :-)

# Requirements
The code requires glib2 and bluez 5.40 (or newer) installed.

The code was tested on a RasPi with ArchLinux.

# ToDo
1. If the bluetooth-stack has the Nuimo already recognized the code won't find it. Use bluetoothctl --> remove * to clear the cache
2. If the BT-Link breaks the code does not recognize it and won't reconnect
(3. Multiple Nuimo support)
