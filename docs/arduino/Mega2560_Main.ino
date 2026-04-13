/*
 * =====================================================
 *  نظام الكراج الذكي - المتحكم الرئيسي (Mega 2560)
 *  Smart Garage System - Main Controller (Mega 2560)
 * =====================================================
 *  المبرمج: نظام الكراج الذكي
 *  الإصدار: 2.0.0
 *  التاريخ: 2025
 * =====================================================
 */

#include <Servo.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>

// =====================================================
// تعريف المنافذ
// Pin Definitions
// =====================================================

// --- Servo Motors (البوابات) ---
#define ENTRY_SERVO_PIN 9       // D9 - بوابة الدخول
#define EXIT_SERVO_PIN 10       // D10 - بوابة الخروج
#define SECURITY_SERVO_PIN 53   // D53 - باب غرفة الأمان

// --- Infrared Sensors (حساسات IR) ---
#define IR_ENTRY_PIN 27         // D27 - حساس دخول السيارات
#define IR_EXIT_PIN 28          // D28 - حساس خروج السيارات

// --- Environmental Sensors (مستشعرات البيئة) ---
#define DHT_PIN 26              // D26 - حساس الحرارة والرطوبة
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// --- Gas Sensor (حساس الغاز) ---
#define GAS_SENSOR_PIN A0       // A0 - حساس الغاز MQ-2

// --- Safety Sensors (مستشعرات السلامة) ---
#define FLAME_SENSOR_PIN 22     // D22 - حساس اللهب
#define VIBRATION_SENSOR_PIN 23 // D23 - حساس الاهتزاز
#define BUZZER_PIN 24           // D24 - جرس الإنذار

// --- Relay Module (وحدة الريلي) ---
#define LIGHT_RELAY_PIN 25      // D25 - تحكم الإضاءة

// --- RFID Module (وحدة RFID) ---
#define RFID_SS_PIN 8           // D8 - SS/RST
#define RFID_RST_PIN 49         // D49 - RST
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

// --- LCD Display (شاشة العرض) ---
#define I2C_ADDR 0x27            // عنوان I2C لشاشة LCD
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(I2C_ADDR, LCD_COLS, LCD_ROWS);

// --- Serial Communication (الاتصال التسلسلي مع ESP8266) ---
#define ESP_BAUD 115200

// =====================================================
// كائنات Servo
// Servo Objects
// =====================================================
Servo entryGateServo;
Servo exitGateServo;
Servo securityDoorServo;

// =====================================================
// متغيرات النظام
// System Variables
// =====================================================

// حالة البوابات
bool entryGateOpen = false;
bool exitGateOpen = false;
bool securityDoorOpen = false;

// عداد السيارات
int carsInside = 0;
const int MAX_CARS = 20;

// مستشعرات IR
bool irEntryDetected = false;
bool irExitDetected = false;
unsigned long irEntryTime = 0;
unsigned long irExitTime = 0;
bool entryCarPassed = false;
bool exitCarPassed = false;

// قراءات المستشعرات
float temperature = 25.0;
float humidity = 50.0;
int gasLevel = 150;
bool flameDetected = false;
bool vibrationDetected = false;

// حدود الإنذار
const int GAS_THRESHOLD = 300;
const float TEMP_THRESHOLD = 40.0;

// أوامر الريلي
bool lightOn = false;
bool buzzerActive = false;

//RFID
String authorizedCards[] = {"A1B2C3D4", "E5F6G7H8"}; // أرقام البطاقات المسموحة
int authorizedCount = 2;

// =====================================================
// الإعدادات الأولية
// Setup Function
// =====================================================
void setup() {
    // بدء الاتصال التسلسلي
    Serial.begin(ESP_BAUD);
    while (!Serial) {
        ; // انتظار الاتصال التسلسلي
    }

    // طباعة معلومات البدء
    Serial.println("\n==========================================");
    Serial.println("  نظام الكراج الذكي - Mega 2560");
    Serial.println("  Smart Garage System - Main Controller");
    Serial.println("==========================================\n");

    // تهيئة I2C
    Wire.begin();

    // تهيئة المستشعرات
    initSensors();

    // تهيئة البوابات
    initGates();

    // تهيئة LCD
    initLCD();

    // تهيئة RFID
    initRFID();

    // طباعة حالة البدء
    Serial.println("✓ تم تهيئة جميع المكونات بنجاح!");
    Serial.println("\n--- حالة النظام الأولية ---");
    Serial.print("عدد السيارات: ");
    Serial.println(carsInside);
    Serial.print("الحرارة: ");
    Serial.print(temperature);
    Serial.println("°C");
    Serial.print("الرطوبة: ");
    Serial.print(humidity);
    Serial.println("%");
    Serial.print("مستوى الغاز: ");
    Serial.print(gasLevel);
    Serial.println(" ppm");

    // عرض على LCD
    lcd.clear();
    lcd.print("Smart Garage v2");
    lcd.setCursor(0, 1);
    lcd.print("System Ready!");
    delay(2000);
}

