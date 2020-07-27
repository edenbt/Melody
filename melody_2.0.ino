#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "AudioFileSourceSPIFFS.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include "AudioOutputMixer.h"
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

// Initializes the espClient
WiFiClient espClient;
PubSubClient client(espClient);

// initialzing the digital pins for the buttons
int recButton = REC_PIN;
int upButton  = UP_PIN;

////////// Button state
int buttonValue; //Stores analog value when button is pressed
int recButtonState = 0;
int upButtonState = 0;

////////// Timing
const int NUMofNotes = NUMPLAYERS * NUMKEYS;
unsigned long lastNoteTime = 0;
unsigned long lastPlayTime = 0;
// here we should get the amount of milis per each note to play
// get the note length in minutes
unsigned long bitInterval = 1000 / (BPM*NOTESinBit / 60) ;

//flags
int playerTurn = 0;
bool doneRec = false;
bool doneLoop = false;
int loopsCounter = 0;
bool startFlag = true;
int newNoteFlag[NUMPLAYERS];

////////////indexes
int noteInedx = 0;
int localNoteIndex = 0;
int loopNewNote[NUMPLAYERS];
int loopOldNote [NUMPLAYERS];
int localNewNote;
int localOldNote;


/////////ESPAudio Class initilaize//////////
AudioGeneratorWAV *wav[NUMPLAYERS];
AudioFileSourceSPIFFS *file[NUMPLAYERS];
AudioOutputMixer *mixer;
AudioOutputMixerStub *stub[NUMPLAYERS];
AudioOutputI2S *out;



///////////// Music Arrays
const char* paths[NUMofNotes] = {"/blank1.wav", "/Chords_Am.wav", "/Chords_F.wav", "/Chords_C.wav", "/Chords_G.wav", "/Chords_Dm.wav", "/blank2.wav", "/Lead_C.wav", "/Lead_D.wav", "/Lead_E.wav", "/Lead_G.wav", "/Lead_A.wav", "/blank0.wav", "/Bass_C3.wav", "/Bass_D3.wav", "/Bass_F3.wav", "/Bass_G3.wav", "/Bass_A3.wav"};
char loopNotesChar[SONGSIZE * NUMPLAYERS + 1]; // the song that the speakers play
char loopNotesIncoming[SONGSIZE * NUMPLAYERS + 1]; // a buffer that saves the song
int notesChar[] = {'0', '1', '2', '3', '4', '5'}; // holds a char index to put in the string
char localLoopString [SONGSIZE * NUMPLAYERS + 1]; // the song that we record


/// LED///
Adafruit_NeoPixel strip = Adafruit_NeoPixel(32, Strip_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel matrix = Adafruit_NeoPixel(64, Matrix_PIN, NEO_GRB + NEO_KHZ800);
uint32_t GREEN = strip.Color(0, 255, 0);
uint32_t RED = strip.Color(255, 0, 0);
uint32_t BLUE = strip.Color(0, 0, 255);
uint32_t YELLOW = strip.Color(255, 184, 0);
uint32_t STRIPCOLOR = strip.Color(3, 252, 100);
uint32_t colors[4] = {strip.Color(183, 3, 252), strip.Color(3, 252, 148), strip.Color(252, 3, 132), strip.Color(252, 94, 3)};//array of colors for the matrix
//////////


// This function connects your ESP8266 to your network
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.begin(115200);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected - ESP IP address: ");
  Serial.println(WiFi.localIP());

}



// This functions reconnects your ESP8266 to your MQTT broker
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("Player" + PLAYER)) {
      Serial.println("connected");
      client.subscribe("loop/get");
      client.subscribe("loop/turn");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      // Wait 1 seconds before retrying
      delay(2000);
    }
  }
}


//callback function
// this fuction gets the messages from the MQTT broker
void callback(char* topic, byte* payload, unsigned int length) {
  //check if the message is from the turn channel
  if (strcmp(topic, "loop/turn") == 0) {
    char c[length] ;
    memcpy(c, payload, length);
    playerTurn = (c[0] - '0');
  }

  //check if the message is from the get channel
  if (strcmp(topic, "loop/get") == 0) {
    memcpy(loopNotesIncoming, payload, length);
    Serial.print("Message arrived [");
    Serial.print(loopNotesIncoming);
    Serial.print("] ");
    Serial.println();
  }
  Serial.print("turn callback end ");
  Serial.println(playerTurn);

}


