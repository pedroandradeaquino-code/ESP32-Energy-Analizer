#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <time.h>
#include <HardwareSerial.h>
#include <PZEM004Tv30.h>

// ==================== WIFI ====================
const char* ssid = "Americanopólis";
const char* password = "urso140109";

// ==================== PZEM SETUP ====================
// Usar Serial2 (UART2) do ESP32
// GPIO 16 = RX (recebe dados do PZEM)
// GPIO 17 = TX (envia dados para o PZEM)
// Velocidade: 9600 baud (padrão do PZEM)

//PZEM004Tv30 pzem(Serial2);  // Usar Serial2 diretamente

// ==================== PINOS ====================
const int RELAY_PIN = 5;    // GPIO5 - Controla o relé
const int LED_PIN = 2;      // GPIO2 - LED de status

// ==================== VARIÁVEIS DE PROTEÇÃO ====================
float voltageMax = 250.0;
float voltageMin = 180.0;
float currentMax = 25.0;
int deactivationTime = 10;

// ==================== VARIÁVEIS DE ESTADO ====================
float voltage = 0;
float current = 0;
float power = 0;
float energy = 0;
float frequency = 0;
bool relayActive = true;
bool isDeactivated = false;
unsigned long deactivatedAt = 0;

// ==================== LOGS ====================
struct EventLog {
  unsigned long timestamp;
  String type;
  float voltage;
  float current;
};

#define MAX_LOGS 100
EventLog logs[MAX_LOGS];
int logCount = 0;

// ==================== SERVIDOR WEB ====================
WebServer server(80);

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n╔════════════════════════════════════════╗");
  Serial.println("║   PROTETOR DE CORRENTE COM PZEM v2.0   ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  // Inicializar pinos
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // Relé ligado
  relayActive = true;
  
  // Inicializar SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ Erro ao inicializar SPIFFS");
  }
  
  // Conectar WiFi
  connectWiFi();
  
  // Configurar NTP
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  // Inicializar Serial2 para PZEM
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  delay(100);
  
  // Testar PZEM
  testPZEM();
  
  // Configurar servidor web
  setupServer();
  
  Serial.println("✅ Inicialização completa!\n");
}

// ==================== LOOP ====================
void loop() {
  server.handleClient();
  
  static unsigned long lastRead = 0;
  if (millis() - lastRead >= 500) {
    lastRead = millis();
    readPZEM();
    checkProtection();
    updateLED();
  }
  
  // Rearmar automaticamente
  if (isDeactivated && (millis() - deactivatedAt >= deactivationTime * 1000)) {
    rearmRelay();
  }
}

// ==================== LEITURA PZEM ====================
void readPZEM() {
  voltage = pzem.voltage();
  current = pzem.current();
  power = pzem.power();
  energy = pzem.energy();
  frequency = pzem.frequency();
  
  // Validar leituras
  if (isnan(voltage)) voltage = 0;
  if (isnan(current)) current = 0;
  if (isnan(power)) power = 0;
  if (isnan(energy)) energy = 0;
  if (isnan(frequency)) frequency = 60;
}

// ==================== TESTE PZEM ====================
void testPZEM() {
  Serial.println("🔍 Testando comunicação com PZEM...");
  delay(1000);
  
  float v = pzem.voltage();
  
  if (isnan(v)) {
    Serial.println("❌ PZEM não respondeu!");
    Serial.println("   Verificar:");
    Serial.println("   - GPIO 16/17 conectados?");
    Serial.println("   - GND comum?");
    Serial.println("   - Alimentação 5V?");
  } else {
    Serial.printf("✅ PZEM conectado! Tensão: %.1fV\n", v);
  }
}

// ==================== PROTEÇÃO ====================
void checkProtection() {
  if (!relayActive) return;
  
  bool shouldTrip = false;
  String reason = "";
  
  if (voltage > voltageMax) {
    shouldTrip = true;
    reason = "PICO";
  }
  else if (voltage < voltageMin) {
    shouldTrip = true;
    reason = "QUEDA";
  }
  else if (current > currentMax) {
    shouldTrip = true;
    reason = "SOBRECORRENTE";
  }
  
  if (shouldTrip) {
    tripRelay(reason);
  }
}

void tripRelay(String reason) {
  digitalWrite(RELAY_PIN, LOW);
  relayActive = false;
  isDeactivated = true;
  deactivatedAt = millis();
  
  addLog(reason, voltage, current);
  
  Serial.printf("⚠️  RELÉ DESARMADO: %s (V:%.1f A:%.2f)\n", reason.c_str(), voltage, current);
}

void rearmRelay() {
  digitalWrite(RELAY_PIN, HIGH);
  relayActive = true;
  isDeactivated = false;
  
  addLog("REARME", voltage, current);
  
  Serial.println("✅ RELÉ REARMADO");
}

