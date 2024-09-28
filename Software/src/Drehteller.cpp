
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <ESP32Servo.h>
#include <Arduino_GFX_Library.h>
#include <U8g2lib.h>
#include <SPI.h>

String version = "0.0.1";

//#define DEBUG
#define READ_PREFERENCES
#define USE_DISPLAY 1               // 0 = no display conected, 1 = display conected
#define USE_INIT_CORR 1             // 0 = no init correctur, 1 = init correctur (can be usefull if the gear is 3D ptinted)
#define CHANGE_MAC_ADDRESS_HM 1     // HaniMandl: 0 = do not change the mac address, 1 = change the mac address
#define CHANGE_MAC_ADDRESS_TT 1     // Turntable: 0 = do not change the mac address, 1 = change the mac address

Preferences preferences;

//ESP Now
#if CHANGE_MAC_ADDRESS_HM == 0
  #include "./Resources/mac.h"
#endif
#if CHANGE_MAC_ADDRESS_HM == 1
  const uint8_t MacAdressHanimandl[] = {0x74, 0x00, 0x00, 0x00, 0x00, 0x01};
#endif
esp_now_peer_info_t peerInfo;

int microsteps = 8;

//Pin NEMA17
int STEP_PIN = 4;
int DIRECTION_PIN = 0;
bool change_rotation = true;

//Servo
#define SERVO_WRITE(n)     servo.write(n)
const int servo_pin = 2;
Servo servo;

//Display
#if USE_DISPLAY == 1
  Arduino_DataBus *bus = new Arduino_HWSPI(17 /* DC */, 5 /* CS */);
  //Arduino_GFX *gfx = new Arduino_ST7789(bus, -1 /* RST */, 3 /* rotation */);
  Arduino_GFX *gfx = new Arduino_ST7789(bus, 14 /* RST */, 3 /* rotation */);
#endif

// Fonts
#if USE_DISPLAY == 1
  #include "./Fonts/Punk_Mono_Bold_120_075.h"           //10 x 7
  #include "./Fonts/Punk_Mono_Bold_160_100.h"           //13 x 9
  #include "./Fonts/Punk_Mono_Bold_200_125.h"           //16 x 12
  #include "./Fonts/Punk_Mono_Bold_240_150.h"           //19 x 14
  #include "./Fonts/Punk_Mono_Bold_320_200.h"           //25 x 18
  #include "./Fonts/Punk_Mono_Bold_600_375.h"           //48 x 36
  #include "./Fonts/Punk_Mono_Thin_120_075.h"           //10 x 7
  #include "./Fonts/Punk_Mono_Thin_160_100.h"           //13 x 9
  #include "./Fonts/Punk_Mono_Thin_240_150.h"           //19 x 14
#endif

//Logos
#if USE_DISPLAY == 1
  #include "./Resources/LogoTFT.h"
#endif

//Colors TFT
#if USE_DISPLAY == 1
  unsigned long  BACKGROUND = 0x0000;
  unsigned long  TEXT = 0xFFFF;
#endif

//Pin Positionsswitch
int SWITCH = 16;

bool stepper_init_done = false;           //has to be true that the turntable works
bool move_jar_done = false;               //Flag to check move jar is done
bool move_pos_done = false;               //Flag to check move pos is done
bool move2pos_running = false;            //Flag if we are in the menu Move Jar
bool move_tp_done = false;                //Flaf if we are in the menu Tropprotection for the angle setting
bool drop_protection = false;             //fase --> drop protction open, true --> drop protection close
bool turntable_init_check = true;         //false --> turntable init check: off, true --> turntable init check on
int step_counter = 0;                     //counter for steps
int step_counter_value = 0;               //steps between two jars
int step_speed = 500;                     //Stepper speed (less is faster)
int step_speed_init = 500;                //Stepper speed to find the right position
int step_speed_run = 500;                 //Stepper speed in the run modus
int step_center_jar = 0;                  //Steps to center the jar on the scale 
int servo_min = 0;                        //min servo angle
int servo_max = 180;                      //max servo angle
int servo_speed = 0;                      //servo speed (lower is faster)
int servo_wait = 0;                       //waittime to open the dropprodection

int counter1 = 0;                         //Temp counter 1
int counter2 = 0;                         //Temp counter 2
int counter3 = 0;                         //Temp counter 3

//Variables
bool stop = false;
bool esp_now_init_success = false;
bool esp_now_msg_recived = false;
bool esp_now_send_error = true;
//bool esp_now_message_processed = false;
char esp_now_msg[] = "";

//ESP NOW
typedef struct messageToBeSent {
    char text[64];
    int value;
} messageToBeSent;

typedef struct receivedMessage {
    char text[64];
    int value;
} receivedMessage;

messageToBeSent myMessageToBeSent; 
receivedMessage myReceivedMessage;

int get_lenght(int i) {
  #if USE_DISPLAY == 1
    int res;
    if (i == -999 or i == -499) {res = 4;}
    else if (i < 10) {res = 1;}
    else if (i < 100) {res = 2;}
    else if (i < 1000) {res = 3;}
    else if (i < 10000) {res = 4;}
    else if (i < 100000) {res = 5;}
    return res;
  #endif
}

