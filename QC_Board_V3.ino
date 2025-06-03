#include <BluetoothSerial.h>
BluetoothSerial SerialBT;
// struct global untuk menyimpan hasil QC satu channel
struct ChannelResult {
  int state;      // STATE_CONNECTED, STATE_DISCONNECTED, STATE_SHORT, STATE_MISWIRE
  int partner;    // channel partner untuk SHORT atau MISWIRE
};


#define DS 23    // Data (SER)
#define SHCP 18  // Clock (SRCLK)
#define STCP 5   // Latch (RCLK)

#define SIG 26   // Output dari MUX
#define S0 13    // Select pin 0
#define S1 12    // Select pin 1
#define S2 14    // Select pin 2
#define S3 19    // Select pin 3
#define EN_MUX1 25  // Enable MUX 1
#define EN_MUX2 33  // Enable MUX 2
#define EN_MUX3 15  // Enable MUX 3
#define EN_MUX4 2   // Enable MUX 4
#define EN_MUX5 4   // Enable MUX 5

// --- PENAMBAHAN UNTUK KABEL X --- //
#define EN_MUX6 27  // Enable MUX 6 (kabel X)

// Definisi status kabel
#define STATE_CONNECTED     0      // Terhubung (C)
#define STATE_DISCONNECTED  1      // Putus (N)
#define STATE_SHORT         2      // Short (S)
#define STATE_MISWIRE       3      // Salah jalur (W&...)

// Delay antar channel (ms)
const int interval = 0;

// Array status channel: true = digunakan, false = tidak digunakan
bool channelUsed[76] = {
  true, false, false, false, true, true, true, true, true, false, false, true, true,  // Channel 1-13
  false, true, false, false, false, false, false, false, true, false, true, true, false,  // Channel 14-26
  true, true, true, true, false, true, true, true, true, true, true, false, true,           // Channel 27-39
  true, true, false, true, true, true, true, false, false, true, true, false, true,           // Channel 40-52
  true, true, true, true, true, true, true, true, true, true, true, true,                   // Channel 53-64
  true, true, true, true, true, true, true, true, true, true, true, true                  // Channel 65-76
};

// Fungsi untuk menghitung jumlah bit yang aktif dalam uint16_t
uint8_t countSetBits(uint16_t value) {
  uint8_t count = 0;
  while (value) {
    count += (value & 1);
    value >>= 1;
  }
  return count;
}

// Menghasilkan label kabel berdasarkan nomor global channel
String getLabel(int globalChannel) {
  if (globalChannel <= 13) {
    return "K1-A" + String(globalChannel);
  } else if (globalChannel <= 26) {
    return "K1-B" + String(globalChannel - 13);
  } else if (globalChannel <= 39) {
    return "K2-A" + String(globalChannel - 26);
  } else if (globalChannel <= 52) {
    return "K2-B" + String(globalChannel - 39);
  } else if (globalChannel <= 64) {
    return "K3-A" + String(globalChannel - 52);
  } else {
    return "K3-B" + String(globalChannel - 64);
  }
}



void resetPins() {
  pinMode(DS, OUTPUT);
  pinMode(SHCP, OUTPUT);
  pinMode(STCP, OUTPUT);
  
  digitalWrite(DS, LOW);
  digitalWrite(SHCP, LOW);
  digitalWrite(STCP, LOW);

  pinMode(SIG, INPUT_PULLDOWN);

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  
  digitalWrite(EN_MUX1, LOW);
  digitalWrite(EN_MUX2, LOW);
  digitalWrite(EN_MUX3, LOW);
  digitalWrite(EN_MUX4, LOW);
  digitalWrite(EN_MUX5, LOW);
  digitalWrite(EN_MUX6, HIGH);
}

// --- FUNGSI SHIFT REGISTER UNTUK 76 BIT --- //
void shiftOut80(uint64_t data) {
  digitalWrite(STCP, LOW);
  for (int i = 75; i >= 0; i--) {
    digitalWrite(SHCP, LOW);
    digitalWrite(DS, (data >> i) & 1);
    digitalWrite(SHCP, HIGH);
    delayMicroseconds(5);
  }
  digitalWrite(STCP, HIGH);
  delayMicroseconds(20);
}

