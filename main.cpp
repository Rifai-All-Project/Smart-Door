#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>  // ← tambah ini

// =====================
//  Pin Definition
// =====================
#define SS_PIN    5
#define RST_PIN   2
#define SERVO_PIN 13
#define LED_HIJAU 26
#define LED_MERAH 27
#define BUZZER    14
#define EEPROM_SIZE 10  // ← ukuran EEPROM yang dipakai

// =====================
//  LCD I2C
// =====================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================
//  Keypad 4x4
// =====================
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {32, 33, 25, 15};
byte colPins[COLS] = {16, 17, 12, 4};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// =====================
//  Object
// =====================
MFRC522 rfid(SS_PIN, RST_PIN);
Servo myServo;

// =====================
//  Kredensial
// =====================
byte allowedUID[] = {0xB9, 0x8C, 0xD1, 0x06};
const byte UID_SIZE = 4;
String correctPIN = "1234"; // PIN default awal

// =====================
//  Fungsi EEPROM
// =====================
void simpanPIN(String pin) {
  for (int i = 0; i < 4; i++) {
    EEPROM.write(i, pin[i]);
  }
  EEPROM.commit(); // wajib di ESP32!
  Serial.println(">> PIN tersimpan ke EEPROM");
}

String bacaPIN() {
  String pin = "";
  for (int i = 0; i < 4; i++) {
    char c = (char)EEPROM.read(i);
    // Validasi: pastikan isinya angka
    if (c >= '0' && c <= '9') {
      pin += c;
    } else {
      // EEPROM kosong/korup → pakai default
      return "1234";
    }
  }
  return pin;
}

// =====================
//  Prototypes
// =====================
bool cekUID();
void aksesBenar(String metode);
void aksesSalah(String pesan);
void buzzerBeep(int jumlah, int durasi, int jeda);
void modePIN();
void gantiPIN();
void tampilIdle();
String inputPINHelper(String judul);

// =====================
//  Setup
// =====================
void setup() {
  Serial.begin(115200);

  // Init EEPROM
  EEPROM.begin(EEPROM_SIZE);

  // Baca PIN dari EEPROM
  correctPIN = bacaPIN();
  Serial.print(">> PIN aktif: ");
  Serial.println(correctPIN);

  keypad.setDebounceTime(100);
  keypad.setHoldTime(500);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  SPI.begin(18, 19, 23, 5);
  rfid.PCD_Init();

  byte v = rfid.PCD_ReadRegister(rfid.VersionReg);
  if (v == 0x00 || v == 0xFF) {
    Serial.println("ERROR: RFID tidak terdeteksi!");
  } else {
    Serial.println("RFID OK!");
  }

  myServo.attach(SERVO_PIN);
  myServo.write(0);

  pinMode(LED_HIJAU, OUTPUT);
  pinMode(LED_MERAH, OUTPUT);
  pinMode(BUZZER,    OUTPUT);
  digitalWrite(LED_HIJAU, LOW);
  digitalWrite(LED_MERAH, LOW);
  digitalWrite(BUZZER,    LOW);

  tampilIdle();
}

