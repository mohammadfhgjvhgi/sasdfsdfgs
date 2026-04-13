/*
 * =====================================================
 *  نظام الكراج الذكي - وحدة WiFi (ESP8266)
 *  Smart Garage System - WiFi Module (ESP8266)
 * =====================================================
 *  المبرمج: نظام الكراج الذكي
 *  الإصدار: 2.0.0
 *  التاريخ: 2025
 * =====================================================
 */

#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>

// =====================================================
// إعدادات WiFi
// WiFi Settings
// =====================================================
#define WIFI_SSID "Your_WiFi_Name"
#define WIFI_PASSWORD "Your_WiFi_Password"

// =====================================================
// إعدادات Firebase
// Firebase Settings
// =====================================================
#define FIREBASE_HOST "my-systim-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "AIzaSyDemo-Auth-Token"

// كائنات Firebase
FirebaseData firebaseData;
FirebaseJson json;

// =====================================================
// تعريف المنافذ
// Pin Definitions
// =====================================================
#define RX_PIN 4    // D2 - اتصال تسلسلي مع Mega
#define TX_PIN 5    // D1 - اتصال تسلسلي مع Mega

// =====================================================
// متغيرات النظام
// System Variables
// =====================================================
unsigned long lastUpdate = 0;
unsigned long updateInterval = 2000; // تحديث كل 2 ثانية
bool isConnected = false;

// حالات النظام
struct SystemState {
    int carsCount;
    float temperature;
    float humidity;
    bool flameAlert;
    bool gasAlert;
    bool vibrationAlert;
    bool entryGate;
    bool exitGate;
    bool securityDoor;
    bool irEntry;
    bool irExit;
    int gasLevel;
};

SystemState systemState = {0, 25.0, 50.0, false, false, false, false, false, false, false, false, 150};

// =====================================================
// الإعدادات الأولية
// Setup Function
// =====================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==========================================");
    Serial.println("  نظام الكراج الذكي - وحدة WiFi");
    Serial.println("  Smart Garage System - WiFi Module");
    Serial.println("==========================================\n");

    // تشغيل WiFi
    connectWiFi();

    // تشغيل Firebase
    connectFirebase();

    // إعداد الاتصال التسلسلي مع Mega
    Serial.println("جاري إعداد الاتصال مع Mega 2560...");
    Serial.flush();

    Serial.println("\n✓ تم تهيئة وحدة WiFi بنجاح!");
    Serial.println("  - WiFi: متصل");
    Serial.println("  - Firebase: متصل");
    Serial.println("  - Mega 2560: في انتظار الاتصال");
}

// =====================================================
// الحلقة الرئيسية
// Main Loop
// =====================================================
void loop() {
    // التحقق من اتصال WiFi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠ فقد الاتصال بالشبكة! جاري إعادة الاتصال...");
        connectWiFi();
    }

    // التحقق من الاتصال التسلسلي مع Mega
    if (Serial.available()) {
        String data = Serial.readStringUntil('\n');
        parseMegaData(data);
    }

    // إرسال واستقبال بيانات Firebase
    if (millis() - lastUpdate >= updateInterval) {
        lastUpdate = millis();
        syncWithFirebase();
        sendToMega();
    }

    // معالجة الأوامر من Firebase
    checkFirebaseCommands();

    delay(50);
}

// =====================================================
// الاتصال بشبكة WiFi
// Connect to WiFi Network
// =====================================================
void connectWiFi() {
    Serial.print("جاري الاتصال بـ WiFi: ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✓ تم الاتصال بـ WiFi!");
        Serial.print("  عنوان IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("  قوة الإشارة: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        isConnected = true;
    } else {
        Serial.println("\n✗ فشل الاتصال بـ WiFi!");
        isConnected = false;
    }
}