// Membaca nilai 16-bit dari MUX (MUX 1-5)
uint16_t readMux16(int muxNumber) {
  uint16_t muxData = 0;
  
  digitalWrite(EN_MUX1, muxNumber == 1 ? LOW : HIGH);
  digitalWrite(EN_MUX2, muxNumber == 2 ? LOW : HIGH);
  digitalWrite(EN_MUX3, muxNumber == 3 ? LOW : HIGH);
  digitalWrite(EN_MUX4, muxNumber == 4 ? LOW : HIGH);
  digitalWrite(EN_MUX5, muxNumber == 5 ? LOW : HIGH);
  
  for (int i = 0; i < 16; i++) {
    digitalWrite(S0, i & 0x01);
    digitalWrite(S1, (i >> 1) & 0x01);
    digitalWrite(S2, (i >> 2) & 0x01);
    digitalWrite(S3, (i >> 3) & 0x01);
    delayMicroseconds(10);
    
    pinMode(SIG, INPUT_PULLDOWN);
    int val = digitalRead(SIG);
    if (val == HIGH) {
      muxData |= (1 << i);
    }
  }
  return muxData;
}

// Struct untuk menyimpan hasil deteksi tiap channel

// Global flag untuk mode QC kabel normal dan kabel X
bool qcActive = false;
bool qcActiveX = false;
bool qcAllActive = false;

//
// Fungsi runQC() untuk kabel normal (channel 1-76)
// tanpa iterasi sampling (mengambil pembacaan satu kali)
//
// --- FUNGSI runQC() untuk kabel normal (channel 1-76) ---
// --- NEW: read all 6 MUX outputs into one 96-bit array (we'll actually use up to 88) ---
// ===================================================
// Run full QC untuk 76 channel normal (MUX1–5),
// dengan deteksi antar-MUX
// ===================================================
// ===== Fungsi generic untuk membaca globalMap =====
void readAllMux(bool globalMap[], int maxCh) {
  // baca MUX1–5
  for (int m = 1; m <= 5; m++) {
    uint16_t d16 = readMux16(m);
    for (int b = 0; b < 16; b++) {
      int idx = (m-1)*16 + b;
      if (idx < maxCh) globalMap[idx] = (d16 >> b) & 1;
    }
  }
}

void runQC_K1(String &outputBuffer) {
  const int startCh = 0, endCh = 26; 
  ChannelResult results[26];
  bool globalMap[76] = {0};

  for (int i = startCh; i < endCh; i++) {
    results[i - startCh].state   = -1;
    results[i - startCh].partner = -1;
  }

  for (int ch = startCh; ch < endCh; ch++) {
    if (!channelUsed[ch]) {
      results[ch - startCh].state = -1; 
       yield();
      continue;
    }

    shiftOut80(1ULL << ch);
    delayMicroseconds(5);

    memset(globalMap, 0, sizeof(globalMap));
    readAllMux(globalMap, 76);

    int totalActive = 0;
    for (int i = startCh; i < endCh; i++)
      totalActive += globalMap[i];
    bool selfActive = globalMap[ch];

    // 4) Tentukan state & partner hanya dalam grup ini
    if (selfActive) {
      if (totalActive == 1) {
        results[ch - startCh].state = STATE_CONNECTED;
      } else {
        results[ch - startCh].state = STATE_SHORT;
        // cari partner dalam range startCh..endCh
        for (int i = startCh; i < endCh; i++) {
          if (i != ch && globalMap[i]) {
            results[ch - startCh].partner = i;
            break;
          }
        }
      }
    } else {
      if (totalActive > 0) {
        results[ch - startCh].state = STATE_MISWIRE;
        for (int i = startCh; i < endCh; i++) {
          if (globalMap[i]) {
            results[ch - startCh].partner = i;
            break;
          }
        }
      } else {
        results[ch - startCh].state = STATE_DISCONNECTED;
      }
    }
     yield();
  }

  // 5) Cetak hasil untuk K1 saja
  for (int i = startCh; i < endCh; i++) {
    String label = getLabel(i+1);
    String s;
    ChannelResult &r = results[i - startCh];
    switch (r.state) {
      case STATE_CONNECTED:    s = "C"; break;
      case STATE_DISCONNECTED: s = "N"; break;
      case STATE_SHORT:
        s = "S&" + getLabel(r.partner+1).substring(getLabel(r.partner+1).indexOf('-')+1);
        break;
      case STATE_MISWIRE:
        s = "W&" + getLabel(r.partner+1).substring(getLabel(r.partner+1).indexOf('-')+1);
        break;
      default: s = "E"; break;
    }
    char buf[40];
  snprintf(buf, sizeof(buf), "%s:%s,", label.c_str(), s.c_str());
    outputBuffer += buf;
      yield();
  }
}