// the setup routine runs once when you press reset:
void setup() {
  Serial.begin(115200);// initialize serial communication at 9600 bits per second:

  //setting up the connection and MQTT conection
  setup_wifi();

  client.setServer(mqtt_server, port);
  client.setCallback(callback);

  pinMode(recButton, INPUT_PULLUP);
  pinMode(upButton, INPUT_PULLUP);


  //ISP setup
  out = new AudioOutputI2S();
  mixer = new AudioOutputMixer(32, out);

  // put your setup code here, to run once:
  // setup of the LED strip
#if defined (__AVR_ATtiny85__)
  if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show(); // Initialize all pixels to 'off'
  /////
  matrix.begin();
  matrix.setBrightness(BRIGHTNESS);
  matrix.show(); // Initialize all pixels to 'off'
  /////

  //set up the arrays
  for (int i = 0; i < NUMPLAYERS; i++) {
    loopNewNote[i] = 0;
    loopOldNote[i] = 0;
    stub[i] = mixer->NewInput();
    stub[i]->SetGain(0.05);
    wav[i] = new AudioGeneratorWAV();
    newNoteFlag[i] = 1;
  }
  for (int i = 0; i < SONGSIZE * NUMPLAYERS; i++) {
    loopNotesChar[i] = '0';
    loopNotesIncoming[i] = '0';
    localLoopString[i] = '0';
  }
  loopNotesChar[SONGSIZE * NUMPLAYERS] = '\0';
  loopNotesIncoming[SONGSIZE * NUMPLAYERS] = '\0';
  localLoopString[SONGSIZE * NUMPLAYERS] = '\0';


  //set up the music
  for (int i = 0; i < NUMPLAYERS; i++) {
    newNoteFlag[i] = 1;

  }

  upButtonState = 0;
  recButtonState = 0;

}

void loop() {

  // This part ensures that you ESP is connected to your broker
  if (!client.connected()) {
    reconnect();
  }

  client.loop();// connect to the MQTT server

  //show cound down ones when after reset
  if (startFlag) {
    countDown();
    startFlag = false;
  }

  //copy the arrays (from the broker to the played loop)
  if (doneLoop) { //if the loop ended
    copyArrays(loopNotesChar, loopNotesIncoming, SONGSIZE * NUMPLAYERS);
    if (!(recButtonState && playerTurn == PLAYER)) {
      copyArrays(localLoopString, loopNotesIncoming, SONGSIZE * NUMPLAYERS);
    }
    doneLoop = false; //change back the flag

  }

  PlayAllNotes(); //play all the notes that are currently running

  //play the loop
  if (millis() - lastPlayTime > bitInterval) {
    PlayLocalLoop();
    playSharedLoop();
    lastPlayTime = millis(); //reset the time
    noteInedx = (noteInedx + 1) % SONGSIZE; // advance the index

    checkButtons();

    //check if the upload button was pressed
    if (upButtonState && playerTurn == PLAYER) {
      Serial.println("uplaod");

      // reset the buttons states
      upButtonState = 0;
      recButtonState = 0;

      //change the local player's turn
      playerTurn = (playerTurn + 1) % NUMPLAYERS;

      doneRec = false; //reset flag

      client.publish(topic, localLoopString);
      Serial.print("sent loop is  ");
      Serial.println(localLoopString);
    }
  }
  yield();

}


//////////////////////////////////
//Play the song from the array///
/////////////////////////////////
void playSharedLoop() {
  //show the LED strip and Matrix
  int scale = map(noteInedx, 0, 64, 1, 28);
  ShowLEDStrip(scale);
  ShowLEDMatix(noteInedx);

  //iterate for every player
  for (int i = 0; i < NUMPLAYERS; i++) {
    //dont play the players note here (we play them in the local loop)
    if (i == PLAYER) {
      continue;
    }
    loopNewNote[i] = loopNotesChar[noteInedx + (i * SONGSIZE)] - '0'; // set the next note to play

    // check if the note has changed
    if (loopNewNote[i] != loopOldNote[i]) {

      stopNote(loopOldNote[i] + (i * NUMKEYS), i); //stop the old note
      loopOldNote[i] = loopNewNote[i];
      if (loopNewNote[i] != 0) { // if the note is zero - dont play
        playNote(loopNewNote[i] + (i * NUMKEYS), i); // play the note
      }
    }
  }

  lastPlayTime = millis(); // reset time
  if (noteInedx == SONGSIZE - 1) {
    doneLoop = true; // set flag
  }
}