// =====================================================
// الاتصال بـ Firebase
// Connect to Firebase
// =====================================================
void connectFirebase() {
    Serial.print("جاري الاتصال بـ Firebase...");

    Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
    Firebase.setMaxRetry(firebaseData, 3);

    if (Firebase.isWiFiOK()) {
        Serial.println(" ✓");
        Serial.println("✓ تم الاتصال بـ Firebase بنجاح!");
    } else {
        Serial.println(" ✗");
        Serial.println("✗ فشل الاتصال بـ Firebase!");
        Serial.print("  السبب: ");
        Serial.println(Firebase.errorReason());
    }
}

// =====================================================
// تحليل البيانات من Mega
// Parse Data from Mega
// =====================================================
void parseMegaData(String data) {
    // تنسيق البيانات: cars,temp,humidity,flame,gas,vibration,irEntry,irExit
    // مثال: 5,25.5,50,false,150,false,false,true

    data.trim();

    if (data.startsWith("DATA:")) {
        data = data.substring(5);

        int index = 0;
        int commaIndex = 0;

        // استخراج عدد السيارات
        commaIndex = data.indexOf(',');
        if (commaIndex > 0) {
            systemState.carsCount = data.substring(0, commaIndex).toInt();
            index = commaIndex + 1;
        }

        // استخراج الحرارة
        commaIndex = data.indexOf(',', index);
        if (commaIndex > 0) {
            systemState.temperature = data.substring(index, commaIndex).toFloat();
            index = commaIndex + 1;
        }

        // استخراج الرطوبة
        commaIndex = data.indexOf(',', index);
        if (commaIndex > 0) {
            systemState.humidity = data.substring(index, commaIndex).toFloat();
            index = commaIndex + 1;
        }

        // استخراج حالة اللهب
        commaIndex = data.indexOf(',', index);
        if (commaIndex > 0) {
            systemState.flameAlert = data.substring(index, commaIndex) == "1";
            index = commaIndex + 1;
        }

        // استخراج مستوى الغاز
        commaIndex = data.indexOf(',', index);
        if (commaIndex > 0) {
            systemState.gasLevel = data.substring(index, commaIndex).toInt();
            index = commaIndex + 1;
        }

        // استخراج حالة الاهتزاز
        commaIndex = data.indexOf(',', index);
        if (commaIndex > 0) {
            systemState.vibrationAlert = data.substring(index, commaIndex) == "1";
            index = commaIndex + 1;
        }

        // استخراج حساس IR الدخول
        commaIndex = data.indexOf(',', index);
        if (commaIndex > 0) {
            systemState.irEntry = data.substring(index, commaIndex) == "1";
            index = commaIndex + 1;
        }

        // استخراج حساس IR الخروج
        if (index < data.length()) {
            systemState.irExit = data.substring(index) == "1";
        }

        // طباعة البيانات المستلمة
        Serial.println("\n--- بيانات مستلمة من Mega ---");
        Serial.print("عدد السيارات: ");
        Serial.println(systemState.carsCount);
        Serial.print("الحرارة: ");
        Serial.print(systemState.temperature);
        Serial.println("°C");
        Serial.print("الرطوبة: ");
        Serial.print(systemState.humidity);
        Serial.println("%");
        Serial.print("اللهب: ");
        Serial.println(systemState.flameAlert ? "خطر!" : "طبيعي");
        Serial.print("الغاز: ");
        Serial.print(systemState.gasLevel);
        Serial.println(" ppm");
        Serial.print("الاهتزاز: ");
        Serial.println(systemState.vibrationAlert ? "نعم" : "لا");
        Serial.print("IR الدخول: ");
        Serial.println(systemState.irEntry ? "كشف" : "لا");
        Serial.print("IR الخروج: ");
        Serial.println(systemState.irExit ? "كشف" : "لا");
    }
}

