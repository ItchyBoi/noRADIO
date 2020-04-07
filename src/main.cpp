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
#define onoffsignal 21
#define knockAIN 34 // ADC CH1 cant be used while wifi is active

long interval = 1000; 
int SECONDS_TO_AUTOSAVE = 30;
long seconds = 0;
long previousMillis = 0;

int radioStation = 0;
int previousRadioStation = -1;

int volume = -1;
bool active = true;

int isr_timer = 0;


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
void isr_onoff();





/////////////// SETUP ///////////////

void setup () {

  Serial.begin(115200);
  delay(500);
  Serial.println("happy beep");

  ESP32Encoder::useInternalWeakPullResistors=true;
  volonoff.attachHalfQuad(16,17);
  volonoff.setCount(100);

  attachInterrupt(digitalPinToInterrupt(onoffbutton), isr_onoff, RISING);

  SPI.begin();
  EEPROM.begin(EEPROM_SIZE);
  initMP3Decoder();
  connectToWIFI();
  //readStationFromEEPROM();

  pinMode(onoffsignal, OUTPUT);
}

/////////////// MAINLOOP ///////////////

void loop() {
    
  play_buffer();
  radioroutine();

  //int sensor = analogRead(knockAIN);

}


void isr_onoff(){
    int tmp_isr_timer = millis();
    if (tmp_isr_timer - isr_timer >= 1000){
      active = !active;
      Serial.println(active);
      digitalWrite(onoffsignal,active);
      isr_timer = millis();
    }
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

void radioroutine(){

  volumecontrol();

  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis > interval) 
    {
    if(radioStation!=previousRadioStation)
    {
      station_connect(radioStation);
      previousRadioStation = radioStation;
      seconds = 0;
    }else
    {
      seconds++;
      if(seconds == SECONDS_TO_AUTOSAVE)
      {
        seconds = 0;
        int readStation = readStationFromEEPROM();
        if(readStation!=radioStation)
          {
            Serial.println("Saving new station to EEPROM");
            writeStationToEEPROM(&radioStation);
          }
      }
    }
    previousMillis = currentMillis; 
    Serial.println("radiostation: "+String(radioStation)+" MEM buffer: "+client.available());
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
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
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