#if USE_DISPLAY == 1
  char myReceivedMessageTextOld[64];
  int myReceivedMessageValueOld = -999;
  char myMessageToBeSentTextOld[64];
  int myMessageToBeSentValueOld = -999;
  bool stepper_init_done_old;
  int step_speed_init_old = -999; 
  int step_speed_run_old = -999;
  int step_center_jar_old = -999;
  bool drop_protection_old;
  int servo_min_old = -999; 
  int servo_max_old = -999;
  int servo_speed_old = -999; 
  int servo_wait_old = -999;
  char revived_text_old[64] = "";
  int recived_value_old = -999;
  char text_display_check_recived[64];
  char text_display_check_send[64];
#endif

void update_display() {
  #if USE_DISPLAY == 1
    //recive mesage
    if(strcmp(myReceivedMessage.text, myReceivedMessageTextOld) != 0) {
      gfx->setTextBound(82, 108, 320 - 80, 21);
      gfx->fillRect(82, 86, 320 - 80, 22, BACKGROUND);
      gfx->fillRect(82, 108, 320 - 80, 21, BACKGROUND);
      gfx->setTextColor(RED);
      gfx->setCursor(85, 125);
      gfx->print(myReceivedMessage.text);
      gfx->print(" - ");
      gfx->print(myReceivedMessage.value);
      sprintf(myReceivedMessageTextOld, "%s", myReceivedMessage.text);
      myReceivedMessageValueOld = myReceivedMessage.value;
      sprintf(text_display_check_recived, "%s - %i", myReceivedMessage.text, myReceivedMessage.value);
      gfx->setTextBound(0, 0, 320, 240);
    }
    //send mesage
    if(strcmp(myMessageToBeSent.text, myMessageToBeSentTextOld) != 0) {
      gfx->fillRect(82, 86, 320 - 80, 21, BACKGROUND);
      gfx->fillRect(82, 108, 320 - 80, 21, BACKGROUND);
      gfx->setTextColor(RED);
      gfx->setCursor(85, 105);
      gfx->print(myMessageToBeSent.text);
      gfx->print(" - ");
      gfx->print(myMessageToBeSent.value);
      sprintf(myMessageToBeSentTextOld, "%s", myMessageToBeSent.text);
      myMessageToBeSentValueOld = myMessageToBeSent.value;
      sprintf(text_display_check_send, "%s - %i", myMessageToBeSent.text, myMessageToBeSent.value);
    }
  #endif
}

void update_display_check () {
  #if USE_DISPLAY == 1
    gfx->setTextBound(82, 108, 320 - 80, 21);
    if (strcmp(myReceivedMessage.text, "") == 0) {
      gfx->setTextColor(GREEN);
      gfx->setCursor(85, 125);
      gfx->print(text_display_check_recived);
      sprintf(myReceivedMessageTextOld, "%s", myReceivedMessage.text);
    }
    if (strcmp(myMessageToBeSent.text, "") == 0) {
      gfx->setTextColor(GREEN);
      gfx->setCursor(85, 105);
      gfx->print(text_display_check_send);
      sprintf(myMessageToBeSentTextOld, "%s", myMessageToBeSent.text);
    }
    gfx->setTextBound(0, 0, 320, 240);
  #endif
}

void update_display_init() {
  #if USE_DISPLAY == 1
    if (stepper_init_done != stepper_init_done_old) {
      gfx->setFont(Punk_Mono_Bold_160_100);
      int i = get_lenght(stepper_init_done_old);
      gfx->setCursor(145 - 9 * i, 150);
      gfx->setTextColor(BACKGROUND);
      gfx->print(stepper_init_done_old);
      i = get_lenght(stepper_init_done);
      gfx->setCursor(145 - 9 * i, 150);
      if (stepper_init_done == 1) {gfx->setTextColor(GREEN);}
      else {gfx->setTextColor(RED);}
      gfx->print(stepper_init_done);
      stepper_init_done_old = stepper_init_done;
    }
  #endif
}

void update_display_speed_init() {
  #if USE_DISPLAY == 1
    if (step_speed_init != step_speed_init_old) {
      gfx->setFont(Punk_Mono_Bold_160_100);
      int i = get_lenght(step_speed_init_old);
      gfx->setCursor(145 - 9 * i, 170);
      gfx->setTextColor(BACKGROUND);
      gfx->print(step_speed_init_old);
      i = get_lenght(step_speed_init);
      gfx->setTextColor(TEXT);
      gfx->setCursor(145 - 9 * i, 170);
      gfx->print(step_speed_init);
      step_speed_init_old = step_speed_init;
    }
  #endif
}

void update_display_speed_run() {
  #if USE_DISPLAY == 1
    if (step_speed_run != step_speed_run_old) {
      gfx->setFont(Punk_Mono_Bold_160_100);
      int i = get_lenght(step_speed_run_old);
      gfx->setCursor(315 - 9 * i, 170);
      gfx->setTextColor(BACKGROUND);
      gfx->print(step_speed_run_old);
      i = get_lenght(step_speed_run);
      gfx->setTextColor(TEXT);
      gfx->setCursor(315 - 9 * i, 170);
      gfx->print(step_speed_run);
      step_speed_run_old = step_speed_run;
    }
  #endif
}

