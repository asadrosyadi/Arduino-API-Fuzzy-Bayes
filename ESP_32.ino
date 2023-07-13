#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <RBDdimmer.h>

// Sensor Suhu Air
#include <OneWire.h>
#include <DallasTemperature.h>
const int oneWireBus = 19; 
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

// Sensor DHT 22
#include "DHT.h"
#define DHTPIN 23     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Sensor MLX90614
#include <Adafruit_MLX90614.h>
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// Sensor Soil Moinsture
int media_tanam,sensor_moinsture;
const int moinsture_pin = 36;

// Sensor TDS
#define TdsSensorPin 39
#define VREF 3.3              // analog reference voltage(Volt) of the ADC
#define SCOUNT  30            // sum of sample point
int analogBuffer[SCOUNT];     // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;
float averageVoltage = 0;
float tdsValue = 0;
// median filtering algorithm
int getMedianNum(int bArray[], int iFilterLen){
  int bTab[iFilterLen];
  for (byte i = 0; i<iFilterLen; i++)
  bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0){
    bTemp = bTab[(iFilterLen - 1) / 2];
  }
  else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}

// Sensor pH
#define ph_pin 34
float pH = 0;
float PH_step;
int nilai_analog_PH;
double TeganganPh;

//untuk kalibrasi
float PH4 = 3.28;
float PH7 = 2.44;

//Sensor DO
const int doPin = 35;


//PWM AC
const int zeroCrossPin  = 13;
const int acdPin  = 12;
int lambat  = 60;
int sedang  = 80;
int cepat  = 100;
dimmerLamp acd(acdPin,zeroCrossPin);

// output Kontrol Suhu
int led = 4;
int pemanas = 16;

// output Kelembapan
int humidifer = 27;
int dehumidifer = 17;

// output Nutrisi & Air
int pompa_nutrisi = 26;
int pompa_penguras = 18;
int kipas = 32;

// output pH
int ph_turun = 33;
int ph_naik = 25;

const char* ssid = "myfarm";
const char* password = "12345678";
String token = "zwJdmaOCQoDW9XgH";
String HWID = "VP221201D";
String email = "admin@admin.com";
String linkGET = "https://myfarm.lactograin.id/rest/fuzzynaviebayes/" + HWID;
String kirim_server = "https://myfarm.lactograin.id/rest/kirimdatasensor" ;


void setup() {
  Serial.begin(115200);
  sensors.begin();
  dht.begin();
  mlx.begin();
  pinMode(TdsSensorPin,INPUT);
  pinMode(ph_pin, INPUT);
  pinMode(doPin, INPUT);
  acd.begin(NORMAL_MODE, ON);
  pinMode(led, OUTPUT);
  pinMode(pemanas, OUTPUT);
  pinMode(humidifer, OUTPUT);
  pinMode(dehumidifer, OUTPUT);
  pinMode(pompa_nutrisi, OUTPUT);
  pinMode(pompa_penguras, OUTPUT);
  pinMode(kipas, OUTPUT);
  pinMode(ph_turun, OUTPUT);
  pinMode(ph_naik, OUTPUT);

  // Hubungkan ke WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Gagal terhubung ke WiFi. Mengulangi...");
  }
  Serial.println("Terhubung ke WiFi!");

}

void loop() {
  baca_sensor();
  baca_jason();
 
}

