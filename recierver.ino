#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Konfigurasi OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
// Ganti 0x3C jika alamat I2C OLED Anda berbeda
#define OLED_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int outputPin = D8;   // Pin buzzer

// Struktur data harus SAMA PERSIS dengan Transmitter
typedef struct struct_message {
  bool state;      
  float distance;   
} struct_message;

struct_message receivedData;

// Variabel untuk kontrol buzzer non-blocking
unsigned long previousMillis = 0;
bool buzzerState = false;
int buzzerInterval = 0;

// Variabel untuk animasi dan status koneksi (STATE MACHINE CONTROL)
bool isConnected = false;
unsigned long lastDataReceived = 0;
int animationFrame = 0;
unsigned long lastAnimationUpdate = 0;
unsigned long connectionAnimStart = 0; // Waktu mulai animasi koneksi 2 detik
bool showConnectionAnim = false;      // Flag untuk menampilkan animasi koneksi

// Fungsi untuk menghitung interval buzzer berdasarkan jarak
int calculateBuzzerInterval(float distance) {
  if (distance = 1.7) {
    return 0;   // mati error data 
  }else if (distance < 10){
    return 100; // SAngat cepat (1-10 cm)
  } else if (distance < 20) {
    return 200;   // Cepat (10-20 cm)
  } else if (distance < 30) {
    return 300;   // Sedang (20-30 cm)
  } else if (distance < 50) {
    return 500;   // Lambat (30-50 cm)
  } else if (distance < 100) {
    return 800;   // Sangat lambat (50-100 cm)
  } else {
    return 0;     // Mati (>100 cm)
  }
}

// Fungsi untuk menampilkan animasi koneksi
void displayConnectionAnimation() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.println("CONNECTED");
  
  // Animasi loading bar
  // animationFrame akan dihitung di loop()
  int barWidth = (animationFrame % 100) * SCREEN_WIDTH / 100;
  display.fillRect(0, 40, barWidth, 8, SSD1306_WHITE);
  display.drawRect(0, 40, SCREEN_WIDTH, 8, SSD1306_WHITE);
  
  display.display();
}

// Fungsi untuk menampilkan data jarak
void displayDistance() {
  display.clearDisplay();
  
  // Header
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("BISSMILLAH");
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
  
  // Status koneksi
  display.setCursor(0, 15);
  display.print("Status: ");
  // Tampilkan ACTIVE jika data diterima dalam 3 detik terakhir
  if (millis() - lastDataReceived < 3000) {
    display.println("ACTIVE");
  } else {
    display.println("WAITING");
  }
  
  // Tampilkan jarak
  display.setTextSize(2);
  display.setCursor(0, 30);
  display.print("D: ");
  display.print(receivedData.distance, 1);
  display.println("cm");
  
  // Teks status bahaya/jarak
  display.setTextSize(1);
  display.setCursor(0, 52);
  
  float dist = receivedData.distance;
  if (dist < 10) {
    display.print("BERHENTI!!!!");
  } else if (dist < 50) {
    display.print("HAMPIRR");
  } else if (dist < 100) {
    display.print("MASIH AMAN");
  } else {
    display.print("MASIH JAUH");
  }

  // Bar grafik jarak (max 200cm)
  int barLength = map(constrain(dist, 0, 200), 0, 200, 0, SCREEN_WIDTH);
  display.fillRect(0, 60, barLength, 4, SSD1306_WHITE);
  display.drawRect(0, 60, SCREEN_WIDTH, 4, SSD1306_WHITE);
  
  display.display();
}

// Fungsi untuk menampilkan screen menunggu koneksi
void displayWaiting() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 10);
  display.println("STILL");
  
  display.setTextSize(2);
  display.setCursor(10, 25);
  display.println("WAITING");
  
  // Animasi titik-titik (non-blocking)
  display.setTextSize(1);
  display.setCursor(35, 45);
  int dots = (millis() / 500) % 4; // Update setiap 500ms
  for (int i = 0; i < dots; i++) {
    display.print(".");
  }
  
  display.display();
}