// =====================
//  Loop
// =====================
void loop() {
  char key = keypad.getKey();

  if (key == '*') {
    modePIN();
    tampilIdle();
    return;
  }

  if (key == 'A') {
    gantiPIN();
    tampilIdle();
    return;
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial())   return;

  Serial.print("UID terdeteksi: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) Serial.print("0");
    Serial.print(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) Serial.print(":");
  }
  Serial.println();

  if (cekUID()) {
    aksesBenar("Kartu");
  } else {
    aksesSalah("Kartu");
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(1000);
  tampilIdle();
}

// =====================
//  Tampilan Idle
// =====================
void tampilIdle() {
  lcd.setCursor(0, 0);
  lcd.print("Silahkan Scan  ");
  lcd.setCursor(0, 1);
  lcd.print("Kartu/Tekan *  ");
}

// =====================
//  Helper input PIN
// =====================
String inputPINHelper(String judul) {
  while (keypad.getKey()) delay(10);
  delay(200);

  String input = "";
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(judul);
  lcd.setCursor(0, 1);
  lcd.print("PIN: ");

  while (true) {
    char k = keypad.getKey();
    if (!k) continue;

    if (k == '#' && input.length() == 4) {
      return input;
    } else if (k == '*') {
      input = "";
      lcd.setCursor(0, 1);
      lcd.print("PIN:           ");
      lcd.setCursor(5, 1);
    } else if (k != '#' && input.length() < 4) {
      input += k;
      lcd.setCursor(4 + input.length(), 1);
      lcd.print("*");
    }
  }
}

// =====================
//  Akses Benar ✅
// =====================
void aksesBenar(String metode) {
  Serial.println(">> Status: AKSES DITERIMA ✓");

  lcd.clear();
  lcd.backlight();
  lcd.setCursor(0, 0);
  if (metode == "Kartu") {
    lcd.print("Kartu Berhasil");
  } else {
    lcd.print("PIN Berhasil");
  }
  lcd.setCursor(0, 1);
  lcd.print("Pintu Terbuka!");

  digitalWrite(LED_HIJAU, HIGH);
  digitalWrite(LED_MERAH, LOW);
  buzzerBeep(1, 300, 0);

  // Pastikan servo mulai dari 0° dulu
  myServo.write(0);
  delay(300);

  // Buka pintu: gerak pelan dari 0° ke 90°
  for (int pos = 0; pos <= 180; pos++) {
    myServo.write(pos);
    delay(15);
  }
  myServo.write(90); // pastikan posisi 90°
  Serial.println(">> Pintu terbuka...");

  // Tahan 3 detik, refresh LCD tiap 1 detik
  for (int i = 0; i < 3; i++) {
    delay(1000);
    lcd.backlight();
    lcd.setCursor(0, 0);
    if (metode == "Kartu") {
      lcd.print("Kartu Berhasil");
    } else {
      lcd.print("PIN Berhasil");
    }
    lcd.setCursor(0, 1);
    lcd.print("Pintu Terbuka!");
  }

  // Update LCD sebelum servo tutup
  myServo.detach();
  delay(200);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Pintu Tertutup");
  delay(500);

  // Tutup pintu: gerak pelan dari 90° ke 0°
  myServo.attach(SERVO_PIN);
  delay(100);
  myServo.write(180); // mulai dari 90°
  delay(200);
  for (int pos = 90; pos >= 0; pos--) {
    myServo.write(pos);
    delay(15);
  }
  myServo.write(0); // pastikan posisi 0°
  Serial.println(">> Pintu tertutup.");

  delay(100);
  digitalWrite(LED_HIJAU, LOW);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Pintu Tertutup");
  delay(1500);
}

// =====================
//  Akses Salah ❌
// =====================
void aksesSalah(String pesan) {
  Serial.println(">> Status: AKSES DITOLAK ✗");

  lcd.clear();
  lcd.setCursor(0, 0);
  if (pesan == "Kartu") {
    lcd.print("Kartu Gagal");
  } else {
    lcd.print("PIN Salah!");
  }
  lcd.setCursor(0, 1);
  lcd.print("Akses Ditolak!");

  digitalWrite(LED_MERAH, HIGH);
  digitalWrite(LED_HIJAU, LOW);
  buzzerBeep(3, 200, 200);

  delay(500);
  digitalWrite(LED_MERAH, LOW);
  delay(1000);
}

// =====================
//  Mode PIN
// =====================
void modePIN() {
  int attempts = 0;
  Serial.println(">> Mode PIN aktif");

  while (true) {
    String inputPIN = inputPINHelper("Masukkan PIN:");

    if (inputPIN == correctPIN) {
      aksesBenar("PIN");
      return;
    } else {
      attempts++;
      Serial.print(">> PIN salah! Sisa: ");
      Serial.println(3 - attempts);
      aksesSalah("PIN");

      if (attempts >= 3) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Terlalu Banyak!");
        lcd.setCursor(0, 1);
        lcd.print("Tunggu 10 detik");
        for (int i = 0; i < 10; i++) {
          digitalWrite(LED_MERAH, HIGH);
          delay(500);
          digitalWrite(LED_MERAH, LOW);
          delay(500);
        }
        return;
      }
    }
  }
}

// =====================
//  Ganti PIN
//  PIN baru disimpan ke EEPROM
//  → tidak hilang meski cabut USB
// =====================
void gantiPIN() {
  Serial.println(">> Mode Ganti PIN");

  // Step 1: PIN lama
  String inputLama = inputPINHelper("PIN Lama:");

  if (inputLama != correctPIN) {
    aksesSalah("PIN");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PIN Lama Salah!");
    lcd.setCursor(0, 1);
    lcd.print("Ganti Dibatal!");
    delay(2000);
    return;
  }

  // Step 2: PIN baru
  String inputBaru = inputPINHelper("PIN Baru:");

  // Step 3: Konfirmasi
  String inputKonfirmasi = inputPINHelper("Konfirmasi PIN:");

  if (inputBaru == inputKonfirmasi) {
    correctPIN = inputBaru;
    simpanPIN(correctPIN); // ← simpan ke EEPROM permanen!

    digitalWrite(LED_HIJAU, HIGH);
    buzzerBeep(1, 300, 0);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PIN Berhasil");
    lcd.setCursor(0, 1);
    lcd.print("Diganti!");
    delay(2000);

    digitalWrite(LED_HIJAU, LOW);
    Serial.print(">> PIN baru: ");
    Serial.println(correctPIN);

  } else {
    aksesSalah("PIN");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PIN Tidak");
    lcd.setCursor(0, 1);
    lcd.print("Cocok! Ulangi.");
    delay(2000);
  }
}

// =====================
//  Cek UID
// =====================
bool cekUID() {
  if (rfid.uid.size != UID_SIZE) return false;
  for (byte i = 0; i < UID_SIZE; i++) {
    if (rfid.uid.uidByte[i] != allowedUID[i]) return false;
  }
  return true;
}

// =====================
//  Helper Buzzer
// =====================
void buzzerBeep(int jumlah, int durasi, int jeda) {
  for (int i = 0; i < jumlah; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(durasi);
    digitalWrite(BUZZER, LOW);
    if (i < jumlah - 1) delay(jeda);
  }
}