// PROJECT  : The IR Nixie Gecko
// PURPOSE  : Long ISP
// COURSE   : ICS3U
// AUTHOR   : Xander Chin
// DATE     : May 29, 2021
// MCU      : 328P
// STATUS   : Working. Could use some improvements (very messy)
// REFERENCE:

#include <EEPROM.h>   //was going to be used for alarm

#include <IRremote.h>
int RECV_PIN = 8;
IRrecv irrecv(RECV_PIN);
decode_results results;

#include <SPI.h>
#define OENABLE 9

#include <Wire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>

#include <NewTone.h>

#include <DHT.h>
#define DHTpin 9
DHT dht(DHTpin, DHT11);

uint32_t code;
uint32_t lastSwitch;
#define pov 5000
bool state = true;
uint8_t digits[4] = {1, 2, 3, 4};

//for mixed up digits
uint8_t trueNumbers[10] = {9, 2, 1, 4, 3, 6, 5, 8, 7, 0};

uint8_t output = 0;
uint8_t x = 0; 

uint8_t hours;
uint8_t minutes;
uint8_t months;
uint8_t days;
uint16_t years;
//uint8_t timerMinutes;
//uint16_t timerSeconds;
//uint16_t stopwatch = 0000;    //unimplemented

uint8_t alarmHour = 7;
uint8_t alarmMinute = 0;

bool edit;
bool digitOn;
bool alarm;
bool alarmOn;
bool snooze;
uint32_t lastSnooze;
bool timeMode;

uint8_t mode = 0;
uint8_t editMode = 0;
uint32_t lastCycle;
#define cycle 15

uint32_t lastPress;
#define debounce 250

uint32_t lastFlash;
uint32_t lastOn;

bool tempMode;
uint16_t t;
uint8_t h;

uint32_t lastTimeCycle;
uint8_t flash;

tmElements_t tm;

#define snoozeDuration 10000

void setup() {
  irrecv.enableIRIn(); // Start the receiver
  pinMode(2, OUTPUT);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(5, OUTPUT);
  pinMode(6, INPUT_PULLUP); 
  pinMode(7, OUTPUT);
  pinMode(12, INPUT_PULLUP);
    
  dht.begin();
  t = dht.readTemperature()*100;
  h = dht.readHumidity();
}

void loop() {
  alarm = digitalRead(A2) ? true : false;
  timeMode = digitalRead(A3) ? true : false;
  if(!alarm) alarmOn = false;
  if(alarm && alarmHour == tm.Hour && alarmMinute == tm.Minute && tm.Second == 0) {
    alarmOn = true;
    snooze = false;
  }
  if(alarm && alarmOn) {
    NewTone(5, 330);
    if(!digitalRead(12)) {
      alarmOn = false;
      snooze = true;
    }    
  } else noNewTone();
    
  if(alarm && snooze) {
    if(millis() - lastSnooze > snoozeDuration) {
      alarmOn = true;
      snooze = false;
      lastSnooze = millis();
    }
  }

  //refresh variables
  hours = tm.Hour;
  minutes = tm.Minute;
  months = tm.Month;
  days = tm.Day;
  years = tm.Year; 
  
  edit = digitalRead(A1) ? true : false; 
  if(edit) {
    switch(mode) {
      case 0: editTime(); break;
      case 1: editDate(); break;
      case 2: editYear(); break;
      case 3: editAlarm(); break;
      //case 4: editTimer(); break;   //unimplemented
      default: break;    
    }
  } else {
    flash = B1100;
    switch(mode) {
      case 0: showTime(); break;
      case 1: showDate(); break;
      case 2: showYear(); break;
      case 3: showTemp(); break;
      //case 4: showTimer(); break;   //unimplemented
      default: break;
    }
  }

  //IR codes
  if(irrecv.decode(&results)) {
    code = results.value;
    irrecv.resume();
  }
  
  if(code == 100) {
    mode++;
    code = 0;
    scramble();
  }
  
  if(code == 200) {
    mode--;
    code = 0;
    scramble();
  }
  
  if(!edit) {
    if(!digitalRead(6) && millis() - lastPress > debounce) {       
      scramble();
      mode++;
      lastPress = millis();  
    }
  }

  //cycle through digits
  if(millis() - lastTimeCycle > 60000) {
    scramble();
    lastTimeCycle = millis();
  }

  //blink colon bulbs
  state = tm.Second % 2 ? 1 : 0;
  digitalWrite(7, state);
    
  if(!edit) displayNumbers();   //instead flash digits

  //cycles to the first mode after the last
  mode = mode % 4;
}

