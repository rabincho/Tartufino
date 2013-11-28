/*This file defines ethernet and wifi addresses of the boards mounted
  on the Bluebot.
  Further port of the services implemented in the system are predefined.
  */

/**IP addresses over ethernet**/

/*GEA is the front board that provide the lcd interface. Unlikely the board will be 
changed with another one, so the name shouldn't lead to confusion.*/
#define ETH_GEA "192.168.254.100"

/*Two boards will be placed on the cage on the top of the BlueBot.
These boards can be beagleboard, pandaboard, igep or something else.*/
#define ETH_BOARD1 "192.168.254.200"
#define ETH_BOARD2 "192.168.254.210"

/*The FLEX on the cage probably will never make use of ethernet connection.*/
#define ETH_FLEX "192.168.254.220"

/*A default ip for a connected PC is set. Probably it won't be necessary inside the
BlueBot to know the ip of a potential PC since at the moment we don't plan to run any
server on a PC*/
#define ETH_PC "192.168.254.150"

/**Wifi addresses over ethernet**/

/*Only a single board inside the BlueBot will have wifi extension activated. Probably one
of the two inside the cage,*/
#define WIFI_PC "192.168.1.1"
#define WIFI_BOARD1 "192.168.1.2"
#define WIFI_BOARD2 "192.168.1.3"

/**Port of the services**/
#define PORT_CAMERA "3490"
#define PORT_LOGGER "3495"
#define PORT_REMOTE_CONTROLLER "3500"

/**Possible communication mode for services**/
#define MODE_CANBUS 0
#define MODE_SOCKET 1

struct service_parameters{
  char *ip_service;
  char *port_service;
  int mode;
  void *args;
};

struct logger_service_parameters{
  struct service_parameters *logger;
  struct service_parameters *camera;
};

struct remotecontroller_service_parameters{
  struct service_parameters *remotecontroller;
  struct service_parameters *motors;
};