void update_display_scenter_jar() {
  #if USE_DISPLAY == 1
    if (step_center_jar != step_center_jar_old) {
      gfx->setFont(Punk_Mono_Bold_160_100);
      int i = get_lenght(step_center_jar_old / 2);
      gfx->setCursor(315 - 9 * i, 150);
      gfx->setTextColor(BACKGROUND);
      gfx->print(step_center_jar_old / 2);
      i = get_lenght(step_center_jar / 2);
      gfx->setTextColor(TEXT);
      gfx->setCursor(315 - 9 * i, 150);
      gfx->print(step_center_jar / 2);
      step_center_jar_old = step_center_jar;
    }
  #endif
}

void update_display_dp_status() {
  #if USE_DISPLAY == 1
    if (drop_protection != drop_protection_old) {
      gfx->setFont(Punk_Mono_Bold_160_100);
      gfx->setTextColor(BACKGROUND);
      if (drop_protection_old == true) {
        gfx->setCursor(145 - 9 * 5, 195);
        gfx->print("close");
      }
      else {
        gfx->setCursor(145 - 9 * 4, 195);
        gfx->print("open");
      }
      gfx->setTextColor(TEXT);
      if (drop_protection == true) {
        gfx->setCursor(145 - 9 * 5, 195);
        gfx->print("close");
      }
      else {
        gfx->setCursor(145 - 9 * 4, 195);
        gfx->print("open");
      }
      drop_protection_old = drop_protection;
    }
  #endif
}

void update_display_dp_speed() {
  #if USE_DISPLAY == 1
    if (servo_speed != servo_speed_old) {
      gfx->setFont(Punk_Mono_Bold_160_100);
      int i = get_lenght(servo_speed_old);
      gfx->setCursor(315 - 9 * i, 195);
      gfx->setTextColor(BACKGROUND);
      gfx->print(servo_speed_old);
      i = get_lenght(servo_speed);
      gfx->setTextColor(TEXT);
      gfx->setCursor(315 - 9 * i, 195);
      gfx->print(servo_speed);
      servo_speed_old = servo_speed;
    }
  #endif
}

void update_display_dp_angle_min() {
  #if USE_DISPLAY == 1
    if (servo_min != servo_min_old) {
      gfx->setFont(Punk_Mono_Bold_160_100);
      int i = get_lenght(servo_min_old);
      gfx->setCursor(145 - 9 * i, 215);
      gfx->setTextColor(BACKGROUND);
      gfx->print(servo_min_old);
      i = get_lenght(servo_min);
      gfx->setTextColor(TEXT);
      gfx->setCursor(145 - 9 * i, 215);
      gfx->print(servo_min);
      servo_min_old = servo_min;
    }
  #endif
}

void update_display_dp_angle_max() {
  #if USE_DISPLAY == 1
    if (servo_max != servo_max_old) {
      gfx->setFont(Punk_Mono_Bold_160_100);
      int i = get_lenght(servo_max_old);
      gfx->setCursor(315 - 9 * i, 215);
      gfx->setTextColor(BACKGROUND);
      gfx->print(servo_max_old);
      i = get_lenght(servo_max);
      gfx->setTextColor(TEXT);
      gfx->setCursor(315 - 9 * i, 215);
      gfx->print(servo_max);
      servo_max_old = servo_max;
    }
  #endif
}

void update_display_dp_close_wait_time() {
  #if USE_DISPLAY == 1
    if (servo_wait != servo_wait_old) {
      gfx->setFont(Punk_Mono_Bold_160_100);
      int i = get_lenght(servo_wait_old);
      gfx->setCursor(315 - 9 * i, 235);
      gfx->setTextColor(BACKGROUND);
      gfx->print(servo_wait_old);
      i = get_lenght(servo_wait);
      gfx->setTextColor(TEXT);
      gfx->setCursor(315 - 9 * i, 235);
      gfx->print(servo_wait);
      servo_wait_old = servo_wait;
    }
  #endif
}

void delESPnowArrays() {
  #ifdef DEBUG
    Serial.println("delESPnowArrays");
    Serial.print("myReceivedMessage: "); Serial.println(myReceivedMessage.text);
    Serial.print("myMessageToBeSent: "); Serial.println(myMessageToBeSent.text);
  #endif
  update_display();
  memset (&myReceivedMessage, 0, sizeof(myReceivedMessage));
  memset (&myMessageToBeSent, 0, sizeof(myMessageToBeSent));
  update_display_check();
  esp_now_msg_recived = false;
}

void sendMessage() {
  esp_err_t result = esp_now_send(MacAdressHanimandl, (uint8_t *) &myMessageToBeSent, sizeof(myMessageToBeSent));
  if (result != ESP_OK) {
    bool esp_now_send_error = true;
    #ifdef DEBUG
      Serial.print("Sending error: Text: "); Serial.print(myMessageToBeSent.text); Serial.print(" - Value: "); Serial.println(myMessageToBeSent.value);
    #endif
  }
  else {
    bool esp_now_send_error = false;
    #ifdef DEBUG
      Serial.print("Sending sucsess: Text: "); Serial.print(myMessageToBeSent.text); Serial.print(" - Value: "); Serial.println(myMessageToBeSent.value);
    #endif
  }
}

