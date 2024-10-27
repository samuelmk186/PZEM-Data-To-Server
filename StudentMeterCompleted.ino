#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <PZEM004Tv30.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

#if defined(ESP32)
PZEM004Tv30 pzem(Serial2, 16, 17);
#else
PZEM004Tv30 pzem(Serial2);
#endif

#define SS_PIN 5
#define RST_PIN 4
#define RELAY_PIN 2  
#define BUZZER_PIN 25 

byte nuidPICC[4] = {0, 0, 0, 0};
MFRC522::MIFARE_Key key;
MFRC522 rfid = MFRC522(SS_PIN, RST_PIN);

String uidString; 
float balance =0.0;

const char* ssid = "5G+";
const char* password = "samuelmk186##";

const char* serverAddress = "http://192.168.43.144/LSSEMS/Views/Student/pzem.php"; 
// const char* serverAddress = "http://192.168.43.166/LSSEMS/Views/Student/pzem.php";

void setup() {
    Serial.begin(115200);
    Wire.begin(); 
    SPI.begin(); 

    pinMode(RELAY_PIN, OUTPUT); 
    pinMode(BUZZER_PIN, OUTPUT); 
    digitalWrite(BUZZER_PIN, HIGH);

    lcd.init();  
    lcd.backlight();

    lcd.setCursor(0, 0);
    lcd.print("Initializing...");
    delay(2000);

    rfid.PCD_Init(); 

    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting...");
    }
    Serial.println("Connected to WiFi");

    // Display WiFi status on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi:");
    lcd.setCursor(6, 0);
   
    if (WiFi.status() == WL_CONNECTED) {
        lcd.print("Connected");
        delay(2000);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Please Place");
        lcd.setCursor(0, 1);
        lcd.print("The Card");

        rfid.PCD_Init();
    } else {
        lcd.print("Connection Error");
    }
}

void loop() {
    bool cardPresent = false;

    while (true) {
        if (rfid.PICC_IsNewCardPresent()) {
            readtag();
            float balance = sendUIDAndPZEMDataToServer(uidString, pzem.voltage(), pzem.current(), pzem.power(), pzem.energy(), pzem.frequency(), pzem.pf());

            if (balance > 0) {
                digitalWrite(RELAY_PIN, LOW);
                displayPZEMValues();
                lcd.setCursor(0, 0);
                lcd.print("Unit Balance: ");
                lcd.setCursor(0, 1); 
                lcd.print(balance);
 
                delay(4000);
                
                if (balance <= 5) {
                    for (int i = 0; i < 3; i++) {
                        digitalWrite(BUZZER_PIN, LOW); 
                        delay(100); 
                        digitalWrite(BUZZER_PIN, HIGH);
                        delay(100); 
                    }
                }
            } else {
                digitalWrite(RELAY_PIN, HIGH);
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Insufficient Balance");

                for (int i = 0; i < 3; i++) {
                    digitalWrite(BUZZER_PIN, LOW);
                    delay(100); 
                    digitalWrite(BUZZER_PIN,HIGH); 
                    delay(100);
                }
            }
            
            if (uidString == "") {
                digitalWrite(RELAY_PIN, HIGH);
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("No user found");

                delay(2000);
            }
            
            cardPresent = true; 
        } else {
            if (cardPresent) {
                digitalWrite(RELAY_PIN, HIGH);
                digitalWrite(BUZZER_PIN, HIGH);
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("No Card Present");
                cardPresent = false; 
            }
        }
    }
}


void readtag() {
    rfid.PICC_ReadCardSerial();
    MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
    Serial.print("RFID/NFC Tag Type: ");
    Serial.println(rfid.PICC_GetTypeName(piccType));

    // Print UID in Serial Monitor in the hex format
    Serial.print("UID:");
    uidString = ""; // Assign value to global variable
    for (int i = 0; i < rfid.uid.size; i++) {
        uidString += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
        uidString += String(rfid.uid.uidByte[i], HEX);
    }
    Serial.println(uidString);
}


void displayPZEMValues() {
    lcd.clear(); 

    Serial.print("Custom Address:");
    Serial.println(pzem.readAddress(), HEX);

    float voltage = pzem.voltage();
    float current = pzem.current();
    float power = pzem.power();
    float energy = pzem.energy();
    float frequency = pzem.frequency();
    float pf = pzem.pf();

    // Check if the data is valid
    if (isnan(voltage) || isnan(current) || isnan(power) || isnan(energy) || isnan(frequency) || isnan(pf)) {
        Serial.println("Error reading PZEM data");
    } else {
        lcd.setCursor(0, 0);
        lcd.print("V:");
        lcd.print(voltage);

        lcd.setCursor(0, 1);
        lcd.print("A:");
        lcd.print(current);

        delay(2000); 
        lcd.clear();
    }

    Serial.println();
    delay(2000);
}


float sendUIDAndPZEMDataToServer(String uid, float voltage, float current, float power, float energy, float frequency, float pf) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        if (http.begin(serverAddress)) { 
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            String post_data = "uid=" + uid +
                               "&voltage=" + String(voltage) +
                               "&current=" + String(current) +
                               "&power=" + String(power) +
                               "&energy=" + String(energy) +
                               "&frequency=" + String(frequency) +
                               "&pf=" + String(pf);
            int httpResponseCode = http.POST(post_data);
            if (httpResponseCode > 0) {
                Serial.print("HTTP Response code: ");
                Serial.println(httpResponseCode);
                String response = http.getString();

                Serial.println(response);
                balance = response.toFloat();
            } else {
                Serial.print("Error code: ");
                Serial.println(httpResponseCode);
            }
            http.end();
        } else {
            Serial.println("Unable to connect to server");
        }
    }
    return balance; 
}


void printDec(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], DEC);
    }
}