// Callback ketika data diterima
// FUNGSI INI HARUS SINGKAT DAN NON-BLOCKING
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  // Salin data masuk ke struct lokal
  memcpy(&receivedData, incomingData, sizeof(receivedData));
  
  lastDataReceived = millis();
  
  // Pindahkan logika animasi ke loop()
  if (!isConnected) {
    isConnected = true;
    connectionAnimStart = millis(); // Catat waktu mulai animasi koneksi
    showConnectionAnim = true;      // Set flag untuk menampilkan animasi
  }
  
  Serial.print("Data diterima. Distance: ");
  Serial.print(receivedData.distance, 1);
  Serial.println(" cm");
  
  // Hitung interval buzzer berdasarkan jarak
  if (receivedData.state == HIGH) {
    buzzerInterval = calculateBuzzerInterval(receivedData.distance);
  } else {
    buzzerInterval = 0;
    digitalWrite(outputPin, LOW);
    buzzerState = false;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(outputPin, OUTPUT);
  digitalWrite(outputPin, LOW);
  
  // Inisialisasi OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  // Tampilan Inisialisasi
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Initializing...");
  display.display();
  delay(1000);
  
  // Set WiFi mode sebagai Station (STA) untuk ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  Serial.println("\n========================================");
  Serial.println("ESP-NOW RECEIVER");
  Serial.print("MAC Address Receiver: ");
  Serial.println(WiFi.macAddress());
  Serial.println("========================================");
  
  // Tampilkan MAC Address di OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("MAC Address:");
  display.setCursor(0, 15);
  display.println(WiFi.macAddress()); // Tampilkan MAC address yang sebenarnya
  display.setCursor(0, 35);
  display.println("Copy to TX!");
  display.display();
  delay(3000);
  
  // Inisialisasi ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error: Gagal inisialisasi ESP-NOW");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("ESP-NOW FAILED!");
    display.display();
    return;
  }
  
  Serial.println("ESP-NOW berhasil diinisialisasi");
  
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);
  
  displayWaiting(); // Tampilkan layar waiting pertama kali
}

void loop() {
  unsigned long currentMillis = millis();
  
  // 1. Kontrol Buzzer (NON-BLOCKING)
  // Buzzer berbunyi hanya jika buzzerInterval > 0
  if (buzzerInterval > 0) {
    if (currentMillis - previousMillis >= buzzerInterval) {
      previousMillis = currentMillis;
      
      // Toggle buzzer state
      buzzerState = !buzzerState;
      digitalWrite(outputPin, buzzerState ? HIGH : LOW);
    }
  } else {
    // Pastikan buzzer mati jika interval 0 (jarak aman/state LOW)
    digitalWrite(outputPin, LOW);
    buzzerState = false;
  }
  
  // 2. Cek Timeout Koneksi
  // Jika tidak ada data dalam 5 detik, status koneksi berubah menjadi terputus (false)
  if (isConnected && (currentMillis - lastDataReceived > 1000)) {
    isConnected = false;
    showConnectionAnim = false; // Hentikan animasi jika timeout
    lastAnimationUpdate = 0; 

    //matikan buzzer 
    buzzerInterval = 0;
    digitalWrite(outputPin, LOW);
    buzzerState = false;

    Serial.println("Connection LOsT!");
  }
  
  // 3. Kontrol Tampilan OLED (NON-BLOCKING STATE MACHINE)
  if (showConnectionAnim) {
    // A. Tampilkan Animasi Koneksi (hanya selama 2 detik)
    if (currentMillis - connectionAnimStart < 2000) {
      // Update animasi setiap 50ms
      if (currentMillis - lastAnimationUpdate > 50) { 
        lastAnimationUpdate = currentMillis;
        // Hitung frame berdasarkan waktu yang sudah berlalu
        animationFrame = (currentMillis - connectionAnimStart) / 20; 
        displayConnectionAnimation();
      }
    } else {
      // Setelah 2 detik, animasi selesai dan tampilkan data
      showConnectionAnim = false; 
      displayDistance(); 
    }
  } else if (isConnected) {
    // B. Tampilkan Data Jarak (Selama masih terkoneksi/data baru)
    // Update tampilan setiap 500ms agar display tidak flicker berlebihan
    if (currentMillis - lastAnimationUpdate > 500) {
        lastAnimationUpdate = currentMillis;
        displayDistance();
    }
  } else {
    // C. Tampilkan Screen Waiting (Jika belum terkoneksi atau timeout)
    // Update animasi waiting setiap 500ms
    if (currentMillis - lastAnimationUpdate > 500) { 
      lastAnimationUpdate = currentMillis;
      displayWaiting();
    }
  }

  // Penting: Hilangkan atau perkecil delay di loop()
  // delay(1) sudah cukup untuk membantu background tasks berjalan
  delay(1); 
}