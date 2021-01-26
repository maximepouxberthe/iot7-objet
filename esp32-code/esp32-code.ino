// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <Adafruit_VS1053.h>

#define VS1053_RESET   -1     // VS1053 reset pin (not used!)

#define VS1053_CS      32     // MP3CS  - VS1053 chip select pin (output)
#define VS1053_DCS     33     // XDCS  - VS1053 Data/command select pin (output)
#define CARDCS         14     // SD CS  - Card chip select pin
#define VS1053_DREQ    15     // DREQ  - VS1053 Data request, ideally an Interrupt pin

const char* ssid     = "ArduinoWifi";
const char* password = "321321321";
WiFiServer server(80);

const byte led_wifi = 27;
const byte led_on = 13;
uint8_t led_rgb_red = A0;
uint8_t led_rgb_green = A1;
uint8_t led_rgb_blue = A5; 

const boolean invert = false; // set true if common anode, false if common cathode

uint8_t color = 0;          // a value from 0 to 255 representing the hue
uint32_t R, G, B;           // the Red Green and Blue color components
uint8_t brightness = 255;   // 255 is maximum brightness, but can be changed.  Might need 256 for common anode to fully turn off.

int countMusic = 1;         // numéro musique à jouer en première dans la playlist

const double sizeCard = 16000000000;

Adafruit_VS1053_FilePlayer musicPlayer = 
  Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);




void setup() {
  Serial.begin(115200);
  delay(50);

  // Wait for serial port to be opened, remove this line for 'standalone' operation
  while (!Serial) { delay(1); }
  delay(500);
  
  Serial.println("\n\nProjet IOT/SE - Maxime Poux-Berthe et Rémi Da Costa");

  setupLeds();
  setupSound();
  setupSD();
  
  // Comme tout est ok (Leds, Son, SD) on allume la led ON
  digitalWrite(led_on, HIGH); 

  setupWifi();
}

void setupLeds() {
  pinMode(led_wifi, OUTPUT);
  pinMode(led_on, OUTPUT);

  // assign RGB led pins to channels
  ledcAttachPin(led_rgb_red, 1); 
  ledcAttachPin(led_rgb_green, 2);
  ledcAttachPin(led_rgb_blue, 3);

  // 12 kHz PWM, 8-bit resolution
  ledcSetup(1, 12000, 8);
  ledcSetup(2, 12000, 8);
  ledcSetup(3, 12000, 8);

  Serial.println(F("Leds OK!"));
}

void setupSound() {
  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }
  Serial.println(F("VS1053 OK!"));

  // TEST AUDIO POUR VERFIFIER SI CA FONCTIONNE
  musicPlayer.sineTest(0x44, 500); 

  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(10,10);
  
  // If DREQ is on an interrupt pin we can do background
  // audio playing
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
}


void setupSD() {
  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1); 
  }
  Serial.println("SD OK!");
  
  // liste les fichiers sur la carte sd
  printDirectory(SD.open("/"), 0);

  gestionLEDStockageSD();
}


void setupWifi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int count = 0;
  while (WiFi.status() != WL_CONNECTED && count < 8) {
      delay(500);
      count++;
  }

  if(WiFi.status() == WL_CONNECTED) {
    digitalWrite(led_wifi, HIGH); 
    
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    server.begin();
  } else {
    Serial.println("");
    Serial.println("WiFi disconnected");
  }
}


void loop() {
  if(WiFi.status() != WL_CONNECTED) {
    digitalWrite(led_wifi, LOW); 
    setupWifi();
  } else {
    listenForClients();
  }

  gestionLEDStockageSD();
  playMusicsFromPlaylist();

  delay(500);
}


