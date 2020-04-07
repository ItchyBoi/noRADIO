// "bookmark" for later features

#include <VS1053.h>  //https://github.com/baldram/ESP_VS1053_Library
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_wifi.h>
#include <EEPROM.h>
#include <ESP32Encoder.h>

#define VS1053_CS    32 
#define VS1053_DCS   33  
#define VS1053_DREQ  35 

// EEPROM data (int = 4 bytes)
// 0 radiostation (value 0-9)
// 1 volume (value 0-100)
// 100-199 wifi ssid //bookmark: save login in eeprom and encrypt it
// 200-299 wifi pass
#define EEPROM_SIZE 512 // 0-255Bit savable in one adress = 1 byte
                        // int = 4 bytes but if int is smaller then 255 it just takes one adress
                        // char = 1 byte // be aware of \0 !
#define radiostationaddrstart 0
#define volumeaddrstart 1
#define ssidaddrstart 100
#define passaddrstart 200

#define onoffbutton 15
#define onoffsignal 21
#define knockAIN 34 // ADC CH1 cant be used while wifi is active

int fiftymill = 50; 
int thirtysec = 30000;
int onesec = 1000;
unsigned long millcounter = 0;
unsigned long previousmillis = 0;

int radiostation = 1;
int previousradiostation = -1;

int volume = -1;
bool active = true;

char ssid[] = "WIFIblock";
char pass[] = "F1delistH0me";

// Few Radio Stations // no https!! // get some from http://www.radio-browser.info
//  egofm 128kbps
//  fm4 192kbps
char *host[4] = {"egofm-ais-edge-3006.fra-eco.cdn.addradio.net","185.85.29.144","",""};
char *path[4] = {"/egofm/live/mp3/high/stream.mp3","/","",""};
int   port[4] = {80,8000,80,80};

int status = WL_IDLE_STATUS;
WiFiClient  client;
uint8_t mp3buff[32];
VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);
ESP32Encoder volonoff;

void play_buffer();
void time_routine();
void initMP3Decoder();
void connectToWIFI();
int readEEPROM(String name, int startaddr);
void writeEEPROM(String name, int val, int startaddr);
void station_connect(int station_no);
void volumecontrol();
void stationchange();
void savedata();
void readEEPROM();
void writeEEPROM();


/////////////// SETUP ///////////////

void setup () {
  SPI.begin();
  EEPROM.begin(EEPROM_SIZE);
  Serial.begin(115200);
  delay(500);
  Serial.println("happy beep");

  connectToWIFI();

  ESP32Encoder::useInternalWeakPullResistors=true;
  volonoff.attachHalfQuad(16,17);
  volonoff.setCount(readEEPROM("volume",volumeaddrstart));

  radiostation = readEEPROM("station",radiostationaddrstart);
  initMP3Decoder();
  stationchange();
  
  Serial.println(sizeof(int));
  Serial.println(sizeof(volume));
  Serial.println(sizeof(ssid));
}

/////////////// MAINLOOP ///////////////

void loop() {
    
  play_buffer();
  time_routine();

  //int sensor = analogRead(knockAIN);

}

void volumecontrol(){
  int tmpvol = volonoff.getCount();
  if(volume == tmpvol)return;

  if(tmpvol >= 100){
    tmpvol = 100;
    volonoff.setCount(100);
  } 
  if(tmpvol <= 0){
    tmpvol = 0;
    volonoff.setCount(0);
  } 
  
    player.setVolume(tmpvol);
    Serial.println("volume:"+String(tmpvol));
    volume = tmpvol;
}

void time_routine(){

  unsigned long currentmillis = millis();
  if(currentmillis - previousmillis > fiftymill){

    volumecontrol();
    onesec = onesec - 50;
    if(onesec <= 0){

      onesec = 1000;
      Serial.println("radiostation: "+String(radiostation)+" MEM buffer: "+client.available());
      stationchange();

      thirtysec = thirtysec - 1000;
      if(thirtysec <= 0){

        thirtysec = 30000;
        savedata();
      }
    }
  previousmillis = currentmillis;
  }
}

void stationchange(){
  if(radiostation!=previousradiostation){

        station_connect(radiostation);
        previousradiostation = radiostation;
}
}
void savedata(){
  int readStation = readEEPROM("station", radiostationaddrstart);
          if(readStation!=radiostation){

            Serial.println("Saving new station to EEPROM");
            writeEEPROM("station",radiostation, radiostationaddrstart);
          }
  int readvolume = readEEPROM("volume", volumeaddrstart);
          if(readvolume!=volume){

            Serial.println("Saving new volume to EEPROM");
            writeEEPROM("volume",volume, volumeaddrstart);
          }
}
          
void play_buffer(){
        if (client.available() > 0)
      {
        uint8_t bytesread = client.read(mp3buff, 32);
        player.playChunk(mp3buff, bytesread);
      }  
}

void station_connect (int station_no ) {
    if (client.connect(host[station_no],port[station_no]) ) Serial.println("Connected now"); 
    client.print(String("GET ") + path[station_no] + " HTTP/1.1\r\n" +
               "Host: " + host[station_no] + "\r\n" + 
               "Connection: close\r\n\r\n");         
  }

void initMP3Decoder()
  {
    player.begin();
    player.switchToMp3Mode();
    volumecontrol();
  }

  void connectToWIFI()
  {
    Serial.print("connecting wifi..");
    WiFi.begin(ssid, pass);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
   }
   Serial.println("done");
  }


  int readEEPROM(String name, int startaddr){

    int val;
    val = EEPROM.read(startaddr);    
    Serial.println("readEEPROM("+name+"): "+String(val));
    return val;
  }

  void writeEEPROM(String name, int val, int startaddr){
  
    EEPROM.write(startaddr, val);
    EEPROM.commit();
    Serial.println("writeEEPROM("+String(name)+"): "+String(val));
  }