//
void displayNumbers() {
  if(micros() - lastSwitch > pov) {
    bitWrite(output, 4+x, LOW);
    hardwareShiftOut(output);
    x = (x+1) % 4;
    output = digits[x];
    hardwareShiftOut(output);
    bitWrite(output, 4+(3-x), HIGH);
    lastSwitch = micros();
  }  
  hardwareShiftOut(output);
}

void digitFlash(uint8_t number) {
  if(millis() - lastOn > 500) {
    digitOn = !digitOn;
    lastOn = millis();
  }
  if(micros() - lastFlash > pov) {   
    bitWrite(output, 4+x, LOW);
    hardwareShiftOut(output);
    x = (x+1) % 4;
    output = digits[x];
    hardwareShiftOut(output);
    if(digitOn) {
      bitWrite(output, 4+(3-x), HIGH);
    } else {
      if(bitRead(number, 3-x)) bitWrite(output, 4+(3-x), HIGH);
    }    
    lastFlash = micros();
  }  
  hardwareShiftOut(output);
}

void showTime() {
  if(RTC.read(tm)) {
    if(timeMode) {
      if(tm.Hour == 0 || tm.Hour == 12) 
        setDigits(12*100 + tm.Minute);
      else setDigits((tm.Hour % 12)*100 + tm.Minute);
    }
    else setDigits(tm.Hour*100 + tm.Minute);
  }
  if(timeMode) {
    if(tm.Hour > 11) digitalWrite(2, HIGH);
    else digitalWrite(2, LOW);
  } else digitalWrite(2, LOW);  
}

void showDate() {
  if(RTC.read(tm)) {
    setDigits(tm.Month*100 + tm.Day);
  }
}

void showYear() {
  if(RTC.read(tm)) {
    setDigits(tmYearToCalendar(tm.Year));
  }
}

void showTemp() {
  if(RTC.read(tm));   //keeps updating the time
  if(millis() - lastCycle > 3000) {
    tempMode = !tempMode;
    if(!tempMode) scramble();
    t = dht.readTemperature()*100;
    h = dht.readHumidity();
    lastCycle = millis();
  }
  tempMode ? setDigits(h) : setDigits(t);
}

void showTimer() {
  //unimplemented
}

void showAlarm() {
  setDigits(alarmHour*100 + alarmMinute); 
}

void editTime() {
  //configure time
  if(!digitalRead(3) && millis() - lastPress > debounce) {
    if(flash == B1100) {
      hours = (hours + 1) % 24;
      tm.Hour = hours;
      RTC.write(tm); 
    } else if(flash == B0011) {
      minutes = (minutes + 1) % 60;
      tm.Minute = minutes;
      RTC.write(tm); 
    }
    lastPress = millis();
  } else if(!digitalRead(4) && millis() - lastPress > debounce) {
    if(flash == B1100) {
      hours == 0 ? hours = 23 : hours--;
      tm.Hour = hours;
      RTC.write(tm); 
    } else if(flash == B0011) {
      minutes == 0 ? minutes = 59 : minutes--;
      tm.Minute = minutes;
      RTC.write(tm); 
    }
    lastPress = millis();
  }
  showTime();
  flashNumbers();
}