void runQC_K2(String &outputBuffer) {
  const int startCh = 26, endCh = 52;  // global 0–25
  ChannelResult results[26];
  bool globalMap[76] = {0};

  // Inisialisasi
  for (int i = startCh; i < endCh; i++) {
    results[i - startCh].state   = -1;
    results[i - startCh].partner = -1;
  }

  for (int ch = startCh; ch < endCh; ch++) {
    if (!channelUsed[ch]) {
      results[ch - startCh].state = -1;  // skip
       yield();
      continue;
    }

    // 1) Shift sinyal di bit ch
    shiftOut80(1ULL << ch);
     yield();
    delayMicroseconds(5);

    // 2) Baca semua MUX (tapi kita akan evaluasi subset)
    memset(globalMap, 0, sizeof(globalMap));
    readAllMux(globalMap, 76);

    // 3) Hitung totalActive & selfActive DALAM rentang K1
    int totalActive = 0;
    for (int i = startCh; i < endCh; i++)
      totalActive += globalMap[i];
    bool selfActive = globalMap[ch];

    // 4) Tentukan state & partner hanya dalam grup ini
    if (selfActive) {
      if (totalActive == 1) {
        results[ch - startCh].state = STATE_CONNECTED;
      } else {
        results[ch - startCh].state = STATE_SHORT;
        // cari partner dalam range startCh..endCh
        for (int i = startCh; i < endCh; i++) {
          if (i != ch && globalMap[i]) {
            results[ch - startCh].partner = i;
            break;
          }
        }
      }
    } else {
      if (totalActive > 0) {
        results[ch - startCh].state = STATE_MISWIRE;
        for (int i = startCh; i < endCh; i++) {
          if (globalMap[i]) {
            results[ch - startCh].partner = i;
            break;
          }
        }
      } else {
        results[ch - startCh].state = STATE_DISCONNECTED;
      }
    }
  }

  {
    // Dalam array results, index 0..12 = K2-A1..K2-A13, index 13..25 = K2-B1..K2-B13
    const int idxA10  = 9;   // K2-B2  → results[14]
    const int idxB13 = 13 + (13 - 1);  // K2-B13 → results[25]

    // Cek kondisi: B2 short dengan B13, sekaligus B3 disconnected
    if ( results[idxA10].state   == STATE_SHORT
      && results[idxA10].partner == (startCh + idxB13)
      && results[idxB13].state   == STATE_DISCONNECTED
    ) {
      // Tandai B2 dan B13 sebagai connected
      results[idxA10].state   = STATE_CONNECTED;
      results[idxB13].state  = STATE_CONNECTED;
      // Hapus partner agar tidak tercetak "S&..." atau "W&..."
      results[idxA10].partner  = -1;
      results[idxB13].partner = -1;
    }
     yield();
  }

  // 5) Cetak hasil untuk K1 saja
  for (int i = startCh; i < endCh; i++) {
    String label = getLabel(i+1);
    String s;
    ChannelResult &r = results[i - startCh];
    switch (r.state) {
      case STATE_CONNECTED:    s = "C"; break;
      case STATE_DISCONNECTED: s = "N"; break;
      case STATE_SHORT:
        s = "S&" + getLabel(r.partner+1).substring(getLabel(r.partner+1).indexOf('-')+1);
        break;
      case STATE_MISWIRE:
        s = "W&" + getLabel(r.partner+1).substring(getLabel(r.partner+1).indexOf('-')+1);
        break;
      default: s = "E"; break;
    }
    char buf[40];
snprintf(buf, sizeof(buf), "%s:%s,", label.c_str(), s.c_str());
    outputBuffer += buf;
    yield();
  }
}

