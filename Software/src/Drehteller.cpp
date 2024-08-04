
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ESP32Servo.h>

//#define DEBUG
#define READ_PREFERENCES

Preferences preferences;

//ESP Now
//const uint8_t MacAdressHanimandl[] = {0xB8, 0xD6, 0x1A, 0x47, 0xE0, 0x60};
#include "./Resources/mac.h"
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

//Pin Positionsswitch
int SWITCH = 16;

bool stepper_init_done = false;           //has to be true that the turntable works
bool move_jar_done = false;               //Flag to check move jar is done
bool move_pos_done = false;               //Flag to check move pos is done
bool move2pos_running = false;            //Flag if we are in the menu Move Jar
bool drop_protection = false;             //fase --> drop protction open, true --> drop protection close
bool turntable_init_check = true;           //false --> turntable init check: off, true --> turntable init check on
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

void messageSent(const uint8_t *macAddr, esp_now_send_status_t status) {
  #ifdef DEBUG
    Serial.print("Send status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success":"Fail");
  #endif
}

void messageReceived(const uint8_t* macAddr, const uint8_t* incomingData, int len){
  memset (&myReceivedMessage, 0, sizeof(myReceivedMessage));
  memcpy(&myReceivedMessage, incomingData, sizeof(myReceivedMessage));
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
  step_counter = 0;
  while (step_counter < step_counter_value / 4) {make_step();}
  if (change_rotation) {digitalWrite(DIRECTION_PIN, LOW);}
  else {digitalWrite(DIRECTION_PIN, HIGH);}
  step_speed = step_speed_run*2;
  step_counter = 0;
  while (step_counter < step_counter_value / 4 * 1.5 and digitalRead(SWITCH) == 1) {make_step();}
  step_speed = step_speed_run;
  if (change_rotation) {digitalWrite(DIRECTION_PIN, LOW);}
  else {digitalWrite(DIRECTION_PIN, HIGH);}
  step_counter = 0;
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
    //so ne pseudo abfrage über den Servowinkel. Muss noch win taster implementiert werden
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

void close_drop_protection() {
  if (drop_protection == false) {
    //drop_protection = false;
    int servo_act = servo_max;
    //servo_wait
    int wait_millis = millis();
    while (millis() - wait_millis < servo_wait * 1000) { 
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

void setup() {
  Serial.begin(115200);
  delay(1000);
  #ifdef DEBUG
    Serial.println("-- Setup loop");
  #endif
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIRECTION_PIN, OUTPUT);
  pinMode(SWITCH, INPUT_PULLDOWN);
  readPreferences();
  servo.attach(servo_pin, 1000, 2000); // default Werte. Achtung, steuert den Nullpunkt weniger weit aus!
  SERVO_WRITE(90);
  delay(1000);
  close_drop_protection();
  WiFi.mode(WIFI_STA);
  if (esp_now_init() == ESP_OK) {
    esp_now_init_success = true;
    #ifdef DEBUG
      Serial.println("ESPNow Init success");
    #endif
  }
  else {
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
    stepper_init();
  }
  #ifdef DEBUG
    Serial.println("-- End Setup loop");
  #endif
}

void loop() {
  //move_dp

  while (esp_now_msg_recived == false) {
    stop = false;
    //Serial.println(turntable_init_check);
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
      if (stepper_init_done == true) {strcpy(myMessageToBeSent.text, "ok_init");}
      else                           {strcpy(myMessageToBeSent.text, "nok_init");}
      #ifdef DEBUG
        Serial.print("myMessageToBeSent: "); Serial.println(myMessageToBeSent.text);
      #endif
    }
    else if (strcmp(myReceivedMessage.text, "speed_init") == 0) {
      myMessageToBeSent.value = step_speed_init;
    }
    else if (strcmp(myReceivedMessage.text, "speed_run") == 0) {
      myMessageToBeSent.value = step_speed_run;
    }
    else if (strcmp(myReceivedMessage.text, "ts_angle_min") == 0) {
      myMessageToBeSent.value = servo_min;
    }
    else if (strcmp(myReceivedMessage.text, "ts_angle_max") == 0) {
      myMessageToBeSent.value = servo_max;
    }
    else if (strcmp(myReceivedMessage.text, "ts_waittime") == 0) {
      myMessageToBeSent.value = servo_wait;
    }
    else if (strcmp(myReceivedMessage.text, "ts_speed") == 0) {
      myMessageToBeSent.value = servo_speed;
    }
    else if (strcmp(myReceivedMessage.text, "speed_init_save") == 0) {
      preferences.putUInt("step_speed_init", myReceivedMessage.value);
      step_speed_init = myReceivedMessage.value;
    }
    else if (strcmp(myReceivedMessage.text, "speed_run_save") == 0) {
      preferences.putUInt("step_speed_run", myReceivedMessage.value);
      step_speed_run = myReceivedMessage.value;
    }
    else if (strcmp(myReceivedMessage.text, "pos_jar_steps_save") == 0) {
      preferences.putUInt("step_center_jar", myReceivedMessage.value);
      step_center_jar = myReceivedMessage.value;
    }
    else if (strcmp(myReceivedMessage.text, "ts_angle_min_save") == 0) {
      preferences.putUInt("servo_min", myReceivedMessage.value);
      servo_min = myReceivedMessage.value;
    }
    else if (strcmp(myReceivedMessage.text, "ts_angle_max_save") == 0) {
      preferences.putUInt("servo_max", myReceivedMessage.value);
      servo_max = myReceivedMessage.value;
    }
    else if (strcmp(myReceivedMessage.text, "ts_waittime_save") == 0) {
      preferences.putUInt("servo_wait", myReceivedMessage.value);
      servo_wait = myReceivedMessage.value;
    }
    else if (strcmp(myReceivedMessage.text, "ts_speed_save") == 0) {
      preferences.putUInt("servo_speed", myReceivedMessage.value);
      servo_speed = myReceivedMessage.value;
    }
    else if (strcmp(myReceivedMessage.text, "init") == 0) {
      stepper_init();
      if (stepper_init_done == true) {strcpy(myMessageToBeSent.text, "ok_init_done");}
      else                           {strcpy(myMessageToBeSent.text, "nok_init_done");}
    }
    else if (strcmp(myReceivedMessage.text, "move_jar") == 0 and stepper_init_done == true) {
      move_jar();
      if (move_jar_done == true) {strcpy(myMessageToBeSent.text, "ok_move_jar");}
      else                       {strcpy(myMessageToBeSent.text, "nok_move_jar");}
      move_jar_done = false;
    }
    else if (strcmp(myReceivedMessage.text, "move_pos") == 0) {
      if (move2pos_running == false) {
        counter1 = 0;
        stepper_init_done = false;
      }
      move2pos_running = true;
      move_pos();
      if (move_pos_done == true) {strcpy(myMessageToBeSent.text, "ok_move_pos");}
      else                       {strcpy(myMessageToBeSent.text, "nok_move_pos");}
      move_pos_done = false;
    }
    else if (strcmp(myReceivedMessage.text, "close_drop_prodection") == 0) {
      close_drop_protection();
      if (drop_protection == true) {strcpy(myMessageToBeSent.text, "ok_close_dp");}
      else                         {strcpy(myMessageToBeSent.text, "nok_close_dp");}
    }
    else if (strcmp(myReceivedMessage.text, "open_drop_prodection") == 0) {
      open_drop_protection();
      if (drop_protection == false) {strcpy(myMessageToBeSent.text, "ok_open_dp");}
      else                          {strcpy(myMessageToBeSent.text, "nok_open_dp");}
    }
    else if (strcmp(myReceivedMessage.text, "turn_on_stepper_init_check") == 0) {
      Serial.println("gaga");
      turntable_init_check = true;
      strcpy(myMessageToBeSent.text, "ok_on_init_check");
    }
    else if (strcmp(myReceivedMessage.text, "turn_off_stepper_init_check") == 0) {
      Serial.println("gugu");
      turntable_init_check = false;
      strcpy(myMessageToBeSent.text, "ok_off_init_check");
    }
  }
  if (stepper_init_done == false and move2pos_running == false and strcmp(myReceivedMessage.text, "check") != 0) {
    #ifdef DEBUG
      Serial.print("stepper_init_done: "); Serial.println(stepper_init_done);
      Serial.print("move2pos_running: "); Serial.println(move2pos_running);
    #endif
    strcpy(myMessageToBeSent.text, "init_error");
  }
  esp_now_msg_recived = false;
  esp_err_t result = esp_now_send(MacAdressHanimandl, (uint8_t *) &myMessageToBeSent, sizeof(myMessageToBeSent));
  if (result != ESP_OK) {
    bool esp_now_send_error = true;
    #ifdef DEBUG
      Serial.print("Sending error: "); Serial.println(myMessageToBeSent.text);
    #endif
  }
  else {
    bool esp_now_send_error = false;
    #ifdef DEBUG
      Serial.print("Sending sucsess: "); Serial.println(myMessageToBeSent.text);
    #endif
  }
  memset (&myReceivedMessage, 0, sizeof(myReceivedMessage));
  memset (&myMessageToBeSent, 0, sizeof(myMessageToBeSent));
}