// =====================================================
// الحلقة الرئيسية
// Main Loop
// =====================================================
void loop() {
    // قراءة المستشعرات
    readSensors();

    // فحص حساسات IR
    checkIRSensors();

    // فحص مستشعرات السلامة
    checkSafetySensors();

    // فحص RFID
    checkRFID();

    // استقبال أوامر من ESP8266
    checkSerialCommands();

    // تحديث LCD
    updateLCD();

    // إرسال البيانات إلى ESP8266
    sendDataToESP();

    // تأخير قصير
    delay(100);
}

// =====================================================
// تهيئة المستشعرات
// Initialize Sensors
// =====================================================
void initSensors() {
    Serial.println("جاري تهيئة المستشعرات...");

    // DHT11
    dht.begin();
    Serial.println("  ✓ DHT11 (الحرارة والرطوبة)");

    // IR Sensors (الدخول والخروج)
    pinMode(IR_ENTRY_PIN, INPUT_PULLUP);
    pinMode(IR_EXIT_PIN, INPUT_PULLUP);
    Serial.println("  ✓ حساسات IR (الدخول والخروج)");

    // Flame Sensor
    pinMode(FLAME_SENSOR_PIN, INPUT_PULLUP);
    Serial.println("  ✓ حساس اللهب");

    // Vibration Sensor
    pinMode(VIBRATION_SENSOR_PIN, INPUT_PULLUP);
    Serial.println("  ✓ حساس الاهتزاز");

    // Gas Sensor (Analog)
    pinMode(GAS_SENSOR_PIN, INPUT);
    Serial.println("  ✓ حساس الغاز MQ-2");

    // Buzzer
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("  ✓ جرس الإنذار");

    // Relay
    pinMode(LIGHT_RELAY_PIN, OUTPUT);
    digitalWrite(LIGHT_RELAY_PIN, LOW);
    Serial.println("  ✓ وحدة الريلي");

    Serial.println("✓ تم تهيئة جميع المستشعرات!\n");
}

// =====================================================
// تهيئة البوابات
// Initialize Gates
// =====================================================
void initGates() {
    Serial.println("جاري تهيئة البوابات...");

    // تهيئة Servo motors
    entryGateServo.attach(ENTRY_SERVO_PIN);
    exitGateServo.attach(EXIT_SERVO_PIN);
    securityDoorServo.attach(SECURITY_SERVO_PIN);

    // وضع البوابات في الوضع المغلق
    closeEntryGate();
    closeExitGate();
    closeSecurityDoor();

    Serial.println("  ✓ بوابة الدخول (D9)");
    Serial.println("  ✓ بوابة الخروج (D10)");
    Serial.println("  ✓ باب غرفة الأمان (D53)");
    Serial.println("✓ تم تهيئة البوابات!\n");
}

// =====================================================
// تهيئة LCD
// Initialize LCD
// =====================================================
void initLCD() {
    Serial.println("جاري تهيئة شاشة LCD...");

    lcd.init();
    lcd.backlight();

    Serial.println("✓ تم تهيئة شاشة LCD!\n");
}

// =====================================================
// تهيئة RFID
// Initialize RFID
// =====================================================
void initRFID() {
    Serial.println("جاري تهيئة وحدة RFID...");

    SPI.begin();
    rfid.PCD_Init();

    Serial.println("  ✓ MFRC522 RFID Reader");
    Serial.print("  ✓ عدد البطاقات المسجلة: ");
    Serial.println(authorizedCount);
    Serial.println("✓ تم تهيئة RFID!\n");
}

// =====================================================
// قراءة المستشعرات
// Read Sensors
// =====================================================
void readSensors() {
    // قراءة الحرارة والرطوبة
    float newTemp = dht.readTemperature();
    float newHumidity = dht.readHumidity();

    if (!isnan(newTemp)) {
        temperature = newTemp;
    }
    if (!isnan(newHumidity)) {
        humidity = newHumidity;
    }

    // قراءة مستوى الغاز
    gasLevel = analogRead(GAS_SENSOR_PIN);
    gasLevel = map(gasLevel, 0, 1023, 0, 1000); // تحويل إلى ppm تقريباً

    // قراءة حالة حساس اللهب
    flameDetected = (digitalRead(FLAME_SENSOR_PIN) == LOW);

    // قراءة حالة حساس الاهتزاز
    vibrationDetected = (digitalRead(VIBRATION_SENSOR_PIN) == HIGH);
}