//////////////////////////////////
//Record the new Loop///
/////////////////////////////////
void PlayLocalLoop() {
  //play the notes to the the loop
  localNewNote = analogButtons();

  //save non-zero notes
  if (!localNewNote) {
    localNewNote = localLoopString[noteInedx + (PLAYER * SONGSIZE)] - '0';
  }

  // if there is new note to play
  if (localOldNote != localNewNote) {
    localOldNote = localNewNote;
    stopNote(localNewNote + (PLAYER * NUMKEYS), PLAYER);

    //play non-zero notes
    if (localNewNote  != 0) {
      playNote(localNewNote + (PLAYER * NUMKEYS), PLAYER);
    }
  }
  //check if to record
  if ((playerTurn == PLAYER) && recButtonState) {
    localLoopString[noteInedx + (PLAYER * SONGSIZE)] = notesChar[localNewNote];


    //check if done with the loop
    if (noteInedx == SONGSIZE - 1) {
      doneRec = true; //set flag
    }
  }
}

//////////////////////////////////
//Play All Notesp///
/////////////////////////////////
void PlayAllNotes() {
  for (int i = 0; i < NUMPLAYERS; i++) {
    if (wav[i]->isRunning()) {
      wav[i]->loop();
    }
  }
}

//////////////////////////////////
//Stop All Notesp///
/////////////////////////////////
void stopAllNotes(int player) {
  for (int i = 0; i < NUMofNotes; i++) {
    stopNote(i, player);
  }
}

//////////////////////////////////
//Play a single Note///
/////////////////////////////////
//play the note for the given player
void playNote(int note, int player) {
  Serial.println(note);
  if (newNoteFlag[player]) { //check if note needs to begin
    file[player] = new AudioFileSourceSPIFFS(paths[note]);
    wav[player]->begin(file[player], stub[player]);
    newNoteFlag[player] = 0;
  }
  //play the note
  if (wav[player]->isRunning()) {
    if (!wav[player]->loop()) {
      stopNote(note, player);
    }
  }
}

//////////////////////////////////
//Play a Single Note///
/////////////////////////////////
void stopNote(int note, int player) {
  if (wav[player]->isRunning()) { // check if the note is playing
    //stop the playing functions
    wav[player]->stop();
    stub[player]->stop();
    file[player]->close();
    newNoteFlag[player] = 1;//seting the flag back to new note

  } else {
    newNoteFlag[player] = 1;//seting the flag back to new note

  }
}

//////////////////////////////////
//copy arrays///
/////////////////////////////////
void copyArrays(char arry1[], char arry2[], unsigned int length) {
  for (int i = 0 ; i < length; i++) {
    arry1[i] = arry2[i];
  }
}

///////////////////////////////////////////
//checkButtons                          //
// check the record and uplaod Buttons///
////////////////////////////////////////
void  checkButtons() {
  if ((recButtonState == 0) && playerTurn == PLAYER) {
    recButtonState = !digitalRead(recButton);
    if (recButtonState) {
      Serial.println("Record Butoon pressd");
    }
  }
  if (upButtonState == 0 ) {
    upButtonState = !digitalRead(upButton);
    if (upButtonState) {
      Serial.println("Upload Butoon pressd");
    }
  }
}

///////////////////////////////////////////
//analogButtons                         //
// check the which buttons was pressed///
////////////////////////////////////////
int  analogButtons() {
  buttonValue = analogRead(A0); //Read analog value from A0 pin

  //For 1st button:
  if (buttonValue >= 0 && buttonValue <= 150) {
    return 1;
    Serial.println("1");
  }
  //For 2nd button:
  else if (buttonValue >= 151 && buttonValue <= 350) {
    return 2;
    Serial.println("2");
  }
  //For 3rd button:
  else if (buttonValue >= 351  && buttonValue <= 470) {
    return 3;
    Serial.println("3");
  }
  //For 4th button:
  else if (buttonValue >= 471  && buttonValue <= 550) {
    return 4;
    Serial.println("4");
  }
  //For 5th button:
  else if (buttonValue >= 551  && buttonValue <= 700) {
    Serial.println("5");
    return 5;
  }
  //No button pressed, turn off LEDs
  else {
    return 0;
  }
}


///////////////////////////////////////////
//ShowLEDStrip                          //
//Shows the advance in the LED strip   ///
///////////////////////// ///////////////
void ShowLEDStrip(int pos) {
  //clear the screen
  for (int i = 0; i <= strip.numPixels(); i += 1) {
    strip.setPixelColor(i, 0);
  }

  //advance the light from 1 to the pos
  for (int i = 1; i <= pos; i += 1) {

    //if it's the player's turn change the color
    if (playerTurn == PLAYER) {
      strip.setPixelColor(i, colors[0]);
    } else {
      strip.setPixelColor(i, STRIPCOLOR);
    }
  }

  //if the player is recording change the last light to be red
  if (recButtonState) {
    strip.setPixelColor(pos, RED);
  }
  strip.show();
}

