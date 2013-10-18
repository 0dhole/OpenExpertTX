#include "config.h"
#include <EEPROM.h>
#include <avr/wdt.h>

/****************************************************
 * Open Tiny transmitter code
 ****************************************************/
byte RF_channel = 0;
byte FSstate = 0; // 1 = waiting timer, 2 = send FS, 3 sent waiting BUTTON release

unsigned long FStime = 0;  // time when button went down...
unsigned long lastSent = 0;


void checkFS(void)
{

  switch (FSstate) {
  case 0:
    if (!digitalRead(BUTTON)) {
      FSstate = 1;
      FStime = millis();
    }

    break;

  case 1:
    if (!digitalRead(BUTTON)) {
      if ((millis() - FStime) > 1000) {
        FSstate = 2;
//        buzzerOn(BZ_FREQ);
      }
    } else {
      FSstate = 0;
    }

    break;

  case 2:
    if (digitalRead(BUTTON)) {
//      buzzerOff();
      FSstate = 0;
    }

    break;
  }
}

extern volatile word PPM[RC_CHANNEL_COUNT];     // текущие длительности канальных импульсов
extern volatile byte ppmAge; // age of PPM data

void setup(void)
{
//  setupSPI();
   pinMode(SDO_pin, INPUT); //SDO
   pinMode(SDI_pin, OUTPUT); //SDI        
   pinMode(SCLK_pin, OUTPUT); //SCLK
   pinMode(IRQ_pin, INPUT); //IRQ
   pinMode(nSel_pin, OUTPUT); //nSEL
     
   pinMode(0, INPUT); // Serial Rx
   pinMode(1, OUTPUT);// Serial Tx


  //LED and other interfaces
  pinMode(RED_LED_pin, OUTPUT);   //RED LED
  pinMode(GREEN_LED_pin, OUTPUT);   //GREEN LED
  pinMode(BUTTON, INPUT);   //Buton

  pinMode(PPM_IN, INPUT);   //PPM from TX
  digitalWrite(PPM_IN, HIGH); // enable pullup for TX:s with open collector output

//  buzzerInit();

  Serial.begin(SERIAL_BAUD_RATE);

  setupPPMinput();
  attachInterrupt(IRQ_interrupt,RFM22B_Int,FALLING);

  RF22B_init_parameter();

  sei();

//  buzzerOn(BZ_FREQ);
//  digitalWrite(BUTTON, HIGH);
  Red_LED_ON;
  delay(100);
  for(byte i=0; i<RC_CHANNEL_COUNT; i++) PPM[i]=0;

  Red_LED_OFF;
//  buzzerOff();
  ppmAge = 255;
  rx_reset();
}

void loop(void)
{
  byte i,j,k,sendCntr=0;
  word pwm;
  unsigned long time = micros();

  Serial.println("\nBaychi soft 2013");
  Serial.print("TX Open Tiny V2 F"); Serial.println(version[0]);
//  eeprom_check(); 
  getTemper();
  Serial.print("T="); Serial.println((int)curTemperature);
  wdt_enable(WDTO_1S);     // запускаем сторожевой таймер 

  while(1) {
    ppmLoop();
    wdt_reset();               //  поддержка сторожевого таймера
    if(checkMenu()) doMenu(); 
    
    if (_spi_read(0x0C) == 0) {     // detect the locked module and reboot
      Serial.println("RFM locked?");
      Red_LED_ON;
      RF22B_init_parameter();
      rx_reset();
      Red_LED_OFF;
    }

    time = micros();

    if(ppmAge < 5 && (time - lastSent) >= 31500) {

      lastSent=time;
      if(++sendCntr > (8-lastPower)*3) sendCntr=0; // мигаем с частотой пропорциональной мощности
      if(sendCntr == 0) Green_LED_ON;
      Hopping();
      to_tx_mode();                           // формируем и посылаем пакет
      getTemper();                            // меряем темперартуру
      Green_LED_OFF;
      ppmAge++;

//-------------------------------------------------------------
      for(i=0; i<RC_CHANNEL_COUNT; i++) {
        Serial.print(PPM[i]);
        ppmLoop();
        Serial.print(" ");
      }
      Serial.print(" L=");  Serial.print(maxDif);
      ppmLoop();
      Serial.print(" T=");  Serial.println((int)curTemperature);
      
     } else if(ppmAge == 255) {
       getTemper();                            // меряем темперартуру
       Serial.print("Waiting Start");
       Serial.print(" L="); Serial.print(maxDif);
       Serial.print(" T="); Serial.println((int)curTemperature);
       Sleep(299);
     } else if(ppmAge > 5) {
       to_sleep_mode();                      // нет PPM - нет и передачи
       getTemper();                            // меряем темперартуру
       Serial.println("Waiting PPM");
       Sleep(299);
     }
  }  
  
}