// =====================================================
// فحص حساسات IR
// Check IR Sensors
// =====================================================
void checkIR Sensors() {
    // حساس الدخول
    bool irEntry = (digitalRead(IR_ENTRY_PIN) == LOW);

    if (irEntry && !irEntryDetected) {
        irEntryDetected = true;
        irEntryTime = millis();

        // فتح البوابة عند كشف سيارة
        if (carsInside < MAX_CARS) {
            openEntryGate();
            Serial.println(">> كشف سيارة عند الدخول!");
        } else {
            Serial.println(">> الكراج ممتلئ!");
            triggerBuzzer(2); // صفير تحذيري
        }
    }

    if (!irEntry) {
        irEntryDetected = false;
    }

    // حساس الخروج
    bool irExit = (digitalRead(IR_EXIT_PIN) == LOW);

    if (irExit && !irExitDetected) {
        irExitDetected = true;
        irExitTime = millis();

        // فتح بوابة الخروج
        openExitGate();
        Serial.println(">> كشف سيارة عند الخروج!");
    }

    if (!irExit) {
        irExitDetected = false;
    }

    // التحقق من مرور السيارة (بعد كشف الدخول)
    if (entryGateOpen && !entryCarPassed) {
        if (irEntryDetected) {
            // تأخير بسيط لضمان مرور السيارة
            delay(500);
            entryCarPassed = true;
        }
    }

    // إغلاق بوابة الدخول بعد مرور السيارة
    if (entryCarPassed && !irEntryDetected) {
        delay(1000); // تأخير لإغلاق البوابة
        closeEntryGate();
        entryCarPassed = false;
        carsInside++;
        Serial.print(">> سيارة进去了. العدد الكلي: ");
        Serial.println(carsInside);
    }

    // إغلاق بوابة الخروج بعد مرور السيارة
    if (exitGateOpen && !exitCarPassed) {
        if (irExitDetected) {
            delay(500);
            exitCarPassed = true;
        }
    }

    if (exitCarPassed && !irExitDetected) {
        delay(1000);
        closeExitGate();
        exitCarPassed = false;
        if (carsInside > 0) {
            carsInside--;
            Serial.print(">> سيارة خرجت. العدد الكلي: ");
            Serial.println(carsInside);
        }
    }
}

// =====================================================
// فحص مستشعرات السلامة
// Check Safety Sensors
// =====================================================
void checkSafetySensors() {
    // فحص الغاز
    if (gasLevel > GAS_THRESHOLD) {
        Serial.println("⚠ تحذير: مستوى الغاز مرتفع!");
        triggerAlarm("GAS");
    }

    // فحص اللهب
    if (flameDetected) {
        Serial.println("🔥 تحذير: كشف حريق!");
        triggerAlarm("FIRE");
    }

    // فحص الاهتزاز
    if (vibrationDetected) {
        Serial.println("⚠ تحذير: كشف اهتزاز!");
        triggerAlarm("VIBRATION");
    }

    // فحص الحرارة
    if (temperature > TEMP_THRESHOLD) {
        Serial.println("⚠ تحذير: الحرارة مرتفعة!");
    }
}

// =====================================================
// فحص RFID
// Check RFID
// =====================================================
void checkRFID() {
    // البحث عن بطاقة RFID
    if (!rfid.PICC_IsNewCardPresent()) {
        return;
    }

    if (!rfid.PICC_ReadCardSerial()) {
        return;
    }

    // قراءة رقم البطاقة
    String cardID = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        cardID += String(rfid.uid.uidByte[i], HEX);
    }

    cardID.toUpperCase();

    Serial.print(">> تم كشف بطاقة RFID: ");
    Serial.println(cardID);

    // التحقق من صلاحية البطاقة
    bool authorized = false;
    for (int i = 0; i < authorizedCount; i++) {
        if (cardID == authorizedCards[i]) {
            authorized = true;
            break;
        }
    }

    if (authorized) {
        Serial.println("✓ بطاقة معتمدة - فتح باب غرفة الأمان");
        openSecurityDoor();
    } else {
        Serial.println("✗ بطاقة غير معتمدة!");
        triggerBuzzer(3);
    }

    // إيقاف قراءة البطاقة
    rfid.PICC_HaltA();
}

// =====================================================
// استقبال أوامر من ESP8266
// Check Serial Commands
// =====================================================
void checkSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();

        Serial.print("أمر مستلم: ");
        Serial.println(command);

        // معالجة الأوامر
        if (command.startsWith("CMD:ENTRY:")) {
            String value = command.substring(10);
            if (value == "1") {
                openEntryGate();
            } else {
                closeEntryGate();
            }
        }
        else if (command.startsWith("CMD:EXIT:")) {
            String value = command.substring(9);
            if (value == "1") {
                openExitGate();
            } else {
                closeExitGate();
            }
        }
        else if (command.startsWith("CMD:SECURITY:")) {
            String value = command.substring(13);
            if (value == "1") {
                openSecurityDoor();
            } else {
                closeSecurityDoor();
            }
        }
        else if (command.startsWith("CMD:LIGHT:")) {
            String value = command.substring(10);
            lightOn = (value == "1");
            digitalWrite(LIGHT_RELAY_PIN, lightOn ? HIGH : LOW);
            Serial.println(lightOn ? "✓ تم تشغيل الإضاءة" : "✓ تم إطفاء الإضاءة");
        }
        else if (command.startsWith("CMD:BUZZER:")) {
            String value = command.substring(11);
            if (value == "1") {
                triggerBuzzer(5);
            }
        }
    }
}