void messageSent(const uint8_t *macAddr, esp_now_send_status_t status) {
  #ifdef DEBUG
    Serial.print("Send status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success":"Fail");
  #endif
}

void messageReceived(const uint8_t* macAddr, const uint8_t* incomingData, int len) {
  memset (&myReceivedMessage, 0, sizeof(myReceivedMessage));
  memcpy (&myReceivedMessage, incomingData, sizeof(myReceivedMessage));
  esp_now_msg_recived = true;
  if (strcmp(myReceivedMessage.text, "stop") == 0) {
    esp_now_msg_recived = false;
    stop = true;
    turntable_init_check = true;
  }
  #ifdef DEBUG
    Serial.print("myReceivedMessage.text: Text: "); Serial.print(myReceivedMessage.text); Serial.print(" - Value: "); Serial.println(myReceivedMessage.value);
    Serial.print("esp_now_msg_recived: "); Serial.println(esp_now_msg_recived);
    Serial.print("stepper_init_done: "); Serial.println(stepper_init_done);
  #endif
}

void readPreferences() {
  #ifdef READ_PREFERENCES
    preferences.begin("EEPROM", false);
    step_center_jar  = preferences.getUInt("step_center_jar", 0);
    step_speed_init  = preferences.getUInt("step_speed_init", 0);
    step_speed_run   = preferences.getUInt("step_speed_run", 0);
    servo_min        = preferences.getUInt("servo_min", 0);
    servo_max        = preferences.getUInt("servo_max", 0);
    servo_speed      = preferences.getUInt("servo_speed", 0);
    servo_wait        = preferences.getUInt("servo_wait", 0);
    //Set default if 0
    if (step_speed_init == 0) {step_speed_init = 500;}
    if (step_speed_run == 0) {step_speed_run = 500;}
    if (servo_max == 0) {servo_max = 180;}
   #ifdef DEBUG
      Serial.print("step_center_jar: "); Serial.println(step_center_jar);
      Serial.print("step_speed_init: "); Serial.println(step_speed_init);
      Serial.print("step_speed_run:  "); Serial.println(step_speed_run);
      Serial.print("servo_min:  ");      Serial.println(servo_min);
      Serial.print("servo_max:  ");      Serial.println(servo_max);
      Serial.print("servo_speed:  ");    Serial.println(servo_speed);
      Serial.print("servo_wait:  ");     Serial.println(servo_wait);
    #endif
  #endif
}

//NEMA17
void make_step() {
  digitalWrite(STEP_PIN, !digitalRead(STEP_PIN));
  delayMicroseconds(step_speed);
  step_counter++;
}

void stepper_init() {
  #ifdef DEBUG
    Serial.println("-- Stepper Init");
  #endif
  stepper_init_done = false;
  step_speed = step_speed_init;
  if (change_rotation) {digitalWrite(DIRECTION_PIN, HIGH);}
  else {digitalWrite(DIRECTION_PIN, LOW);}
  //Drehteller in die richtige Position bringen
  for (int i = 0; i < 200; i++) {
    if (stop == false) {make_step();}
  }
  while (digitalRead(SWITCH) == 1 and stop == false) {
    make_step();
  }
  #ifdef DEBUG
    Serial.println("Search position done");
  #endif
  delay(500);
  //Finde Anzahl steps wo es braucht um ein glas zu verschieben
  step_speed = step_speed_run;
  counter1 = counter2 = counter3 = 0;
  for (int i = 0; i < 12; i++) {
    #ifdef DEBUG
      Serial.print("i = "); Serial.println(i);
      Serial.print("counter1 = "); Serial.println(counter1);
      Serial.print("counter2 = "); Serial.println(counter2);
      Serial.print("counter3 = "); Serial.println(counter3);
    #endif
    step_counter = 0;
    for (int j = 0; j < 500; j++) {
      if (stop == false) {make_step();}
    }
    while (digitalRead(SWITCH) == 1 and stop == false) {
      make_step();
    }
    //delay(100);
    if (stop == false) {
      delay(10);
      if (counter1 == 0) {counter1 = step_counter;}
      else if (counter2 == 0) {counter2 = step_counter;}
      else if (counter3 == 0) {counter3 = step_counter; stepper_init_done = true;}
      if (stepper_init_done) {
        int a1 = counter1 - counter2;
        int a2 = counter1 - counter3;
        int a3 = counter2 - counter3;
        #ifdef DEBUG
          Serial.print("i = "); Serial.println(i);
          Serial.print("a1 = "); Serial.println(a1);
          Serial.print("a2 = "); Serial.println(a2);
          Serial.print("a3 = "); Serial.println(a3);
        #endif
        if (abs(a1) <= 200 and abs(a2) <= 200 and abs(a3) <= 200) {
          step_counter_value = (counter1 + counter2 + counter3) / 3;
          stepper_init_done = true;
          i = 12;
          counter1 = counter2 = counter3 = 0;
        }
        else {
          stepper_init_done = false;
          counter1 = counter2 = counter3 = 0;
        }
      }
    }
  }
  //Glas ein viertel vordrehen dann wider zurück
  if (stepper_init_done) {
    step_counter = 0;
    while (step_counter < step_counter_value / 4) {make_step();}
    if (change_rotation) {digitalWrite(DIRECTION_PIN, LOW);}
    else {digitalWrite(DIRECTION_PIN, HIGH);}
    step_speed = step_speed_run*2;
    step_counter = 0;
    while (step_counter < step_counter_value / 4 * 1.5 and digitalRead(SWITCH) == 1) {make_step();}
    delay(500);
    #if USE_INIT_CORR == 1
      if(digitalRead(SWITCH) == 1) {
        step_counter = 0;
        while (step_counter < step_counter_value / 90) {make_step();}  //90: Dreht 1/3° mehr zurück
        Serial.print("fail: ");
        if (digitalRead(SWITCH) == 0) {Serial.println("solved");}
        else {Serial.println("not solved");}
      }
    #endif
    step_speed = step_speed_run;
    if (change_rotation) {digitalWrite(DIRECTION_PIN, LOW);}
    else {digitalWrite(DIRECTION_PIN, HIGH);}
    step_counter = 0;
  }
  counter1 = counter2 = counter3 = 0;
  #ifdef DEBUG
    Serial.print("switch status: "); Serial.println(digitalRead(SWITCH));
    Serial.print("steper init done: "); Serial.println(stepper_init_done);
    Serial.print("step_counter_value: "); Serial.println(step_counter_value);
    Serial.println("-- End Stepper Init");
  #endif
}

