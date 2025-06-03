**README.md**

---

## QC Cable Tester Berbasis ESP32 + Bluetooth

**Deskripsi Singkat**
Proyek ini adalah implementasi sistem Quality Control (QC) untuk pengujian konektivitas kabel, menggunakan modul ESP32, rangkaian shift register 80/88-bit, multiplexer (MUX) hingga 6 buah, dan koneksi Bluetooth. Dengan satu rangkaian, Anda dapat memeriksa hingga 76 kanal “normal” dan 8 kanal “X” secara otomatis. Hasil QC (terhubung, putus, short, salah jalur) kemudian dikirim melalui USB Serial dan Bluetooth Serial secara bersamaan dalam format teks.

---

## Daftar Isi

1. [Fitur Utama](#fitur-utama)
2. [Persyaratan Hardware](#persyaratan-hardware)
3. [Skema Pin dan Konektivitas](#skema-pin-dan-konektivitas)
4. [Persiapan Perangkat Lunak](#persiapan-perangkat-lunak)
5. [Cara Mengunggah (Upload) ke ESP32](#cara-mengunggah-upload-ke-esp32)
6. [Perintah Bluetooth / USB Serial](#perintah-bluetooth--usb-serial)
7. [Penjelasan Fungsi-Fungsi Utama](#penjelasan-fungsi-fungsi-utama)
8. [Format Output QC](#format-output-qc)

---

## Fitur Utama

* **Pengujian 76 Kanal Normal**

  * Memanfaatkan 5 buah MUX 16:1 untuk membaca kondisi setiap pin kabel di kanal 1–76.
  * Deteksi otomatis:

    * `C` (Connected)
    * `N` (Disconnected)
    * `S&<nomor partner>` (Short dengan kanal lain)
    * `W&<nomor partner>` (Miswire / salah jalur)
  * Penanganan kondisi khusus (edge-case) pada grup K2 dan K3 sesuai skema QC kabel di dokumentasi.
* **Pengujian 8 Kanal “X” (MUX6)**

  * Rangkaian tambahan untuk kabel tipe “X” (kanal 80–87) menggunakan MUX6.
  * Deteksi antar-MUX: apabila kanal X terhubung / short / miswire ke salah satu dari 88 global bit (76 kanal normal + 8 X).
* **Bluetooth Serial + USB Serial**

  * Nama perangkat Bluetooth: **“QC Cable”**
  * Hasil pengukuran QC langsung dikirim ke kedua antarmuka (Serial USB & Bluetooth).
  * Komunikasi satu arah (ESP32 → PC / Handphone) untuk log hasil QC.
* **Mode QC Fleksibel**

  * `start` : hanya QC kabel normal (1–76)
  * `startx`: hanya QC kabel X (80–87)
  * `startall`: QC semua secara bersamaan (1–76 dan 80–87)
  * `stop` : menghentikan pengukuran QC dan mereset pin

---

## Persyaratan Hardware

1. **ESP32** (atau varian serupa dengan fitur BluetoothClassic)
2. **Shift Register 80-bit / 88-bit**

   * Anda dapat menyusun rangkaian shift register (misalnya menggunakan beberapa IC 74HC595 beruntun) untuk total 80 bit.
   * Untuk skema kabel “X” (8 kanal tambahan), gunakan shift register hingga 88 bit.
3. **Multiplexer (MUX)**

   * 5 buah MUX 16:1 (misal CD74HC4067 atau sejenis). Masing-masing MUX menangani 16 bit sinyal dari shift register untuk 76 kanal normal (5×16 = 80, gunakan 76).
   * 1 buah MUX 16:1 untuk kabel “X” (kanal 80–87).
4. **Kabel Jumper / Breadboard**
5. **Catu Daya 5V (untuk MUX & Shift Register)**
6. **Kabel Uji / Fixture**

   * Sambungkan kabel-kabel yang akan diuji ke pin output shift register / MUX sesuai diagram.
7. **Komputer / Smartphone**

   * Untuk menerima data QC via USB Serial atau Bluetooth Serial.

---

## Skema Pin dan Konektivitas

```
ESP32                              Shift Register / MUX / Fixture
--------------------------------------------------------------------------------
GPIO 23   (DS)      → Data (SER) pada shift register
GPIO 18   (SHCP)    → Clock (SRCLK) pada shift register
GPIO 5    (STCP)    → Latch (RCLK) pada shift register

GPIO 26   (SIG)     ← Output “sense” (hasil pembacaan) dari MUX (digitalRead)
GPIO 13   (S0)      → Select bit 0 pada semua MUX (S0)
GPIO 12   (S1)      → Select bit 1 pada semua MUX (S1)
GPIO 14   (S2)      → Select bit 2 pada semua MUX (S2)
GPIO 19   (S3)      → Select bit 3 pada semua MUX (S3)

GPIO 25   (EN_MUX1) → Enable MUX1 (untuk kanal 1–16)
GPIO 33   (EN_MUX2) → Enable MUX2 (untuk kanal 17–32)
GPIO 15   (EN_MUX3) → Enable MUX3 (untuk kanal 33–48)
GPIO 2    (EN_MUX4) → Enable MUX4 (untuk kanal 49–64)
GPIO 4    (EN_MUX5) → Enable MUX5 (untuk kanal 65–80)

GPIO 27   (EN_MUX6) → Enable MUX6 (untuk kabel X, kanal 80–87)
```

* **Catatan Channel Mapping**

  * Kanall 1–13 → `K1-A1` … `K1-A13`
  * Kanall 14–26 → `K1-B1` … `K1-B13`
  * Kanall 27–39 → `K2-A1` … `K2-A13`
  * Kanall 40–52 → `K2-B1` … `K2-B13`
  * Kanall 53–64 → `K3-A1` … `K3-A12`
  * Kanall 65–76 → `K3-B1` … `K3-B12`
  * Kabel “X” (8 kanal) → `X-1` … `X-8`

---

## Persiapan Perangkat Lunak

1. **Install Arduino IDE (versi terbaru disarankan)**

   * Bisa diunduh di [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software) (sesuaikan OS)
2. **Tambahkan Board ESP32 ke Arduino IDE**

   * Buka **File → Preferences**
   * Tambahkan URL berikut ke `Additional Boards Manager URLs`:

     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   * Buka **Tools → Board → Boards Manager**, cari “esp32” lalu install “esp32 by Espressif Systems”.
3. **Library yang Diperlukan**

   * `BluetoothSerial.h` (sudah tersedia saat menginstal board ESP32)
   * Tidak ada library eksternal lain (semua fungsi shift register dan multiplexer ditulis manual).

---

## Cara Mengunggah (Upload) ke ESP32

1. Sambungkan ESP32 ke komputer via kabel USB.
2. Pastikan di **Tools → Board** sudah terbaca **“ESP32 Dev Module”** (atau tipe ESP32 yang Anda gunakan).
3. Pilih **Port** yang sesuai (Tools → Port).
4. Buka file `QC_Cable.ino` (atau ganti nama menjadi `QC_Cable.ino`) pada Arduino IDE.
5. Tekan tombol **Upload** (ikon panah kanan). Tunggu hingga proses selesai, ESP32 otomatis restart.

---

## Perintah Bluetooth / USB Serial

Setelah kode berhasil berjalan, ESP32 akan menunggu perintah via USB Serial (baud rate 115200) ataupun Bluetooth Serial (“QC Cable”).
Ketik salah satu perintah berikut (tanpa tanda kutip) lalu tekan **Enter**:

* `start`

  * Menjalankan QC hanya untuk **kanal normal** (1–76).
* `startx`

  * Menjalankan QC hanya untuk **kanal “X”** (80–87).
* `startall`

  * Menjalankan QC untuk **semua kanal** (1–76 dan 80–87).
* `stop`

  * Menghentikan QC, mereset pin, dan berhenti mengirim data.

**Contoh penggunaan di Serial Monitor:**

```
> startall
```

ESP32 akan mengirimkan hasil QC berulang setiap interval 10 ms ke Serial/BT.

---

## Penjelasan Fungsi-Fungsi Utama

1. **`resetPins()`**

   * Mengatur semua pin ESP32 (DS, SHCP, STCP, SIG, S0–S3, EN\_MUX1–EN\_MUX6) ke `OUTPUT` atau `INPUT_PULLDOWN` sesuai fungsinya.
   * Memastikan initial state: latch low, clock low, data low, MUX6 (EN\_MUX6) di-`HIGH` (non-aktif).

2. **`shiftOut80(uint64_t data)`**

   * Mengirim 76 bit (dipadatkan menjadi 80 bit) ke rangkaian shift register secara paling signifikan (MSB) terlebih dahulu.
   * `data` berupa `uint64_t` dimana bit ke-0 (LSB) → kanal terakhir (nomor 75).

3. **`readMux16(int muxNumber)`**

   * Membaca 16 bit data dari MUX tertentu (1–5) dengan mengaktifkan satu `EN_MUXx` pada `LOW` dan membaca pin SIG pada setiap kombinasi `S0–S3`.
   * Mengembalikan `uint16_t` yang menandakan status HIGH/LOW pada 16 output MUX.

4. **`readAllMux(bool globalMap[], int maxCh)`**

   * Membaca MUX1–5 sekaligus, memasukkan hasilnya ke array `globalMap[]` (panjang minimal `maxCh`).
   * Hanya mengisi hingga indeks `maxCh−1`.

5. **`runQC_K1(String &outputBuffer)`**, **`runQC_K2(...)`**, **`runQC_K3(...)`**

   * Masing-masing menangani QC pada grup kabel:

     * **K1** = kanal global 0–25 (K1-A1..K1-A13, K1-B1..K1-B13)
     * **K2** = kanal global 26–51 (K2-A1..K2-A13, K2-B1..K2-B13)
     * **K3** = kanal global 52–75 (K3-A1..K3-A12, K3-B1..K3-B12)
   * Setiap fungsi:

     1. **Inisialisasi** array `ChannelResult results[...]` (state & partner).
     2. Untuk tiap channel `ch` di kelompoknya:

        * Jika `channelUsed[ch]` = `false`, tandai state = −1 (skip).
        * Lakukan `shiftOut80(1ULL << ch)`, delay, `readAllMux(...)` ke `globalMap[]`.
        * Hitung `totalActive` dalam kelompok tersebut.
        * Tentukan `state` (CONNECTED, SHORT, MISWIRE, DISCONNECTED) dan `partner` (jika SHORT/MISWIRE).
        * Penanganan **kondisi khusus**:

          * **K2**

            * Jika B2 (index internal 10) short ke B13 (index internal 25), dan B13 dianggap putus, tandai kedua channel sebagai CONNECTED.
          * **K3**

            * Jika A5 (index internal 4) putus, dan B1 (index internal 12) short ke A5, tandai keduanya sebagai CONNECTED.
     3. **Cetak hasil**: untuk tiap channel, bentuk string `"Label:State,"` (contoh: `K1-A1:C,` atau `K2-B5:W&B12,`). Tambahkan ke `outputBuffer`.

6. **`shiftOut88(uint8_t data[11])`**

   * Serupa `shiftOut80`, tetapi menggunakan array 11 byte untuk total 88 bit (1 bit extra).

7. **`readMux16X()`**

   * Membaca MUX6 (kanal X) dengan mengaktifkan `EN_MUX6 = LOW` dan membaca 16 bit, meski kanal X hanya 8 bit (bit 0–7).

8. **`runQCX(String &outputBuffer)`**

   * Menangani QC untuk 8 kanal X (global bit 80–87).
   * Algoritmanya mirip `runQC_K*`, tetapi mencakup pembacaan **seluruh 88 bit** (MUX1–5 + MUX6) untuk deteksi antar-MUX.
   * Hasil (“X-1\:C,” atau “X-3\:S\&K2-A5,” dst.) ditambahkan ke `outputBuffer`.

9. **`setup()`**

   * Inisialisasi pin dan `Serial.begin(115200)`, `SerialBT.begin("QC Cable")`.
   * `delay(1000)` singkat agar Bluetooth siap.

10. **`loop()`**

    * Terus-menerus:

      * Baca karakter dari `Serial` (USB) & `SerialBT` (Bluetooth), kumpulkan ke `bufUSB` / `bufBT`.
      * Jika ditemukan substring:

        * `"startall"`, `"startx"`, `"start"`, `"stop"` → setel `qcRunning`, `qcActive`, `qcActiveX`, `qcAllActive` sesuai mode.
      * Jika `qcRunning == true` dan interval (10 ms) tercapai:

        * Panggil `runQC_K1()`, `runQC_K2()`, `runQC_K3()` apabila mode `qcActive`/`qcAllActive`.
        * Panggil `runQCX()` apabila mode `qcActiveX`/`qcAllActive`.
        * Kirim `out` (`String` hasil QC) melalui `Serial.println(out)` dan `SerialBT.println(out)`.

---

## Format Output QC

Hasil QC dikomunikasikan dalam satu baris teks, misalnya:

```
K1-A1:C,K1-A2:N,K1-A3:S&K1-B5,...,K3-B12:W&K3-A8,X-1:C,X-2:N,X-3:S&K2-B4,...
```

* **Format umum per-kanal:**

  ```
  <Label>:<State>[&<LabelPartner>],
  ```

  * `<Label>`

    * Contoh: `K1-A1`, `K2-B7`, `K3-A12`, `X-5`
  * `<State>`

    * `C` = Connected (terhubung sempurna satu-ke-satu)
    * `N` = Disconnected (putus, tidak terdeteksi sama sekali)
    * `S&<LabelPartner>` = Short (terhubung dengan lebih dari satu bit; partner ditunjukkan)
    * `W&<LabelPartner>` = Miswire (sinyal keluar dari kelompok yang diharapkan)
  * `<LabelPartner>`

    * Menunjukkan label kanal lain yang terhubung / short / miswire. Pada `S&`/`W&`, `LabelPartner` hanya substring setelah tanda “-” (misal `B5`, `A12`).

**Contoh Lengkap**

```
K1-A1:C,K1-A2:N,K1-A3:S&K1-B5,K1-A4:W&K2-A1,...,K3-B12:C,X-1:N,X-2:C,X-3:S&K1-A7,...
```

---

### Catatan Tambahan

* Pastikan bahwa **`channelUsed[]`** diinisialisasi sesuai kebutuhan. Nilai `false` akan menyebabkan pemrosesan kanal tersebut di-skip. Anda dapat menonaktifkan atau mengaktifkan kanal tertentu dengan mengubah array `channelUsed[76]` pada baris awal kode.
* Jika Anda ingin menyesuaikan interval QC (delay antara iterasi), ubah konstanta `const unsigned long qcInterval = 10;` (nilai dalam milidetik).
* Penanganan edge-case untuk K2 dan K3 ditujukan untuk kondisi khusus kabel tertentu (misal susunan “B2 short ke B13” atau “A5 disconnected & B1 short ke A5”). Silakan sesuaikan logika ini jika topologi kabel Anda berbeda.

