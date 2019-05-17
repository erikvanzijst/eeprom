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
enum MODE {READ, WRITE};
const unsigned int MAX_PAYLOAD = 63;
const unsigned int DELAY_US = 1;

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

// Data pins (LSB to MSB)
const unsigned int dataPins[] = {2, 3, 4, 5, 6, 7, 8, 9};

MODE mode = NULL;
byte buf[1 + MAX_PAYLOAD];

void setup() {
  Serial.begin(9600);
//  Serial.setTimeout(60l * 1000);

  pinMode(EEPROM_CE, OUTPUT);
  pinMode(EEPROM_OE, OUTPUT);
  pinMode(EEPROM_WE, OUTPUT);

  pinMode(SHIFT_SER, OUTPUT);
  pinMode(SHIFT_RCLK, OUTPUT);
  pinMode(SHIFT_SCLK, OUTPUT);

  pinMode(ACT_LED, OUTPUT);
  digitalWrite(ACT_LED, LOW);

  readMode();
}

/*
 * Reads the next message from the serial port and copies its payload into
 * the global `buf` byte array.
 * This function does not send any acknowledgements which is left to the
 * caller.
 *
 * Returns the number of bytes that were copied into `buf` (0 for acks).
 */
int receive() {
  int l = Serial.read();
  if (l > 0) {
    Serial.readBytes(buf, min(l, sizeof(buf)));
  }
  return l;
}

/*
 * Writes the supplied bytes to the serial port, preceeding it with a length
 * octet.
 *
 * This function enforces flow control, blocking until the client has
 * acknowledged receipt with a 0-byte ack.
 */
void send(byte *buf, unsigned int len, bool waitForAck) {
  Serial.write(len);
  if (len > 0) {
    Serial.write(buf, len);
  }
  if (waitForAck && receive() != 0) {
    error();
  }
}

void pulse(int pin) {
  digitalWrite(pin, HIGH);
  delayMicroseconds(DELAY_US);
  digitalWrite(pin, LOW);
  delayMicroseconds(DELAY_US);
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

/*
 * Returns the byte at the specified address.
 */
byte readAddr(unsigned int addr) {
  loadShiftAddr(addr);
  delayMicroseconds(DELAY_US);

  byte val = 0;
  for (unsigned int i = 0; i < 8; i++) {
    val |= (digitalRead(dataPins[i]) << i);
  }
  return val;
}

/*
 * Writes a single byte to the specified addess.
 * Requires IO0-IO7 to be set to OUTPUT mode, EEPROM_CE to be LOW and EEPROM_OE
 * to be HIGH prior to invocation.
 */
void writeAddr(unsigned int addr, byte val) {
  loadShiftAddr(addr);

  // load data byte
  for (unsigned int i = 0; i < 8; i++) {
    digitalWrite(dataPins[i], (val >> i) & 1);
  }
  delayMicroseconds(DELAY_US);

  digitalWrite(EEPROM_WE, LOW);
  delayMicroseconds(DELAY_US);
  digitalWrite(EEPROM_WE, HIGH);
}

/*
 * Writes the full contents of the EEPROM to the serial port in sequential
 * messages of up to 63 bytes, waiting for explicit acknowledgement of each.
 */
void dump() {
  byte payload[MAX_PAYLOAD];
  unsigned int i = 0;

  for (unsigned int addr = 0; addr < 32768; addr++) {
    i = addr % MAX_PAYLOAD;

    if (addr > 0 && i == 0) {
      // payload at capacity, send out:
      send(buf, MAX_PAYLOAD, true);
    }
    payload[i++] = readAddr(addr);
  }

  if (i) {
    // send remainder
    send(buf, i, true);
  }
}

/*
 * Reads the specified number of bytes from the serial port and writes them
 * to the EEPROM.
 * 
 * This requires the Arduino to be in WRITE mode.
 */
void load(unsigned int len) {
  unsigned int addr = 0;

  while (addr < len) {
    unsigned int cnt = receive();
    send(NULL, 0, true);

    for (unsigned int i = 0; i < cnt; i++) {
      writeAddr(addr++, buf[i]);
    }
  }
}

/*
 * Switches the pin mode for the I/O pins to OUTPUT, pulls EEPROM_CE LOW and
 * EEPROM_OE HIGH.
 */
void writeMode() {
  if (mode != WRITE) {
    digitalWrite(EEPROM_CE, LOW);
    digitalWrite(EEPROM_OE, HIGH);
    digitalWrite(EEPROM_WE, HIGH);

    for (unsigned int i = 0; i < 8; i++) {
      pinMode(dataPins[i], OUTPUT);
    }

    mode = WRITE;
  }
}

/**
 * Switches the pin mode for the I/O pins to INPUT, pulls EEPROM_CE LOW,
 * EEPROM_OE LOW and EEPROM_WE HIGH.
 */
void readMode() {
  if (mode != READ) {
    digitalWrite(EEPROM_CE, LOW);
    digitalWrite(EEPROM_OE, LOW);
    digitalWrite(EEPROM_WE, HIGH);

    for (unsigned int i = 0; i < 8; i++) {
      pinMode(dataPins[i], INPUT);
    }

    mode = READ;
  }
}

void error() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(ACT_LED, LOW);
    delay(100);
    digitalWrite(ACT_LED, HIGH);
    delay(100);
  }
}

void loop() {
  if (Serial.available() > 0) {
    digitalWrite(ACT_LED, HIGH);

    unsigned int len = receive();

    if (buf[0] == 0x72 && len == 3) {
      byte val = readAddr((buf[1] << 8) + buf[2]);
      send(&val, 1, false);

    } else if (buf[0] == 0x77 && len == 4) {
        writeMode();
        writeAddr((buf[1] << 8) + buf[2], buf[3]);
        // signal operation completion
        send(NULL, 0, false);
        readMode();
      
    } else if (buf[0] == 0x64 && len == 1) {
        dump();
      
    } else if (buf[0] == 0x6c && len == 3) {
        writeMode();
        load((buf[1] << 8) + buf[2]);
        readMode();

    } else {
      error();
    }
    digitalWrite(ACT_LED, LOW);
  }
}