void baca_sensor(){
  // Sensor suhu Air
  sensors.requestTemperatures(); 
  float suhu_air = (sensors.getTempCByIndex(0));
  Serial.print("suhu_air: ");
  Serial.println(suhu_air);
 
  // DHT 22
  float kelembapan_udara = (dht.readHumidity()-35); //kelembapan
  float suhu_udara = dht.readTemperature(); //suhu
  Serial.print("kelembapan_udara: ");
  Serial.println(kelembapan_udara);
  Serial.print("suhu_udara: ");
  Serial.println(suhu_udara);
 
  // MLX90614
  float suhu_daun = (mlx.readObjectTempC());
  Serial.print("suhu_daun: ");
  Serial.println(suhu_daun);
  

  // Soil Moinsture
  sensor_moinsture = analogRead(moinsture_pin);
  media_tanam = ( 100 - ( (sensor_moinsture/4095.00) * 100 ) );
  Serial.print("media_tanam: ");
  Serial.println(media_tanam);
  

  // Sensor TDS
  static unsigned long analogSampleTimepoint = millis();
  if(millis()-analogSampleTimepoint > 40U){     //every 40 milliseconds,read the analog value from the ADC
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);    //read the analog value and store into the buffer
    analogBufferIndex++;
    if(analogBufferIndex == SCOUNT){ 
    analogBufferIndex = 0;
    }
  }
  static unsigned long printTimepoint = millis();
  if(millis()-printTimepoint > 800U){
    printTimepoint = millis();
    for(copyIndex=0; copyIndex<SCOUNT; copyIndex++){
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
      // read the analog value more stable by the median filtering algorithm, and convert to voltage value
      averageVoltage = getMedianNum(analogBufferTemp,SCOUNT) * (float)VREF / 4096.0;
      //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0)); 
      float compensationCoefficient = 1.0+0.02*(suhu_air-25.0);
      //temperature compensation
      float compensationVoltage=averageVoltage/compensationCoefficient;
      //convert voltage value to tds value
      tdsValue=(133.42*compensationVoltage*compensationVoltage*compensationVoltage - 255.86*compensationVoltage*compensationVoltage + 857.39*compensationVoltage)*0.5;
    }
  }
  float tds = (tdsValue+315);
  Serial.print("tds: ");
  Serial.println(tds);
  
  //Sensor pH
  float total_TeganganPh = 0;
  for (int i = 0; i < 10000; i++) { // Lakukan 500 kali pembacaan
    nilai_analog_PH = analogRead(ph_pin);
    TeganganPh = 3.3/4096.0 * nilai_analog_PH;
    total_TeganganPh += TeganganPh; 
  }
  float rata2_TeganganPh = total_TeganganPh / 10000.0; // Hitung rata-rata tegangan
  PH_step = (PH4-PH7)/3;
  pH = (7.00 + ((PH7 - rata2_TeganganPh)/PH_step))+2;
  if (pH > 8){pH= pH + 1;}
  Serial.print("Tegangan pH: ");
  Serial.println(rata2_TeganganPh, 2);
  Serial.print("pH Cairan: ");
  Serial.println(pH);
  
 //Sensor DO
  float bacaoksigen = analogRead(doPin);
  float Teganganoksigen = 3.3/4096.0 * bacaoksigen;
  //float oksigen = Teganganoksigen * 10;
  //if (oksigen > 8){oksigen = oksigen - 18;}
  //else {oksigen = oksigen + 2;}
  float oksigen = 10;
  Serial.print("oksigen: ");
  Serial.println(oksigen);
  delay(200);

// Kirim data sensor
  if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;
      String serverPath = kirim_server + "?&&token=" + token + "&&HWID=" + HWID + "&&rh=" + kelembapan_udara + "&&suhu_udara=" + suhu_udara + "&&suhu_daun=" + suhu_daun + "&&media_tanam=" + media_tanam + "&&ppm=" + tds + "&&ph=" + pH + "&&oksigen=" + oksigen + "&&email=" + email;
      // Your Domain name with URL path or IP address with path
      http.begin(serverPath.c_str());
      // Send HTTP GET request
      int httpResponseCode = http.GET();
      if (httpResponseCode>0) {
        //Serial.print("HTTP Response code: ");
        //Serial.println(httpResponseCode);
        String payload = http.getString();
        //Serial.println(payload);
      }
      else {
        //Serial.print("Error code: ");
        //Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
    }
       // else {Serial.println("WiFi Disconnected");}
  }

void baca_jason(){
// Mengambil Data Jason
  // Buat koneksi HTTP
  HTTPClient http;
  http.begin(linkGET);

  // Lakukan permintaan GET
  int httpCode = http.GET();

  // Jika permintaan berhasil
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    //Serial.println(payload);

    // Parse data JSON
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, payload);
    JsonObject data = doc["Data"][0];

    // Ambil data yang diperlukan
    String Kesimpulan_debit = data["Kesimpulan_debit"];
    String Kesimpulan_kontrolsuhu = data["Kesimpulan_kontrolsuhu"];
    String Kesimpulan_kontrolkelembapan = data["Kesimpulan_kontrolkelembapan"];
    String Kesimpulan_kontrolnutrisiair = data["Kesimpulan_kontrolnutrisiair"];
    String Kesimpulan_kontrolph = data["Kesimpulan_kontrolph"];
    String jeda_pembacaan = data["jeda_pembacaan"];
    String waktu_penguras = data["waktu_penguras"];

int delay_pembacaan = (jeda_pembacaan.toInt()*60000);
int delay_penguras = (waktu_penguras.toInt()*10000); // 10 detik
Serial.print("jeda_pembacaan (ms): ");
Serial.println(delay_pembacaan);
Serial.print("waktu_penguras (ms): ");
Serial.println(delay_penguras);

//Debit
if (Kesimpulan_debit == "Lambat"){
  acd.setPower(lambat);
  Serial.print("Kesimpulan_debit: ");
  Serial.println("Lambat");
  }
else if (Kesimpulan_debit == "Sedang"){
  acd.setPower(sedang);
  Serial.print("Kesimpulan_debit: ");
  Serial.println("Sedang");
  }  