void listenForClients() {
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                                             // if you get a client,
    Serial.println("New Client detected ! ");               // print a message out the serial port
    String currentLine = "";                                // make a String to hold incoming data from the client
    while (client.connected()) {                            // loop while the client's connected
      if (client.available()) {                             // if there's bytes to read from the client,
        char c = client.read();                             // read a byte, then
        Serial.write(c);                                    // print it out the serial monitor
        if (c == '\n') {                                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:
            client.print("Click <a href=\"/music\">here</a>ajoute une musique à la carte sd<br>");
            client.print("Click <a href=\"/storage\">here</a>retourne la capacité restante de la carte sd<br>");

            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        // Check to see if the client request was "GET /H" or "GET /L":
        if (currentLine.endsWith("POST /music")) {
          addMusicToSDCard();
        }
        if (currentLine.endsWith("GET /storage")) {
          getStorageCard();
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }
}



void gestionLEDStockageSD() {
  double capaciteOccupe = calculStockageDirectory(SD.open("/"));
  double capaciteRestante = sizeCard - capaciteOccupe;
  Serial.print(capaciteRestante/1000000);
  Serial.print(" Mo");
  Serial.print(" = ");
  Serial.print(sizeCard/1000000);
  Serial.print(" Mo");
  Serial.print(" - ");
  Serial.print(capaciteOccupe/1000000);
  Serial.println(" Mo");

  RGB_color((capaciteOccupe/1000000)/63, 255 - (capaciteOccupe/1000000)/63, 0);
}

int calculStockageDirectory(File dir) {
  int stockage = 0;
  while(true) {
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       break;
     }
     
     if (entry.isDirectory()) {
       stockage = stockage + calculStockageDirectory(entry);
     } else {
       // files have sizes, directories do not
       stockage = stockage + entry.size();
     }
     entry.close();
   }

   return stockage;
}


void playMusicsFromPlaylist() {
  int count = 0;
  File dir = SD.open("/");
  File entry;
  if (musicPlayer.stopped()) {
     while(count < countMusic + 3) {
       entry =  dir.openNextFile();
       if (!entry) {
         // no more files
         break;
       }

       if(count != (countMusic + 3 - 1)) {
          entry.close();
       }
       
       count++;
    }

    if (entry && !entry.isDirectory()) {
          Serial.println(entry.name());
          musicPlayer.startPlayingFile(entry.name());
          countMusic++;
     }
     entry.close();
     delay(500);
  }
}


void addMusicToSDCard() {
  // digitalWrite(27, HIGH);               // GET /H turns the LED on

  gestionLEDStockageSD();
}


void getStorageCard() {
  // digitalWrite(27, LOW);                // GET /L turns the LED off

  double capaciteOccupe = calculStockageDirectory(SD.open("/"));
  double capaciteRestante = sizeCard - capaciteOccupe;
}


void RGB_color(int red, int green, int blue) {
  ledcWrite(1, red);
  ledcWrite(2, green);
  ledcWrite(3, blue);
}


void hueToRGB(uint8_t hue, uint8_t brightness)
{
    uint16_t scaledHue = (hue * 6);
    uint8_t segment = scaledHue / 256; // segment 0 to 5 around the
                                            // color wheel
    uint16_t segmentOffset =
      scaledHue - (segment * 256); // position within the segment

    uint8_t complement = 0;
    uint16_t prev = (brightness * ( 255 -  segmentOffset)) / 256;
    uint16_t next = (brightness *  segmentOffset) / 256;

    if(invert)
    {
      brightness = 255 - brightness;
      complement = 255;
      prev = 255 - prev;
      next = 255 - next;
    }

    switch(segment ) {
    case 0:      // red
        R = brightness;
        G = next;
        B = complement;
    break;
    case 1:     // yellow
        R = prev;
        G = brightness;
        B = complement;
    break;
    case 2:     // green
        R = complement;
        G = brightness;
        B = next;
    break;
    case 3:    // cyan
        R = complement;
        G = prev;
        B = brightness;
    break;
    case 4:    // blue
        R = next;
        G = complement;
        B = brightness;
    break;
   case 5:      // magenta
    default:
        R = brightness;
        G = complement;
        B = prev;
    break;
    }
}



/// File listing helper
void printDirectory(File dir, int numTabs) {
   while(true) {
     
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       //Serial.println("**nomorefiles**");
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       Serial.print('\t');
     }
     Serial.print(entry.name());
     if (entry.isDirectory()) {
       Serial.println("/");
       printDirectory(entry, numTabs+1);
     } else {
       // files have sizes, directories do not
       Serial.print("\t\t");
       Serial.println(entry.size(), DEC);
     }
     entry.close();
   }
}