void runQC_K3(String &outputBuffer) {
  const int startCh = 52, endCh = 76;  
  ChannelResult results[26];
  bool globalMap[76] = {0};

  // Inisialisasi
  for (int i = startCh; i < endCh; i++) {
    results[i - startCh].state   = -1;
    results[i - startCh].partner = -1;
  }

  for (int ch = startCh; ch < endCh; ch++) {
    if (!channelUsed[ch]) {
      results[ch - startCh].state = -1;  // skip
      continue;
    }

    // 1) Shift sinyal di bit ch
    shiftOut80(1ULL << ch);
    delayMicroseconds(5);

    // 2) Baca semua MUX (tapi kita akan evaluasi subset)
    memset(globalMap, 0, sizeof(globalMap));
    readAllMux(globalMap, 76);

    // 3) Hitung totalActive & selfActive DALAM rentang K1
    int totalActive = 0;
    for (int i = startCh; i < endCh; i++)
      totalActive += globalMap[i];
    bool selfActive = globalMap[ch];

    // 4) Tentukan state & partner hanya dalam grup ini
    if (selfActive) {
      if (totalActive == 1) {
        results[ch - startCh].state = STATE_CONNECTED;
      } else {
        results[ch - startCh].state = STATE_SHORT;
        // cari partner dalam range startCh..endCh
        for (int i = startCh; i < endCh; i++) {
          if (i != ch && globalMap[i]) {
            results[ch - startCh].partner = i;
            break;
          }
        }
      }
    } else {
      if (totalActive > 0) {
        results[ch - startCh].state = STATE_MISWIRE;
        for (int i = startCh; i < endCh; i++) {
          if (globalMap[i]) {
            results[ch - startCh].partner = i;
            break;
          }
        }
      } else {
        results[ch - startCh].state = STATE_DISCONNECTED;
      }
    }
  }
    // --- SPECIAL CASE: K3A5 disconnected & K3B1 short→A5 ⇒ both connected ---
  {
    const int startCh = 52;           // K3 group mulai di global index 52 (channel 53)
    const int idxA5   = 4;            // offset K3A5: (57−1)−52 = 4
    const int idxB1   = 12;           // offset K3B1: (65−1)−52 = 12
    // Jika A5 putus dan B1 short ke A5
    if ( results[idxA5].state   == STATE_DISCONNECTED
      && results[idxB1].state   == STATE_SHORT
      && results[idxB1].partner == (startCh + idxA5)
    ) {
      // Tanda kedua channel sebagai connected
      results[idxA5].state   = STATE_CONNECTED;
      results[idxB1].state   = STATE_CONNECTED;
      // Clear partner agar tidak tercetak S&... atau W&...
      results[idxA5].partner = -1;
      results[idxB1].partner = -1;
    }
     yield();
  }
  // 5) Cetak hasil untuk K1 saja
  for (int i = startCh; i < endCh; i++) {
    String label = getLabel(i+1);
    String s;
    ChannelResult &r = results[i - startCh];
    switch (r.state) {
      case STATE_CONNECTED:    s = "C"; break;
      case STATE_DISCONNECTED: s = "N"; break;
      case STATE_SHORT:
        s = "S&" + getLabel(r.partner+1).substring(getLabel(r.partner+1).indexOf('-')+1);
        break;
      case STATE_MISWIRE:
        s = "W&" + getLabel(r.partner+1).substring(getLabel(r.partner+1).indexOf('-')+1);
        break;
      default: s = "E"; break;
    }
    char buf[40];
snprintf(buf, sizeof(buf), "%s:%s,", label.c_str(), s.c_str());
    outputBuffer += buf;
    yield();
  }
}