void move_jar() {
  #ifdef DEBUG
    Serial.println("Move Jar");
    Serial.print("Step counter Value: "); Serial.println(step_counter_value);
    Serial.print("Stepper init done: "); Serial.println(stepper_init_done);
  #endif
  move_jar_done = false;
  step_counter = 0;
  if (change_rotation) {digitalWrite(DIRECTION_PIN, HIGH);}
  else {digitalWrite(DIRECTION_PIN, LOW);}
  while (step_counter < step_counter_value / 4 and stop == false) {make_step();}
  delay(10);
  while (digitalRead(SWITCH) == 1 and stop == false and step_counter < step_counter_value * 1.1) {make_step();}
  delay(10);
  step_counter = 0;
  while (step_counter <= step_center_jar and stop == false) {make_step();}
  delay(500);
  step_counter = 0;
  if (change_rotation) {digitalWrite(DIRECTION_PIN, LOW);}
  else {digitalWrite(DIRECTION_PIN, HIGH);}
  while ((digitalRead(SWITCH) == 1 and step_counter <= 1.5 * step_center_jar) and stop == false) {make_step();}
  if (stop == false) {
    move_jar_done = true;
  }
  delay(10);
  if (change_rotation) {digitalWrite(DIRECTION_PIN, HIGH);}
  else {digitalWrite(DIRECTION_PIN, LOW);}
  delay(10);
  #ifdef DEBUG
    Serial.print("Stepper init done: "); Serial.println(stepper_init_done);
    Serial.println("End Move Jar");
  #endif
}

void move_pos() {
  move_pos_done = false;
  step_counter = 0;
  if (myReceivedMessage.value > 0) {
    if (change_rotation) {digitalWrite(DIRECTION_PIN, HIGH);}
    else {digitalWrite(DIRECTION_PIN, LOW);}
  }
  else if (myReceivedMessage.value < 0) {
    if (change_rotation) {digitalWrite(DIRECTION_PIN, LOW);}
    else {digitalWrite(DIRECTION_PIN, HIGH);}
  }
  if (myReceivedMessage.value != 0 and (counter1 + myReceivedMessage.value  <= 0.75 * step_counter_value)) {
    counter1 = counter1 + myReceivedMessage.value;
    while (step_counter <= abs(myReceivedMessage.value) and stop == false) {make_step();}
    move_pos_done = true;
  }
}

void open_drop_protection() {
  if (drop_protection == true) {
    //drop_protection = true;
    int servo_act = servo_min;
    if (servo_speed > 0) {
      while (servo_act < servo_max) {
        #ifdef DEBUG
          Serial.print("servo_act: "); Serial.println(servo_act);
          Serial.print("servo_max: "); Serial.println(servo_max);
        #endif
        SERVO_WRITE(servo_act + 5);
        servo_act = servo_act + 5;
        delay(servo_speed);
      }
    }
    else {
      SERVO_WRITE(servo_max);
    }
    //so ne pseudo abfrage über den Servowinkel. Muss noch ein Taster implementiert werden
    if (175 <= servo.read() <= 185) {drop_protection = false;}
    #ifdef DEBUG
      Serial.print("drop_protection: "); Serial.println(drop_protection);
      Serial.print("servo_act: "); Serial.println(servo_act);
      Serial.print("servo_max: "); Serial.println(servo_max);
      Serial.print("servo.read() "); Serial.println(servo.read());
    #endif
  }
  else {
    #ifdef DEBUG
      Serial.println("drop_protection was open");
    #endif
  }
}