///////////////////////////////////////////
//ShowLEDMatix                          //
//Shows the advance in the LED matrix ///
///////////////////////// //////////////
void ShowLEDMatix(int start) {
  matrix.clear();

  //play after 2 indexes
  if (start > 1) {

    //iterate for each row
    for (int i = 1; i <= 8; i++) {

      //iterate over each player
      for (int j = 0; j < NUMPLAYERS; j++) {

        // get the note from the loop and map it to to 0-2
        int k = map(localLoopString[(start + i - 4) % SONGSIZE] - '0', 1, 5, 1, 2);
        map(noteInedx, 0, 64, 1, 28);

        // for non zero notes show in matrix
        if (localLoopString[(j * SONGSIZE) + (start + i - 4) % SONGSIZE] - '0' > 0) {
          for (int t = 1; t <= k; t++) {
            matrix.setPixelColor(getPos(i, (2 * j) + 3 - t), colors[j]);

            //set the yellow line in the 4th collum
            if (i == 4) {
              matrix.setPixelColor(getPos(i, (2 * j) + 3 - t), YELLOW);
            }

          }
        }
      }
    }
    matrix.show();
  }
}


///////////////////////////////////////////
//countDown                             //
//count down 3..2..1 until start      ///
///////////////////////// //////////////
void countDown() {
  char digits[3][8][8] = {{
      {'E', 'E', 'B', 'B', 'B', 'E', 'E', 'E'},
      {'E', 'B', 'E', 'E', 'E', 'B', 'E', 'E'},
      {'E', 'E', 'E', 'E', 'E', 'B', 'E', 'E'},
      {'E', 'E', 'E', 'B', 'B', 'E', 'E', 'E'},
      {'E', 'E', 'E', 'E', 'E', 'B', 'E', 'E'},
      {'E', 'B', 'E', 'E', 'E', 'B', 'E', 'E'},
      {'E', 'E', 'B', 'B', 'B', 'E', 'E', 'E'},
      {'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E'}
    }, {
      {'E', 'E', 'B', 'B', 'B', 'E', 'E', 'E'},
      {'E', 'B', 'E', 'E', 'E', 'B', 'E', 'E'},
      {'E', 'E', 'E', 'E', 'E', 'B', 'E', 'E'},
      {'E', 'E', 'E', 'E', 'B', 'E', 'E', 'E'},
      {'E', 'E', 'E', 'B', 'E', 'E', 'E', 'E'},
      {'E', 'E', 'B', 'E', 'E', 'E', 'E', 'E'},
      {'E', 'B', 'B', 'B', 'B', 'B', 'E', 'E'},
      {'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E'}
    }, {
      {'E', 'E', 'B', 'B', 'E', 'E', 'E', 'E'},
      {'E', 'E', 'E', 'B', 'E', 'E', 'E', 'E'},
      {'E', 'E', 'E', 'B', 'E', 'E', 'E', 'E'},
      {'E', 'E', 'E', 'B', 'E', 'E', 'E', 'E'},
      {'E', 'E', 'E', 'B', 'E', 'E', 'E', 'E'},
      {'E', 'E', 'E', 'B', 'E', 'E', 'E', 'E'},
      {'E', 'E', 'B', 'B', 'B', 'E', 'E', 'E'},
      {'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E'}
    }
  };
  for (int k = 0; k < 3; k++) {
    for (int i = 0; i < 8; i++) {
      for (int j = 0; j < 8; j++) {
        if (digits[k][j][i] == 'B') {
          switch (k) {
            case 0: matrix.setPixelColor(getPos(i, j), RED); break;
            case 1: matrix.setPixelColor(getPos(i, j), YELLOW); break;
            case 2: matrix.setPixelColor(getPos(i, j), GREEN); break;
          }
        } else {
          matrix.setPixelColor(getPos(i, j), 0);
        }
      }
    }
    for (int i = 0; i <= strip.numPixels(); i += 1) {
      switch (k) {
        case 0: strip.setPixelColor(i, RED); break;
        case 1: strip.setPixelColor(i, YELLOW); break;
        case 2: strip.setPixelColor(i, GREEN); break;
      }
    }
    strip.show();
    matrix.show();

    delay(850);
    for (uint16_t i = 0; i < matrix.numPixels(); i++) {
      matrix.setPixelColor(i, 0);
    }
    for (int i = 0; i < strip.numPixels(); i += 1) {
      strip.setPixelColor(i, 0);
    }
    matrix.show();
    delay(150);

  }
}


//////////////////////////////////////////////
//getPos                                   //
//convert from X Y axies to 1 to 64 index///
///////////////////////////////////////////
int getPos(int x, int y) {
  return y + (7 - x) * 8;
}
