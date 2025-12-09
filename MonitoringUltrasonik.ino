#include <ESP8266WiFi.h>
#include <espnow.h>

//pin ultrasonik to esp
const int trgiPin = D7;
const int echoPin = D8;

//tambahkan kalibrasi 
const float offset = 1.7;

//filter 
const int sample = 5;
float readings[sample];
int readIndex = 0;
bool arrayFilled = false;

//parameter sensor 
float distance;
float rawDistance;

//set threshold 
const float batasDistance = 100.0;

uint8_t receiverMAC[] = {0x08, 0x3A, 0x8D, 0xE6, 0xE9, 0x17};

//struktur data yang akan dikirm 
typedef struct struct_message {
  bool state;
  float jarak;
}struct_message;

struct_message myData;

void sendData(uint8_t *mac_addr, uint8_t sendstatus) {
  Serial.print("Status Pengirim = ");
  if (sendstatus == 0) {
    Serial.println("Sukses");
  }
  else {
    Serial.println("Gagal");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(trgiPin, OUTPUT);
  pinMode(echoPin, INPUT);

  //inisialiasi array 
  for (int i  = 0; i < sample; i++) {
    readings[i] = 0;
  }

  //set wifi 
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.println("ULTRASONIK READY!!!");
  Serial.println("WiFi Addres Transmitter: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != 0) {
    Serial.println("Error : gagal terhubung!");
  }

  Serial.println("Sukses!");

  //set role sebgai controler 
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  //register callback 
  esp_now_register_send_cb(sendData);
  //register peer
  esp_now_add_peer(receiverMAC, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);;

  delay(1000);
}

void loop() {
  float SAMPLE = filteredsensor();

  //simpan ke array untuk filter 
  readings[readIndex] = SAMPLE;
  readIndex++;

  if (readIndex >= sample) {
    readIndex =0;
    arrayFilled = true;
  }

  float medianDistance = Median();

  //terapkan kalibrasi 
  float finalDistance = medianDistance + offset; 
  
  rawDistance = SAMPLE;

  bool currentState;
  if (finalDistance < batasDistance && finalDistance > 1.7 ) {
    currentState = HIGH;
  }else {
    currentState = LOW;
  }

  //menyiapkan data untuk dikirim 
  myData.state = currentState;
  myData.jarak = finalDistance;

  //kirimm data ke reciver
  esp_now_send(receiverMAC, (uint8_t *) &myData, sizeof(myData));

  //raw 
  Serial.print("rawdistance = ");
  Serial.print(rawDistance);
  Serial.println(" cm");
  //fix read
  Serial.print("terkalibrasi = ");
  Serial.print(finalDistance);
  Serial.println(" cm");

  if (finalDistance < 5){
    Serial.println(" Objek terlalu dekat");
  }

  else if (finalDistance > 400 || finalDistance <= 0) {
    currentState = LOW;
    Serial.println("Objek diluar batas sensor");
  }

delay(100);
}

float filteredsensor() {
  const int num_sample = 3;
  float sum = 0;
  int validsample = 0;
  long duration;
  float dist;

  for (int i = 0; i < num_sample; i++){
    digitalWrite(trgiPin, LOW);
    delayMicroseconds(2);
    //delay dan kirim pulse ke pin triger

    digitalWrite(trgiPin,HIGH);
    delayMicroseconds(10);
    digitalWrite(trgiPin,LOW);

    //baca dengan timeout 
    duration = pulseIn(echoPin, HIGH, 30000);

    if (duration > 0) {
      dist = duration * 0.034 / 2;

      //filter
      if (dist > 2 && dist < 400) {
        sum += dist;
        validsample++;
      }
    }
    delayMicroseconds(50);
  }

  if (validsample > 0) {
    return sum / validsample;
  }
  else {
    return 0;
  }
}

float Median() {
  if (!arrayFilled && readIndex < 3) {
    return readings[readIndex > 0 ? readIndex - 1 : 0];
  }

  float sortir[sample];
  int valid = arrayFilled ? sample : readIndex;

  for (int i = 0; i < valid; i++) {
    sortir[i] = readings[i];
  }
   //bubble sort simple 
   for (int i = 0; i < valid - 1; i++){
    for (int j = 0; j < valid - i - 1; j++) {
    if (sortir[j] > sortir[j + 1]){
      float temp = sortir[j];
      sortir[j] = sortir[j + 1];
      sortir[j + 1] = temp;
    }
   }
   }

   return sortir[valid / 2];
}

