// "bookmark" for later features

#include <VS1053.h>  //https://github.com/baldram/ESP_VS1053_Library
#include <WiFi.h>
#include <HTTPClient.h>
//#include <esp_wifi.h>
#include <EEPROM.h>
#include <ESP32Encoder.h>

#define VS1053_CS    32 
#define VS1053_DCS   33  
#define VS1053_DREQ  35 

// EEPROM data (int = 4 bytes) maybe writeint(); bookmark
// 0 active (true or false) // not in use
// 1 radiostation (value 0-9)
// 2 volume (value 0-100)
// 100-199 wifi ssid //bookmark: save login in eeprom and encrypt it
// 200-299 wifi pass
#define EEPROM_SIZE 512 // 0-255Bit savable in one adress = 1 byte
                        // int = 4 bytes but if int is smaller then 255 it just takes one adress
                        // char = 1 byte // be aware of \0 !
#define activeaddr 0 // not in use
#define radiostationaddr 1
#define volumeaddr 2
#define ssidaddrstart 100
#define passaddrstart 200

#define onoffbutton 15
#define onoffsignal 21
#define knockAIN 34 // ADC CH1 cant be used while wifi is active

int lastonoffbutton = 1;

const int threshold = 400;
unsigned long firstknock = 0;
unsigned long secknock = 0;

int fiftymill = 50; 
int thirtysec = 30000;
int onesec = 1000;
unsigned long millcounter = 0;
unsigned long lastmillis = 0;

int numstations = 1; 
int radiostation = 0;
int lastradiostation = -1;

int volume = -1;
bool active = HIGH;

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
void init_MP3Decoder();
void connect_To_WIFI();
int read_EEPROM(String name, int startaddr);
void write_EEPROM(String name, int val, int startaddr);
void station_connect(int station_no);
void volume_control();
void station_change();
void save_data();
void read_EEPROM();
void write_EEPROM();
void onoff_routine();
void knock_knock();


/////////////// SETUP ///////////////

void setup () {
  pinMode(onoffsignal,OUTPUT);

  SPI.begin();
  EEPROM.begin(EEPROM_SIZE);
  Serial.begin(115200);
  delay(500);
  Serial.println("happy beep");

  connect_To_WIFI();

  ESP32Encoder::useInternalWeakPullResistors=true;
  volonoff.attachHalfQuad(16,17);
  volonoff.setCount(read_EEPROM("volume",volumeaddr));

  radiostation = read_EEPROM("station",radiostationaddr);
  init_MP3Decoder();
  station_change();
}

/////////////// MAINLOOP ///////////////

void loop() {
    
  play_buffer();
  time_routine();
  knock_knock();
  //int sensor = analogRead(knockAIN);

}

void knock_knock(){
  int sens = analogRead(knockAIN);
  
  if(millis() - firstknock <= 100)return;

  if(firstknock != 0 && secknock != 0){
    if(secknock - firstknock <= 400 && secknock - firstknock >= 100){
      radiostation++;
    }
    firstknock = 0;
    secknock = 0;
    return;
  }
  if(sens >= threshold){
    if (firstknock == 0){
      firstknock = millis();
      Serial.print("first:");
      Serial.println(firstknock);
      return;
    }
    secknock = millis();
    Serial.print("sec:");
    Serial.println(secknock);
  }
}

void onoff_routine(){
  int tmponoffbutton = digitalRead(onoffbutton);
  if(tmponoffbutton == lastonoffbutton)return;

  if(tmponoffbutton == 1 && lastonoffbutton == 0){
    active =! active;
    digitalWrite(onoffsignal,active);
    Serial.println("Radioactive:"+String(active)); // debug

    if(active == LOW){
      client.stop();
      WiFi.disconnect();
      SPI.end();
    }else ESP.restart();
  }
  lastonoffbutton = tmponoffbutton;
}

void volume_control(){
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
  if(currentmillis - lastmillis > fiftymill){

    onoff_routine();
    volume_control();
    onesec = onesec - 50;
    if(onesec <= 0){

      onesec = 1000;
      Serial.println("radiostation: "+String(radiostation)+" MEM buffer: "+client.available());
      station_change();

      thirtysec = thirtysec - 1000;
      if(thirtysec <= 0){

        thirtysec = 30000;
        save_data();
      }
    }
  lastmillis = currentmillis;
  }
}

void station_change(){
  if(radiostation > numstations)radiostation = 0;

  if(radiostation!=lastradiostation){

        station_connect(radiostation);
        lastradiostation = radiostation;
}
}
void save_data(){
  int readStation = read_EEPROM("station", radiostationaddr);
          if(readStation!=radiostation){

            Serial.println("Saving new station to EEPROM");
            write_EEPROM("station",radiostation, radiostationaddr);
          }
  int readvolume = read_EEPROM("volume", volumeaddr);
          if(readvolume!=volume){

            Serial.println("Saving new volume to EEPROM");
            write_EEPROM("volume",volume, volumeaddr);
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

void init_MP3Decoder()
  {
    player.begin();
    player.switchToMp3Mode();
    volume_control();
  }

  void connect_To_WIFI()
  {
    Serial.print("connecting wifi..");
    WiFi.begin(ssid, pass);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
   }
   Serial.println("done");
  }


  int read_EEPROM(String name, int startaddr){

    int val;
    val = EEPROM.read(startaddr);    
    Serial.println("readEEPROM("+name+"): "+String(val));
    return val;
  }

  void write_EEPROM(String name, int val, int startaddr){
  
    EEPROM.write(startaddr, val);
    EEPROM.commit();
    Serial.println("writeEEPROM("+String(name)+"): "+String(val));
  }