// ===================================================
// Run QC untuk 8 channel “X” (MUX6),
// dengan deteksi antar-MUX (tetap pada 88 global)
// ===================================================
void runQCX(String &outputBuffer) {
  struct ChannelResult { int state, partner; };
  ChannelResult resultsX[8];

  for (int i = 0; i < 8; i++) {
    resultsX[i].state   = -1;
    resultsX[i].partner = -1;
  }

  for (int lx = 0; lx < 8; lx++) {
    int ch = 80 + lx;

    uint8_t buf[11] = {0};
    buf[ch/8] = 1 << (ch % 8);
    shiftOut88(buf);
    delayMicroseconds(5);

    bool globalMap[88] = {0};
    // MUX1–5
    for (int m = 1; m <= 5; m++) {
      uint16_t d16 = readMux16(m);
      for (int b = 0; b < 16; b++) {
        globalMap[(m-1)*16 + b] = (d16 >> b) & 1;
      }
    }
    // MUX6
    uint16_t dx = readMux16X();
    for (int b = 0; b < 8; b++) {
      globalMap[80 + b] = (dx >> b) & 1;
    }

    // 3) Hitung total & selfActive
    int totalActive = 0;
    for (int i = 0; i < 88; i++) totalActive += globalMap[i];
    bool selfActive = globalMap[ch];

    // 4) Tentukan state & partner
    if (selfActive) {
      if (totalActive == 1) {
        resultsX[lx].state = STATE_CONNECTED;
      } else {
        resultsX[lx].state = STATE_SHORT;
        for (int i = 0; i < 88; i++) {
          if (i != ch && globalMap[i]) {
            resultsX[lx].partner = i;
            break;
          }
        }
      }
    } else {
      if (totalActive > 0) {
        resultsX[lx].state = STATE_MISWIRE;
        for (int i = 0; i < 88; i++) {
          if (globalMap[i]) {
            resultsX[lx].partner = i;
            break;
          }
        }
      } else {
        resultsX[lx].state = STATE_DISCONNECTED;
      }
    }
  }

  // 5) Cetak hasil X
  for (int lx = 0; lx < 8; lx++) {
    int ch = 80 + lx;
    String label = getLabelX(lx);
    String s;
    switch (resultsX[lx].state) {
      case STATE_CONNECTED:    s = "C"; break;
      case STATE_DISCONNECTED: s = "N"; break;
      case STATE_SHORT: {
        int p = resultsX[lx].partner;
        String pl = (p < 80) ? getLabel(p+1) : getLabelX(p-80);
        s = "S&" + pl.substring(pl.indexOf('-')+1);
        break;
      }
      case STATE_MISWIRE: {
        int p = resultsX[lx].partner;
        String pl = (p < 80) ? getLabel(p+1) : getLabelX(p-80);
        s = "W&" + pl.substring(pl.indexOf('-')+1);
        break;
      }
      default: s = "E"; break;
    }
    char buf[40];
snprintf(buf, sizeof(buf), "%s:%s,", label.c_str(), s.c_str());
    outputBuffer += buf;
    yield();
  }
}



//
// Fungsi shift register untuk 88 bit (untuk kabel X)
// menggunakan array 11 byte
//
void shiftOut88(uint8_t data[11]) {
  digitalWrite(STCP, LOW);
  for (int i = 87; i >= 0; i--) {
    digitalWrite(SHCP, LOW);
    int byteIndex = i / 8;     
    int bitIndex = i % 8;
    uint8_t bitVal = (data[byteIndex] >> bitIndex) & 1;
    digitalWrite(DS, bitVal);
    digitalWrite(SHCP, HIGH);
    delayMicroseconds(10);
  }
  digitalWrite(STCP, HIGH);
  delayMicroseconds(20);
}