void close_drop_protection(int waittime) {
  if (drop_protection == false) {
    //drop_protection = false;
    int servo_act = servo_max;
    //servo_wait
    int wait_millis = millis();
    while (millis() - wait_millis < waittime * 1000) { 
      delay(10);
    }
    if (servo_speed > 0) {
      while (servo_act > servo_min) {
        #ifdef DEBUG
          Serial.print("servo_act: "); Serial.println(servo_act);
          Serial.print("servo_min: "); Serial.println(servo_min);
        #endif
        SERVO_WRITE(servo_act - 5);
        servo_act = servo_act - 5;
        delay(servo_speed);
      }
    }
    else {
      SERVO_WRITE(servo_min);
    }
    if (-5 <= servo.read() <= 5) {drop_protection = true;}
    #ifdef DEBUG
      Serial.print("drop_protection: "); Serial.println(drop_protection);
      Serial.print("servo_act: "); Serial.println(servo_act);
      Serial.print("servo_min: "); Serial.println(servo_min);
    #endif
  }
  else {
    #ifdef DEBUG
      Serial.println("drop_protection was close");
    #endif
  }
}

void print_credits() {
  #if USE_DISPLAY == 1
    int offset = 22;
    gfx->fillScreen(BACKGROUND);
    gfx->setTextColor(TEXT);
    gfx->setFont(Punk_Mono_Bold_240_150);
    gfx->setCursor(10, 1*offset);
    gfx->print("Idee:");
    gfx->setCursor(90, 1*offset);
    gfx->print("R. Rust");
    gfx->setCursor(10, 2*offset+10);
    gfx->print("Code:");
    gfx->setCursor(90, 2*offset+10);
    gfx->print("R. Rust");
  #endif
}

void setup() {
  #if USE_DISPLAY == 1
    gfx->begin();
    gfx->fillScreen(BACKGROUND);
    gfx->setUTF8Print(true);
  #endif
  Serial.begin(115200);
  while (!Serial) {}
  #ifdef DEBUG
    Serial.println("-- Setup loop");
  #endif
  //set default imnputs and read preverences
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIRECTION_PIN, OUTPUT);
  pinMode(SWITCH, INPUT_PULLDOWN);
  readPreferences();
  //Boot scsreen
  #if USE_DISPLAY == 1
    gfx->drawXBitmap(0, 0, LogTFT, 320, 240, YELLOW);
    gfx->setTextColor(YELLOW);
    gfx->setFont(Punk_Mono_Bold_200_125);
    gfx->setCursor(232, 220);
    gfx->print(version);
    delay(500);
    delay(2500);
    print_credits();
    delay(500); 
    delay(2500);
  #endif
  //Servo
  #if USE_DISPLAY == 1
    gfx->fillScreen(BACKGROUND);
    gfx->setTextColor(TEXT);
    gfx->setFont(Punk_Mono_Bold_320_200);
    gfx->setCursor(25, 27);
    gfx->print("Start Turntable");
    gfx->drawLine(0, 30, 320, 30, TEXT);
    gfx->setCursor(10, 60);
    gfx->setFont(Punk_Mono_Bold_200_125);
    gfx->print("Serup Servo:");
  #endif
  servo.attach(servo_pin, 1000, 2000);
  SERVO_WRITE(90);
  delay(500);
  close_drop_protection(0);
  #if USE_DISPLAY == 1
    gfx->setCursor(320 - 2 * 16, 60);
    gfx->print("ok");
    gfx->setCursor(10, 80);
    gfx->print("Setup ESPNow:");
  #endif
  WiFi.mode(WIFI_STA);
  //change mac address
  #if CHANGE_MAC_ADDRESS_TT == 1
    uint8_t newMACAddress[] = {0x74, 0x00, 0x00, 0x00, 0x00, 0x02};
    esp_err_t err = esp_wifi_set_mac(WIFI_IF_STA, &newMACAddress[0]);
    #ifdef DEBUG
      if (err == ESP_OK) {Serial.println("Success changing Mac Address");}
      else {Serial.println("Fail changing Mac Address");}
    #endif
  #endif
  if (esp_now_init() == ESP_OK) {
    #if USE_DISPLAY == 1
      gfx->setCursor(320 - 2 * 16, 80);
      gfx->print("ok");
    #endif
    esp_now_init_success = true;
    #ifdef DEBUG
      Serial.println("ESPNow Init success");
    #endif
  }
  else {
    #if USE_DISPLAY == 1
      gfx->setCursor(320 - 3 * 16, 80);
      gfx->print("nok");
    #endif
    esp_now_init_success = false;
    #ifdef DEBUG 
      Serial.println("ESPNow Init fail");
    #endif
  }
  if (esp_now_init_success == true) {
    esp_now_register_send_cb(messageSent);  
    esp_now_register_recv_cb(messageReceived); 
    memcpy(peerInfo.peer_addr, MacAdressHanimandl, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      esp_now_init_success = false;
      #ifdef DEBUG
        Serial.println("Failed to add peer");
      #endif
    }
  }
  if (esp_now_init_success == true) {
    #if USE_DISPLAY == 1
      gfx->setCursor(10, 100);
      gfx->print("Init. Turntable:");
    #endif
    stepper_init();
    #if USE_DISPLAY == 1
      if (stepper_init_done = true) {
        gfx->setCursor(320 - 2 * 16, 100);
        gfx->print("ok");
      }
      else {
        gfx->setCursor(320 - 3 * 16, 100);
        gfx->print("nok");
      }
      delay(500);
      gfx->fillScreen(BACKGROUND);
      gfx->setFont(Punk_Mono_Bold_240_150);
      gfx->setCursor((320 - 18 * 14) / 2, 24);
      gfx->print("Honig schlepp Esel");
      gfx->drawLine(0, 30, 320, 30, TEXT);

      uint8_t baseMac[6];
      esp_wifi_get_mac(WIFI_IF_STA, baseMac);
      char mac_adress[30];
      sprintf(mac_adress, "%02x:%02x:%02x:%02x:%02x:%02x", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
      gfx->setCursor((320 - 17 * 14) / 2, 65);
      gfx->print(mac_adress);

      gfx->setFont(Punk_Mono_Bold_160_100);
      gfx->drawLine(0, 85, 320, 85, TEXT);
      gfx->setCursor(10, 105);
      gfx->print("Send:");
      gfx->setCursor(10, 125);
      gfx->print("Recived:");
      gfx->drawLine(0, 130, 320, 130, TEXT);
      gfx->setCursor(10, 150);
      gfx->print("Init done:");
      gfx->setCursor(160, 150);
      gfx->print("Center jar:");
      gfx->setCursor(10, 170);
      gfx->print("Init speed:");
      gfx->setCursor(160, 170);
      gfx->print("Run speed:");
      gfx->drawLine(0, 175, 320, 175, TEXT);
      gfx->setCursor(10, 195);
      gfx->print("Status:");
      gfx->setCursor(160, 195);
      gfx->print("Speed:");
      gfx->setCursor(160, 215);
      gfx->print("Angle max:");
      gfx->setCursor(10, 215);
      gfx->print("Angle min:");
      gfx->setCursor(10, 235);
      gfx->print("Wait to close:");
    #endif
  }
  #ifdef DEBUG
    Serial.println("-- End Setup loop");
  #endif
}

