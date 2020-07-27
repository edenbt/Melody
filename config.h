//change it for each player (num player and the loop/send/#)
#define PLAYER 0
const char*  topic = "loop/send/2";

//setting variables
#define NUMPLAYERS 3
#define NUMKEYS 6
#define SONGSIZE 64
#define BRIGHTNESS 50

////////// Timing
#define BPM 85
#define BARS 4
#define NOTESinBit 4
#define BITSinBar 4

#undef  MQTT_MAX_PACKET_SIZE // un-define max packet size
#define MQTT_MAX_PACKET_SIZE SONGSIZE*NUMPLAYERS  // fix for MQTT client dropping messages over 128B

//////////Network +MQTT
// Change the credentials below, so your ESP8266 connects to your network. USE A HOTSPOT! Proper WiFi does not work for some reason
const char* ssid = "";
const char* password = "";


// Run Mosquitto, and enter the port number 
const char* mqtt_server = "0.tcp.ngrok.io";
int port = 19931;

//button pins setup
#define Strip_PIN D2   // LED Metrix PIN
#define Matrix_PIN D3   // LED Metrix PIN
#define REC_PIN D5   // record button pin
#define UP_PIN D6   // upload button pin