void editDate() {
  //configure date
  if(!digitalRead(3) && millis() - lastPress > debounce) {
    if(flash == B1100) {
      months = (months + 1) % 13;
      tm.Month = months;
      RTC.write(tm); 
    } else if(flash == B0011) {
      days = (days + 1) % 60;
      tm.Day = days;
      RTC.write(tm); 
    }
    lastPress = millis();
  } else if(!digitalRead(4) && millis() - lastPress > debounce) {
    if(flash == B1100) {
      months == 0 ? months = 23 : months--;
      tm.Month = months;
      RTC.write(tm); 
    } else if(flash == B0011) {
      days == 0 ? days = 59 : days--;
      tm.Day = days;
      RTC.write(tm); 
    }
    lastPress = millis();
  }
  showDate();
  flashNumbers();
}

void editYear() {
  if(!digitalRead(3) && millis() - lastPress > debounce) {
    years = (years + 1) % 10000;
    tm.Year = years;
    RTC.write(tm);
    lastPress = millis();
  } else if(!digitalRead(4) && millis() - lastPress > debounce) {
    years == 0 ? years = 9999 : years--;
    tm.Year = years;
    RTC.write(tm);
    lastPress = millis();
  }
  showYear();
  digitFlash(0);
  if(!digitalRead(6) && millis() - lastPress > debounce) {
    mode++;
  } else if(!digitalRead(12) && millis() - lastPress > debounce) {
    mode--;
  }
}

void editAlarm() {
  if(!digitalRead(3) && millis() - lastPress > debounce) {
    if(flash == B1100) {
      alarmHour = (alarmHour + 1) % 24;
    } else if(flash == B0011) {
      alarmMinute = (alarmMinute + 1) % 60;
    }
    lastPress = millis();
  } else if(!digitalRead(4) && millis() - lastPress > debounce) {
    if(flash == B1100) {
      alarmHour == 0 ? alarmHour = 23 : alarmHour--;
    } else if(flash == B0011) {
      alarmMinute == 0 ? alarmMinute = 59 : alarmMinute--;
    }
    lastPress = millis();
  }
  showAlarm();
  flashNumbers();
}

void editTimer() {
  //not implemented yet
}

void hardwareShiftOut(uint8_t value) {
  //Initializes the SPI bus setting SCK, MOSI, and SS to outputs,
  SPI.begin();                //pulls SCK and MOSI low, and SS high. Default: MSBFIRST
  SPI.transfer(value);        //invert the output for ease of interpretation
  digitalWrite(SS, LOW);      //pull Slave Select LOW to identify target device
  digitalWrite(SS, HIGH);     //release target device
  SPI.end();                  //disables SPI Bus (leaving pin modes unchanged)
  digitalWrite(OENABLE, state);
}

void scramble() {   //iterates through all digits
  for(uint8_t x = 0; x < 30; x++) {
    for(uint8_t y = 0; y < 4; y++) {
      digits[y] = (digits[y] + 1) % 10;
    }
    lastCycle = millis();
    while(millis() - lastCycle < cycle) {
      displayNumbers();
    }
  }
}

void setDigits(uint16_t number) {   //set a 4-digit number to display
  uint8_t n = 0;
  for(uint8_t x = 0; x < sizeof(digits); x++) {    
    n = number % 10;
    number /= 10;
    digits[x] = trueNumbers[n]; 
  }
}

void flashNumbers() {   //flash/blink specific digits
  if(!digitalRead(6) && millis() - lastPress > debounce) {
    flash >>= 2;
    if(flash == 0) {
      mode++;
      flash = B1100;
    }
    lastPress = millis();
  } else if(!digitalRead(12) && millis() - lastPress > debounce) {
    flash <<= 2;
    if(flash > 16) {
      mode--;
      flash = B0011;
    }
    lastPress = millis();
  }
  digitFlash(flash);
}