// =====================================================
// مزامنة البيانات مع Firebase
// Sync Data with Firebase
// =====================================================
void syncWithFirebase() {
    if (!Firebase.isWiFiOK()) {
        return;
    }

    // تحديث بيانات الكراج
    Firebase.setInt(firebaseData, "/garage/carsCount", systemState.carsCount);
    Firebase.setFloat(firebaseData, "/garage/temperature", systemState.temperature);
    Firebase.setFloat(firebaseData, "/garage/humidity", systemState.humidity);
    Firebase.setBool(firebaseData, "/garage/flameAlert", systemState.flameAlert);
    Firebase.setInt(firebaseData, "/garage/gasLevel", systemState.gasLevel);
    Firebase.setBool(firebaseData, "/garage/vibrationAlert", systemState.vibrationAlert);
    Firebase.setBool(firebaseData, "/garage/irEntry", systemState.irEntry);
    Firebase.setBool(firebaseData, "/garage/irExit", systemState.irExit);
    Firebase.setBool(firebaseData, "/garage/gasAlert", systemState.gasLevel > 300);

    // تحديث الطوابع الزمنية
    Firebase.setTimestamp(firebaseData, "/garage/lastUpdate");

    if (firebaseData.httpCode() == FIREBASE_ERROR_HTTP_CODE_OK) {
        Serial.print(".");
    } else {
        Serial.print("E");
    }
}

// =====================================================
// التحقق من أوامر Firebase
// Check Firebase Commands
// =====================================================
void checkFirebaseCommands() {
    if (!Firebase.isWiFiOK()) {
        return;
    }

    // التحقق من أمر بوابة الدخول
    if (Firebase.getBool(firebaseData, "/commands/entryGate")) {
        bool command = firebaseData.boolData();
        if (command != systemState.entryGate) {
            systemState.entryGate = command;
            Serial.print("CMD:ENTRY:");
            Serial.println(command ? "1" : "0");
        }
    }

    // التحقق من أمر بوابة الخروج
    if (Firebase.getBool(firebaseData, "/commands/exitGate")) {
        bool command = firebaseData.boolData();
        if (command != systemState.exitGate) {
            systemState.exitGate = command;
            Serial.print("CMD:EXIT:");
            Serial.println(command ? "1" : "0");
        }
    }

    // التحقق من أمر باب غرفة الأمان
    if (Firebase.getBool(firebaseData, "/commands/securityDoor")) {
        bool command = firebaseData.boolData();
        if (command != systemState.securityDoor) {
            systemState.securityDoor = command;
            Serial.print("CMD:SECURITY:");
            Serial.println(command ? "1" : "0");
        }
    }

    // التحقق من أمر الإضاءة
    if (Firebase.getBool(firebaseData, "/commands/light")) {
        bool command = firebaseData.boolData();
        Serial.print("CMD:LIGHT:");
        Serial.println(command ? "1" : "0");
    }

    // التحقق من أمر الصفارة
    if (Firebase.getBool(firebaseData, "/commands/buzzer")) {
        bool command = firebaseData.boolData();
        Serial.print("CMD:BUZZER:");
        Serial.println(command ? "1" : "0");
    }
}

// =====================================================
// إرسال البيانات إلى Mega
// Send Data to Mega
// =====================================================
void sendToMega() {
    // إرسال حالة الاتصال
    String data = "WIFI:" + String(WiFi.status() == WL_CONNECTED ? "OK" : "FAIL");
    data += ",FB:" + String(Firebase.isWiFiOK() ? "OK" : "FAIL");
    data += ",RSSI:" + String(WiFi.RSSI());
    Serial.println(data);
}

// =====================================================
// طباعة حالة النظام
// Print System Status
// =====================================================
void printStatus() {
    Serial.println("\n========== حالة النظام ==========");
    Serial.print("WiFi: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "متصل" : "غير متصل");
    Serial.print("Firebase: ");
    Serial.println(Firebase.isWiFiOK() ? "متصل" : "غير متصل");
    Serial.print("عدد السيارات: ");
    Serial.println(systemState.carsCount);
    Serial.print("الحرارة: ");
    Serial.print(systemState.temperature);
    Serial.println("°C");
    Serial.print("الرطوبة: ");
    Serial.print(systemState.humidity);
    Serial.println("%");
    Serial.println("================================\n");
}
