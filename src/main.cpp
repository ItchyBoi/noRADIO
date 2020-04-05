#include <VS1053.h>  //https://github.com/baldram/ESP_VS1053_Library
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_wifi.h>
#include <EEPROM.h>
#include <ESP32Encoder.h>

#define VS1053_CS    32 
#define VS1053_DCS   33  
#define VS1053_DREQ  35 

#define EEPROM_SIZE 2

#define onoffbutton 15
#define onoffpin 21
#define knockAIN 34 // ADC CH1 cant be used while wifi is active


const int fiftymill = 50;
const int one_s = 1000;
const int thirty_s = 30000;

int radioStation = 0;
int previousRadioStation = -1;

int volume = -1;
bool active = true;
bool prev_onoffbutton = true;

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
void radioroutine();
void initMP3Decoder();
void connectToWIFI();
int readStationFromEEPROM();
void writeStationToEEPROM(int *freq);
void station_connect(int station_no);
void volumecontrol();
void onoff();
void timing_routine();





/////////////// SETUP ///////////////

void setup () {

  Serial.begin(115200);
  delay(500);
  Serial.println("happy beep");
/*
  ESP32Encoder::useInternalWeakPullResistors=true;
  volonoff.attachHalfQuad(16,17);
  volonoff.setCount(100);

  SPI.begin();
  EEPROM.begin(EEPROM_SIZE);
  initMP3Decoder();*/
  connectToWIFI();
  //readStationFromEEPROM();

  //pinMode(onoffpin, OUTPUT);
}

/////////////// MAINLOOP ///////////////

void loop() {
    
  //play_buffer();
  //timing_routine();
  //int sensor = analogRead(knockAIN);

}


void onoff(){
  bool tmp_onoffbutton = digitalRead(onoffbutton);
  if (tmp_onoffbutton == true && prev_onoffbutton == false){
    active = !active;
    Serial.println(active);
    digitalWrite(onoffpin,active);
  }
  prev_onoffbutton = tmp_onoffbutton;
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
  
  if(volume != tmpvol){
    player.setVolume(tmpvol);
    Serial.println("volume:"+String(tmpvol));
    volume = tmpvol;
  }
}

void timing_routine(){
  unsigned long currentMillis = millis();

  if(currentMillis % fiftymill == 0){ // every 50 mill

    onoff();
    volumecontrol();

    if(radioStation!=previousRadioStation)
    {
      station_connect(radioStation);
      previousRadioStation = radioStation;
    }


    if(currentMillis % one_s == 0){ // every second
      
      Serial.println("radiostation: "+String(radioStation)+" MEM buffer: "+client.available());

      if(currentMillis % thirty_s == 0){ // every 30 seconds

        if(readStationFromEEPROM()!=radioStation)
          {
            Serial.println("Saving new station to EEPROM");
            writeStationToEEPROM(&radioStation);
          }


      }
    }
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
    player.switchToMp3Mode(); // optional, some boards require this
    volumecontrol();
  }

  void connectToWIFI()
  {
    WiFi.begin(ssid, pass);
      if (WiFi.status() != WL_CONNECTED) {
        Serial.print("can't connect to wifi");
        delay(2000);
        ESP.restart();
   }
  }

  int readStationFromEEPROM()
  {
  int station;
  byte ByteArray[2];
  for(int x = 0; x < 2; x++)
  {
   ByteArray[x] = EEPROM.read(x);    
  }
  memcpy(&station, ByteArray, 2);
  Serial.println("readFrequencyFromEEPROM(): "+String(station));
  return station;
}

void writeStationToEEPROM(int *freq)
{
 byte ByteArray[2];
 memcpy(ByteArray, freq, 2);
 for(int x = 0; x < 2; x++)
 {
   EEPROM.write(x, ByteArray[x]);
 }  
 EEPROM.commit();
 Serial.println("writeFrequencyToEEPROM(): "+String(radioStation));
}