else if (Kesimpulan_debit == "Cepat"){
  acd.setPower(cepat);
  Serial.print("Kesimpulan_debit: ");
  Serial.println("Cepat");
  }

// Kontrol Suhu
if (Kesimpulan_kontrolsuhu == "LED Menyala"){
  digitalWrite(led, HIGH);
  digitalWrite(kipas, LOW);
  digitalWrite(pemanas, LOW);
  Serial.print("Kesimpulan_kontrolsuhu: ");
  Serial.println("LED Menyala");
  }
else if (Kesimpulan_kontrolsuhu == "Pemanas & LED Menyala"){
  digitalWrite(led, HIGH);
  digitalWrite(kipas, LOW);
  digitalWrite(pemanas, HIGH);
  Serial.print("Kesimpulan_kontrolsuhu: ");
  Serial.println("Pemanas & LED Menyala");
  }
else if (Kesimpulan_kontrolsuhu == "Pendingin Menyala"){
  digitalWrite(led, LOW);
  digitalWrite(kipas, HIGH);
  digitalWrite(pemanas, LOW);
  Serial.print("Kesimpulan_kontrolsuhu: ");
  Serial.println("Pendingin Menyala");
  }  

// Kelembapan
if (Kesimpulan_kontrolkelembapan == "Humidifer Menyala"){
  digitalWrite(humidifer, HIGH);
  digitalWrite(dehumidifer, LOW);
  Serial.print("Kesimpulan_kontrolkelembapan: ");
  Serial.println("Humidifer Menyala");
  }
else if (Kesimpulan_kontrolkelembapan == "Dehumidifer Menyala"){
  digitalWrite(humidifer, LOW);
  digitalWrite(dehumidifer, HIGH);
  Serial.print("Kesimpulan_kontrolkelembapan: ");
  Serial.println("Dehumidifer Menyala");
  }
else if (Kesimpulan_kontrolkelembapan == "-"){
  digitalWrite(humidifer, LOW);
  digitalWrite(dehumidifer, LOW);
  Serial.print("Kesimpulan_kontrolkelembapan: ");
  Serial.println("-");
  }  

// Nutrisi & Air
if (Kesimpulan_kontrolnutrisiair == "Pompa Nutrisi AB Menyala"){
  digitalWrite(pompa_nutrisi, HIGH);
  digitalWrite(pompa_penguras, LOW);
  Serial.print("Kesimpulan_kontrolnutrisiair: ");
  Serial.println("Pompa Nutrisi AB Menyala");
  delay (3000);
  digitalWrite(pompa_nutrisi, LOW);
  digitalWrite(pompa_penguras, LOW); 
  }
else if (Kesimpulan_kontrolnutrisiair == "Pompa Penguras Menyala"){
  digitalWrite(pompa_nutrisi, LOW);
  digitalWrite(pompa_penguras, HIGH);
  Serial.print("Kesimpulan_kontrolnutrisiair: ");
  Serial.println("Pompa Penguras Menyala");
  delay (delay_penguras);
  digitalWrite(pompa_nutrisi, LOW);
  digitalWrite(pompa_penguras, LOW); 
  }
else if (Kesimpulan_kontrolnutrisiair == "-"){
  digitalWrite(pompa_nutrisi, LOW);
  digitalWrite(pompa_penguras, LOW);
  Serial.print("Kesimpulan_kontrolnutrisiair: ");
  Serial.println("-");
  }  

// pH
if (Kesimpulan_kontrolph == "Pompa pH Turun Menyala"){
  digitalWrite(ph_turun, HIGH);
  digitalWrite(ph_naik, LOW);
  Serial.print("Kesimpulan_kontrolph: ");
  Serial.println("Pompa pH Turun Menyala");
  delay (3000);
  digitalWrite(ph_turun, LOW);
  digitalWrite(ph_naik, LOW);
  }
else if (Kesimpulan_kontrolph == "Pompa pH Naik Menyala"){
  digitalWrite(ph_turun, LOW);
  digitalWrite(ph_naik, HIGH);
  Serial.print("Kesimpulan_kontrolph: ");
  Serial.println("Pompa pH Naik Menyala");
  delay (3000);
  digitalWrite(ph_turun, LOW);
  digitalWrite(ph_naik, LOW);
  }
else if (Kesimpulan_kontrolph == "-"){
  digitalWrite(ph_turun, LOW);
  digitalWrite(ph_naik, LOW);
  Serial.print("Kesimpulan_kontrolph: ");
  Serial.println("-");
  }
  
  delay (delay_pembacaan);  
  
  } else {
    Serial.println("Gagal melakukan permintaan HTTP");
  }

  http.end();  // Putuskan koneksi HTTP

  }

  