void loop() {
  //move_dp muss noch gemacht werden

  //Display
  update_display_init();
  update_display_speed_init();
  update_display_speed_run();
  update_display_scenter_jar();
  update_display_dp_status();
  update_display_dp_speed();
  update_display_dp_angle_min();
  update_display_dp_angle_max();
  update_display_dp_close_wait_time();
  while (esp_now_msg_recived == false) {
    stop = false;
    if (digitalRead(SWITCH) == 1 and stepper_init_done == true and strcmp(myReceivedMessage.text, "move_pos") != 0 and turntable_init_check == true and stop == false) {  //Stepper init zurücksetzen, fals einer der Drehteller von Hand verschiebt
      delay(500);
      if (digitalRead(SWITCH) == 1) {
        stepper_init_done = false;
      }
      break;
    }
    delay(10);
  }
  if (esp_now_msg_recived == true) {
    if (strcmp(myReceivedMessage.text, "move_pos") != 0) {
      move2pos_running = false;
    }
    if (strcmp(myReceivedMessage.text, "check") == 0) {
      update_display();
      delESPnowArrays();
      if (stepper_init_done == true) {strcpy(myMessageToBeSent.text, "ok_init");}
      else                           {strcpy(myMessageToBeSent.text, "nok_init");}
      sendMessage();
      #ifdef DEBUG
        Serial.print("myMessageToBeSent: "); Serial.println(myMessageToBeSent.text);
      #endif
    }
    else if (strcmp(myReceivedMessage.text, "speed_init") == 0) {
      update_display();
      delESPnowArrays();
      strcpy(myMessageToBeSent.text, "ok_speed_init");
      myMessageToBeSent.value = step_speed_init;
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "speed_run") == 0) {
      update_display();
      delESPnowArrays();
      strcpy(myMessageToBeSent.text, "ok_speed_run");
      myMessageToBeSent.value = step_speed_run;
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "ts_angle_min") == 0) {
      update_display();
      delESPnowArrays();
      strcpy(myMessageToBeSent.text, "ok_ts_angle_min");
      myMessageToBeSent.value = servo_min;
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "ts_angle_max") == 0) {
      update_display();
      delESPnowArrays();
      strcpy(myMessageToBeSent.text, "ok_ts_angle_max");
      myMessageToBeSent.value = servo_max;
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "ts_waittime") == 0) {
      update_display();
      delESPnowArrays();
      strcpy(myMessageToBeSent.text, "ok_ts_waittime");
      myMessageToBeSent.value = servo_wait;
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "ts_speed") == 0) {
      update_display();
      delESPnowArrays();
      strcpy(myMessageToBeSent.text, "ok_ts_speed");
      myMessageToBeSent.value = servo_speed;
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "speed_init_save") == 0) {
      update_display();
      preferences.putUInt("step_speed_init", myReceivedMessage.value);
      step_speed_init = myReceivedMessage.value;
      delESPnowArrays();
      strcpy(myMessageToBeSent.text, "ok_speed_init_save");
      myMessageToBeSent.value = servo_speed;
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "speed_run_save") == 0) {
      update_display();
      preferences.putUInt("step_speed_run", myReceivedMessage.value);
      step_speed_run = myReceivedMessage.value;
      delESPnowArrays();
      strcpy(myMessageToBeSent.text, "ok_speed_run_save");
      myMessageToBeSent.value = servo_speed;
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "pos_jar_steps_save") == 0) {
      update_display();
      preferences.putUInt("step_center_jar", myReceivedMessage.value);
      step_center_jar = myReceivedMessage.value;
      delESPnowArrays();
      strcpy(myMessageToBeSent.text, "ok_pos_jar_steps_save");
      myMessageToBeSent.value = servo_speed;
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "ts_angle_min_save") == 0) {
      update_display();
      preferences.putUInt("servo_min", myReceivedMessage.value);
      servo_min = myReceivedMessage.value;
      delESPnowArrays();
      strcpy(myMessageToBeSent.text, "ok_ts_angle_min_save");
      myMessageToBeSent.value = servo_speed;
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "ts_angle_max_save") == 0) {
      update_display();
      preferences.putUInt("servo_max", myReceivedMessage.value);
      servo_max = myReceivedMessage.value;
      delESPnowArrays();
      strcpy(myMessageToBeSent.text, "ok_ts_angle_max_save");
      myMessageToBeSent.value = servo_speed;
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "ts_waittime_save") == 0) {
      update_display();
      preferences.putUInt("servo_wait", myReceivedMessage.value);
      servo_wait = myReceivedMessage.value;
      delESPnowArrays();
      strcpy(myMessageToBeSent.text, "ok_ts_waittime_save");
      myMessageToBeSent.value = servo_speed;
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "ts_speed_save") == 0) {
      update_display();
      preferences.putUInt("servo_speed", myReceivedMessage.value);
      servo_speed = myReceivedMessage.value;
      delESPnowArrays();
      strcpy(myMessageToBeSent.text, "ok_ts_speed_save");
      myMessageToBeSent.value = servo_speed;
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "init") == 0) {
      update_display();
      delESPnowArrays();
      stepper_init();
      if (stepper_init_done == true) {strcpy(myMessageToBeSent.text, "ok_init_done");}
      else                           {strcpy(myMessageToBeSent.text, "nok_init_done");}
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "move_jar") == 0 and stepper_init_done == true) {
      update_display();
      delESPnowArrays();
      move_jar();
      if (move_jar_done == true) {strcpy(myMessageToBeSent.text, "ok_move_jar");}
      else                       {strcpy(myMessageToBeSent.text, "nok_move_jar");}
      move_jar_done = false;
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "move_pos") == 0) {
      update_display();
      if (move2pos_running == false) {
        stepper_init_done = false;
      }
      move2pos_running = true;
      move_pos();
      delESPnowArrays();
      if (move_pos_done == true) {strcpy(myMessageToBeSent.text, "ok_move_pos");}
      else                       {strcpy(myMessageToBeSent.text, "nok_move_pos");}
      move_pos_done = false;
      sendMessage();
    }
    //to do
    else if (strcmp(myReceivedMessage.text, "move_dp") == 0) {
      update_display();
      delESPnowArrays();
      //move_tp();
      if (move_tp_done == true) {strcpy(myMessageToBeSent.text, "ok_move_tp");}
      else                       {strcpy(myMessageToBeSent.text, "nok_move_tp");}
      move_tp_done = false;
      sendMessage();
    }
    //end to do
    else if (strcmp(myReceivedMessage.text, "close_drop_prodection") == 0) {
      update_display();
      delESPnowArrays();
      close_drop_protection(servo_wait);
      if (drop_protection == true) {strcpy(myMessageToBeSent.text, "ok_close_dp");}
      else                         {strcpy(myMessageToBeSent.text, "nok_close_dp");}
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "open_drop_prodection") == 0) {
      update_display();
      delESPnowArrays();
      open_drop_protection();
      if (drop_protection == false) {strcpy(myMessageToBeSent.text, "ok_open_dp");}
      else                          {strcpy(myMessageToBeSent.text, "nok_open_dp");}
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "turn_on_stepper_init_check") == 0) {
      update_display();
      delESPnowArrays();
      turntable_init_check = true;
      strcpy(myMessageToBeSent.text, "ok_on_init_check");
      sendMessage();
    }
    else if (strcmp(myReceivedMessage.text, "turn_off_stepper_init_check") == 0) {
      update_display();
      delESPnowArrays();
      turntable_init_check = false;
      strcpy(myMessageToBeSent.text, "ok_off_init_check");
      sendMessage();
    }
  }
  if (stepper_init_done == false and move2pos_running == false and strcmp(myReceivedMessage.text, "check") != 0) {
    #ifdef DEBUG
      Serial.print("stepper_init_done: "); Serial.println(stepper_init_done);
      Serial.print("move2pos_running: "); Serial.println(move2pos_running);
    #endif
    delESPnowArrays();
    strcpy(myMessageToBeSent.text, "init_error");
    sendMessage();
  }
}