// ==================== LOGS ====================
void addLog(String type, float v, float a) {
  if (logCount >= MAX_LOGS) {
    // Shift logs
    for (int i = 0; i < MAX_LOGS - 1; i++) {
      logs[i] = logs[i + 1];
    }
    logCount = MAX_LOGS - 1;
  }
  
  logs[logCount].timestamp = time(nullptr);
  logs[logCount].type = type;
  logs[logCount].voltage = v;
  logs[logCount].current = a;
  logCount++;
  
  saveLogs();
}

void saveLogs() {
  File file = SPIFFS.open("/logs.json", "w");
  if (!file) return;
  
  DynamicJsonDocument doc(8192);
  JsonArray arr = doc.createNestedArray("logs");
  
  for (int i = 0; i < logCount; i++) {
    JsonObject obj = arr.createNestedObject();
    obj["timestamp"] = logs[i].timestamp;
    obj["type"] = logs[i].type;
    obj["voltage"] = logs[i].voltage;
    obj["current"] = logs[i].current;
  }
  
  serializeJson(doc, file);
  file.close();
}

// ==================== LED STATUS ====================
void updateLED() {
  if (relayActive) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink >= 500) {
      lastBlink = millis();
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
  }
}

// ==================== WiFi ====================
void connectWiFi() {
  Serial.print("📡 Conectando WiFi...");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ WiFi conectado!");
    Serial.printf("🌐 IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("❌ WiFi falhou!");
  }
}

// ==================== SERVIDOR WEB ====================
void setupServer() {
  server.on("/api/data", HTTP_GET, handleData);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handleSetConfig);
  server.on("/api/logs", HTTP_GET, handleLogs);
  server.on("/api/relay", HTTP_POST, handleRelay);
  server.on("/api/logs/clear", HTTP_POST, handleClearLogs);
  
  server.begin();
  Serial.println("✅ Servidor web iniciado");
}

// API: Dados em tempo real
void handleData() {
  DynamicJsonDocument doc(512);
  
  doc["voltage"] = round(voltage * 100) / 100.0;
  doc["current"] = round(current * 100) / 100.0;
  doc["power"] = round(power);
  doc["energy"] = round(energy * 100) / 100.0;
  doc["frequency"] = round(frequency * 10) / 10.0;
  doc["relayActive"] = relayActive;
  doc["isDeactivated"] = isDeactivated;
  doc["timestamp"] = time(nullptr);
  
  if (isDeactivated) {
    long remaining = (deactivatedAt + deactivationTime * 1000) - millis();
    doc["timeToReactivate"] = max(0L, remaining / 1000);
  }
  
  String response;
  serializeJson(doc, response);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", response);
}

// API: Obter configurações
void handleGetConfig() {
  DynamicJsonDocument doc(512);
  
  doc["voltageMax"] = voltageMax;
  doc["voltageMin"] = voltageMin;
  doc["currentMax"] = currentMax;
  doc["deactivationTime"] = deactivationTime;
  
  String response;
  serializeJson(doc, response);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", response);
}

// API: Salvar configurações
void handleSetConfig() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(512);
    deserializeJson(doc, server.arg("plain"));
    
    if (doc.containsKey("voltageMax")) voltageMax = doc["voltageMax"];
    if (doc.containsKey("voltageMin")) voltageMin = doc["voltageMin"];
    if (doc.containsKey("currentMax")) currentMax = doc["currentMax"];
    if (doc.containsKey("deactivationTime")) deactivationTime = doc["deactivationTime"];
    
    Serial.println("⚙️  Configurações atualizadas");
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(400, "text/plain", "Dados inválidos");
  }
}

// API: Obter logs
void handleLogs() {
  DynamicJsonDocument doc(8192);
  JsonArray arr = doc.createNestedArray("logs");
  
  for (int i = logCount - 1; i >= 0; i--) {
    JsonObject obj = arr.createNestedObject();
    obj["timestamp"] = logs[i].timestamp;
    obj["type"] = logs[i].type;
    obj["voltage"] = logs[i].voltage;
    obj["current"] = logs[i].current;
  }
  
  String response;
  serializeJson(doc, response);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", response);
}

// API: Controlar relé
void handleRelay() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, server.arg("plain"));
    
    String action = doc["action"];
    
    if (action == "activate") {
      rearmRelay();
    } else if (action == "deactivate") {
      tripRelay("MANUAL");
    }
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  }
}

// API: Limpar logs
void handleClearLogs() {
  logCount = 0;
  SPIFFS.remove("/logs.json");
  
  Serial.println("🗑️  Logs apagados");
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}
