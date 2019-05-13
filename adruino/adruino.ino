/*
 * AT28C256 EEPROM Reader and Programmer
 * 
 * This code implements the serial wire protocol as described in protocol.txt
 * 
 * Pin Layout
 * 
 * Pin | Circuit
 * ----+--------------
 *   2 | EEPROM IO0
 *   3 | EEPROM IO1
 *   4 | EEPROM IO2
 *   5 | EEPROM IO3
 *   6 | EEPROM IO4
 *   7 | EEPROM IO5
 *   8 | EEPROM IO6
 *   9 | EEPROM IO7
 * ----+--------------
 *  10 | 74HC595 SER
 *  11 | 74HC595 RCLK
 *  12 | 74HC595 SCLK
 * ----+--------------
 *  A0 | EEPROM CE
 *  A1 | EEPROM OE
 *  A2 | EEPROM WE
 * ----+--------------
 *  13 | Activity LED
 * ----+--------------
 *  
 * Copyright 2019, Erik van Zijst <erik.van.zijst@gmail.com>
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// AT28C256 contol lines
const unsigned int EEPROM_CE = A0;
const unsigned int EEPROM_OE = A1;
const unsigned int EEPROM_WE = A2;

// 74HC595 control lines
const unsigned int SHIFT_SER = 10;
const unsigned int SHIFT_RCLK = 11;
const unsigned int SHIFT_SCLK = 12;

// Activity indicator LED
const unsigned int ACT_LED = 13;

const unsigned int dataPins[] = {2, 3, 4, 5, 6, 7, 8, 9};

unsigned int len;
byte buf[4];

void setup() {
  Serial.begin(19200);

  pinMode(EEPROM_CE, OUTPUT);
  pinMode(EEPROM_OE, OUTPUT);
  pinMode(EEPROM_WE, OUTPUT);

  pinMode(SHIFT_SER, OUTPUT);
  pinMode(SHIFT_RCLK, OUTPUT);
  pinMode(SHIFT_SCLK, OUTPUT);

  pinMode(ACT_LED, OUTPUT);
  digitalWrite(ACT_LED, LOW);

  for (unsigned int i = 0; i < 8; i++) {
    pinMode(dataPins[i], INPUT);
  }
}

void pulse(int pin) {
  digitalWrite(pin, HIGH);
  delayMicroseconds(1);
  digitalWrite(pin, LOW);
}

/*
 * Loads the specified 16 bit address into the 595 shift register.
 */
void loadShiftAddr(unsigned int addr) {
  for (int i = 15; i >= 0; i--) {
    digitalWrite(SHIFT_SER, ((addr >> i) & 1) ? HIGH : LOW);
    pulse(SHIFT_SCLK);
  }
  pulse(SHIFT_RCLK);
}

void readAddr(unsigned int addr) {
  loadShiftAddr(addr);

  byte val = 0;
  for (unsigned int i = 0; i < 8; i++) {
    val |= (digitalRead(dataPins[i]) << i);
  }

  Serial.write(val);
}

void writeAddr(unsigned int addr, byte val) {
  Serial.write(0);
}

void dump() {
  for (unsigned int addr = 0; addr < 32768; addr++) {
    byte val = 0;
    loadShiftAddr(addr);
    delayMicroseconds(1);
    for (unsigned int i = 0; i < 8; i++) {
      val |= (digitalRead(dataPins[i]) << i);
    }
    Serial.write(val);
  }
}

void load() {
}

void loop() {
  if (Serial.available() > 0) {
    digitalWrite(ACT_LED, HIGH);

    len = Serial.read();
    Serial.readBytes(buf, len);

    if (buf[0] == 0x72 && len == 3) {
        readAddr((buf[1] << 8) + buf[2]);

    } else if (buf[0] == 0x77 && len == 4) {
        writeAddr((buf[1] << 8) + buf[2], buf[3]);
      
    } else if (buf[0] == 0x64 && len == 1) {
        dump();
      
    } else if (buf[0] == 0x6c && len == 3) {
        load();

    } else {
      for (int i = 0; i < 5; i++) {
        digitalWrite(ACT_LED, LOW);
        delay(200);
        digitalWrite(ACT_LED, HIGH);
        delay(200);
      }
    }
    digitalWrite(ACT_LED, LOW);
  }
}