// =====================================================
// إرسال البيانات إلى ESP8266
// Send Data to ESP
// =====================================================
void sendDataToESP() {
    static unsigned long lastSend = 0;

    if (millis() - lastSend >= 2000) {
        lastSend = millis();

        // تنسيق البيانات: cars,temp,humidity,flame,gas,vibration,irEntry,irExit
        String data = "DATA:";
        data += String(carsInside) + ",";
        data += String(temperature) + ",";
        data += String(humidity) + ",";
        data += (flameDetected ? "1" : "0") + ",";
        data += String(gasLevel) + ",";
        data += (vibrationDetected ? "1" : "0") + ",";
        data += (irEntryDetected ? "1" : "0") + ",";
        data += (irExitDetected ? "1" : "0");

        Serial.println(data);
    }
}

// =====================================================
// تحديث LCD
// Update LCD
// =====================================================
void updateLCD() {
    static unsigned long lastUpdate = 0;

    if (millis() - lastUpdate >= 1000) {
        lastUpdate = millis();

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Cars:");
        lcd.print(carsInside);
        lcd.print("/");
        lcd.print(MAX_CARS);

        lcd.setCursor(0, 1);
        lcd.print("T:");
        lcd.print(temperature, 1);
        lcd.print("C H:");
        lcd.print(humidity, 0);
        lcd.print("%");

        // عرض تنبيه إذا موجود
        if (gasLevel > GAS_THRESHOLD || flameDetected) {
            lcd.setCursor(13, 1);
            lcd.print("!");
        }
    }
}

// =====================================================
// التحكم بالبوابات
// Gate Control Functions
// =====================================================
void openEntryGate() {
    if (!entryGateOpen) {
        entryGateOpen = true;
        entryCarPassed = false;

        // تحريك Servo
        for (int pos = 0; pos <= 90; pos += 5) {
            entryGateServo.write(pos);
            delay(15);
        }

        Serial.println(">> بوابة الدخول مفتوحة");
    }
}

void closeEntryGate() {
    if (entryGateOpen) {
        entryGateOpen = false;

        // تحريك Servo
        for (int pos = 90; pos >= 0; pos -= 5) {
            entryGateServo.write(pos);
            delay(15);
        }

        Serial.println(">> بوابة الدخول مغلقة");
    }
}

void openExitGate() {
    if (!exitGateOpen) {
        exitGateOpen = true;
        exitCarPassed = false;

        for (int pos = 0; pos <= 90; pos += 5) {
            exitGateServo.write(pos);
            delay(15);
        }

        Serial.println(">> بوابة الخروج مفتوحة");
    }
}

void closeExitGate() {
    if (exitGateOpen) {
        exitGateOpen = false;

        for (int pos = 90; pos >= 0; pos -= 5) {
            exitGateServo.write(pos);
            delay(15);
        }

        Serial.println(">> بوابة الخروج مغلقة");
    }
}

void openSecurityDoor() {
    if (!securityDoorOpen) {
        securityDoorOpen = true;

        for (int pos = 0; pos <= 90; pos += 5) {
            securityDoorServo.write(pos);
            delay(15);
        }

        Serial.println(">> باب غرفة الأمان مفتوح");
    }
}

void closeSecurityDoor() {
    if (securityDoorOpen) {
        securityDoorOpen = false;

        for (int pos = 90; pos >= 0; pos -= 5) {
            securityDoorServo.write(pos);
            delay(15);
        }

        Serial.println(">> باب غرفة الأمان مغلق");
    }
}

// =====================================================
// الإنذارات
// Alarm Functions
// =====================================================
void triggerAlarm(String type) {
    Serial.print("⚠ إنذار: ");
    Serial.println(type);

    // تشغيل الصفارة
    triggerBuzzer(10);

    // إضاءة الطوارئ
    digitalWrite(LIGHT_RELAY_PIN, HIGH);
    delay(500);
    digitalWrite(LIGHT_RELAY_PIN, LOW);
}

void triggerBuzzer(int times) {
    for (int i = 0; i < times; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(200);
        digitalWrite(BUZZER_PIN, LOW);
        delay(200);
    }
}