// Membaca MUX 6 untuk kabel X
uint16_t readMux16X() {
  uint16_t muxData = 0;
  // Pastikan MUX lainnya non-aktif
  digitalWrite(EN_MUX1, HIGH);
  digitalWrite(EN_MUX2, HIGH);
  digitalWrite(EN_MUX3, HIGH);
  digitalWrite(EN_MUX4, HIGH);
  digitalWrite(EN_MUX5, HIGH);
  // Aktifkan MUX 6
  digitalWrite(EN_MUX6, LOW);
  
  for (int i = 0; i < 16; i++) {
    digitalWrite(S0, i & 0x01);
    digitalWrite(S1, (i >> 1) & 0x01);
    digitalWrite(S2, (i >> 2) & 0x01);
    digitalWrite(S3, (i >> 3) & 0x01);
    delayMicroseconds(10);
    
    pinMode(SIG, INPUT_PULLDOWN);
    int val = digitalRead(SIG);
    if (val == HIGH) {
      muxData |= (1 << i);
    }
  }
  return muxData;
}

// Menghasilkan label untuk kabel X (X1 sampai X8)
String getLabelX(int localChannel) {
  return "X-" + String(localChannel + 1);
}

//
// Fungsi runQCX() untuk kabel X (channel 80-87)
// tanpa sampling iteratif (mengambil pembacaan satu kali)
//
//
// Setup dan Loop utama
//
void setup() {
  pinMode(DS, OUTPUT);
  pinMode(SHCP, OUTPUT);
  pinMode(STCP, OUTPUT);
  
  pinMode(SIG, INPUT_PULLDOWN);
  
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(EN_MUX1, OUTPUT);
  pinMode(EN_MUX2, OUTPUT);
  pinMode(EN_MUX3, OUTPUT);
  pinMode(EN_MUX4, OUTPUT);
  pinMode(EN_MUX5, OUTPUT);
  pinMode(EN_MUX6, OUTPUT);  
  
  digitalWrite(EN_MUX1, LOW);
  digitalWrite(EN_MUX2, LOW);
  digitalWrite(EN_MUX3, LOW);
  digitalWrite(EN_MUX4, LOW);
  digitalWrite(EN_MUX5, LOW);
  digitalWrite(EN_MUX6, HIGH); 

  Serial.begin(115200);
  SerialBT.begin("QC Cable");
  delay(1000);                       
}

bool qcRunning   = false;       
unsigned long lastQC     = 0;    
const unsigned long qcInterval = 10;  

void loop() {
  static String bufUSB = "";
  static String bufBT  = "";


  while (Serial.available()) {
    char c = Serial.read();
    bufUSB += c;
  }

  while (SerialBT.available()) {
    char c = SerialBT.read();
    bufBT += c;
  }

  if ( bufUSB.indexOf("startall") >= 0 || bufBT.indexOf("startall") >= 0 ) {
    qcRunning   = true;
    qcAllActive = true;
    qcActive    = false;
    qcActiveX   = false;
    bufUSB = ""; 
    bufBT  = "";
  }
  else if ( bufUSB.indexOf("startx")   >= 0 || bufBT.indexOf("startx")   >= 0 ) {
    qcRunning   = true;
    qcActiveX   = true;
    qcActive    = false;
    qcAllActive = false;
    bufUSB = "";
    bufBT  = "";
  }
  else if ( bufUSB.indexOf("start")    >= 0 || bufBT.indexOf("start")    >= 0 ) {
    qcRunning   = true;
    qcActive    = true;
    qcAllActive = false;
    qcActiveX   = false;
    bufUSB = "";
    bufBT  = "";
  }
  else if ( bufUSB.indexOf("stop")     >= 0 || bufBT.indexOf("stop")     >= 0 ) {
    qcRunning   = false;
    qcAllActive = qcActive = qcActiveX = false;
    resetPins();
    bufUSB = "";
    bufBT  = "";
  }


  if (qcRunning) {
    unsigned long now = millis();
    if (now - lastQC >= qcInterval) {
      lastQC = now;

      String out = "";
      if (qcAllActive || qcActive) {
        runQC_K1(out);
        runQC_K2(out);
        runQC_K3(out);
      }
      if (qcAllActive || qcActiveX) {
        digitalWrite(EN_MUX6, LOW);
        runQCX(out);
        digitalWrite(EN_MUX6, HIGH);
      }
      Serial.println(out);
      SerialBT.println(out);
    }
  }
}
