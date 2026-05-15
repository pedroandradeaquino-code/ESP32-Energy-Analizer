#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <PZEM004Tv30.h>
#include <Preferences.h>
#include <ESPmDNS.h>

#define RELAY_PIN      5
#define LED_PIN        2

#define PZEM_RX_PIN   16
#define PZEM_TX_PIN   17

#define RELAY_ON      HIGH
#define RELAY_OFF     LOW

#define MAX_LOGS      40

#define WIFI_TIMEOUT  30000
#define PZEM_TIMEOUT  10000

#define SENSOR_INTERVAL     1000
#define WEB_UPDATE_INTERVAL 1000

#define INVALID_LIMIT 5

HardwareSerial pzemSerial(2);
PZEM004Tv30 pzem(pzemSerial, PZEM_RX_PIN, PZEM_TX_PIN);

WebServer server(80);
Preferences prefs;

struct ElectricalData {
    float voltage;
    float current;
    float power;
    float energy;
    float frequency;
    float pf;
};

struct ProtectionConfig {
    float maxVoltage;
    float minVoltage;
    float maxCurrent;
    float minCurrent;

    uint32_t tripDelay;
    uint32_t rearmDelay;
};

ElectricalData dataFiltered;

ProtectionConfig config;

char logs[MAX_LOGS][120];
uint8_t logIndex = 0;

bool relayState = false;
bool protectionTripped = false;
bool pzemConnected = false;
bool wifiConnected = false;
bool systemCritical = false;

uint32_t lastSensorRead = 0;
uint32_t lastWiFiCheck = 0;
uint32_t lastPzemOk = 0;
uint32_t protectionStart = 0;
uint32_t relayTripTime = 0;

uint8_t invalidReadings = 0;

float voltageBuffer[5] = {0};
float currentBuffer[5] = {0};
float powerBuffer[5] = {0};

uint8_t filterIndex = 0;

const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Proteção Elétrica ESP32</title>

<style>

body{
    margin:0;
    background:#0f1115;
    color:#fff;
    font-family:Arial;
}

.container{
    max-width:1200px;
    margin:auto;
    padding:20px;
}

.card{
    background:#1b1f27;
    border-radius:12px;
    padding:20px;
    margin-bottom:20px;
    box-shadow:0 0 10px rgba(0,0,0,.3);
}

.grid{
    display:grid;
    grid-template-columns:repeat(auto-fit,minmax(220px,1fr));
    gap:20px;
}

.value{
    font-size:2em;
    margin-top:10px;
}

.ok{
    color:#00ff88;
}

.bad{
    color:#ff4444;
}

button{
    background:#2962ff;
    color:#fff;
    border:none;
    padding:12px 20px;
    border-radius:8px;
    cursor:pointer;
    margin-right:10px;
}

button:hover{
    opacity:.9;
}

pre{
    white-space:pre-wrap;
    font-size:12px;
}

.status{
    display:flex;
    gap:10px;
    flex-wrap:wrap;
}

.badge{
    padding:10px;
    border-radius:8px;
    background:#222;
}

</style>
</head>

<body>

<div class="container">

<h1>Proteção Elétrica ESP32</h1>

<div class="status">
<div class="badge" id="relayStatus">Relé</div>
<div class="badge" id="protectStatus">Proteção</div>
<div class="badge" id="wifiStatus">WiFi</div>
<div class="badge" id="pzemStatus">PZEM</div>
</div>

<div class="grid">

<div class="card">
<h3>Tensão</h3>
<div class="value" id="voltage">0</div>
</div>

<div class="card">
<h3>Corrente</h3>
<div class="value" id="current">0</div>
</div>

<div class="card">
<h3>Potência</h3>
<div class="value" id="power">0</div>
</div>

<div class="card">
<h3>Energia</h3>
<div class="value" id="energy">0</div>
</div>

<div class="card">
<h3>Frequência</h3>
<div class="value" id="freq">0</div>
</div>

<div class="card">
<h3>Fator Potência</h3>
<div class="value" id="pf">0</div>
</div>

</div>

<div class="card">
<h3>Controle</h3>

<button onclick="rearm()">Rearmar</button>
<button onclick="resetAlarms()">Reset Alarmes</button>

</div>

<div class="card">
<h3>Logs</h3>
<pre id="logs"></pre>
</div>

</div>

<script>

async function updateData(){

    try{

        const response = await fetch('/api/status');
        const data = await response.json();

        document.getElementById('voltage').innerHTML =
            data.voltage.toFixed(1)+' V';

        document.getElementById('current').innerHTML =
            data.current.toFixed(2)+' A';

        document.getElementById('power').innerHTML =
            data.power.toFixed(1)+' W';

        document.getElementById('energy').innerHTML =
            data.energy.toFixed(3)+' kWh';

        document.getElementById('freq').innerHTML =
            data.frequency.toFixed(1)+' Hz';

        document.getElementById('pf').innerHTML =
            data.pf.toFixed(2);

        document.getElementById('relayStatus').innerHTML =
            data.relay ? 'Relé: LIGADO' : 'Relé: DESLIGADO';

        document.getElementById('relayStatus').className =
            data.relay ? 'badge ok' : 'badge bad';

        document.getElementById('protectStatus').innerHTML =
            data.tripped ? 'PROTEÇÃO ATIVA' : 'Sistema Normal';

        document.getElementById('protectStatus').className =
            data.tripped ? 'badge bad' : 'badge ok';

        document.getElementById('wifiStatus').innerHTML =
            data.wifi ? 'WiFi OK' : 'WiFi OFF';

        document.getElementById('pzemStatus').innerHTML =
            data.pzem ? 'PZEM OK' : 'PZEM FAIL';

    }catch(e){}

}

async function updateLogs(){

    try{

        const response = await fetch('/api/logs');
        const logs = await response.json();

        let txt='';

        logs.forEach(l=>{
            txt += l + '\n';
        });

        document.getElementById('logs').innerText = txt;

    }catch(e){}

}

async function rearm(){
    await fetch('/api/rele?state=on');
}

async function resetAlarms(){
    await fetch('/api/reset');
}

setInterval(updateData,1000);
setInterval(updateLogs,2000);

updateData();
updateLogs();

</script>

</body>
</html>
)rawliteral";

void addLog(const char* msg){

    strncpy(logs[logIndex], msg, sizeof(logs[0])-1);
    logs[logIndex][sizeof(logs[0])-1] = '\0';

    logIndex++;

    if(logIndex >= MAX_LOGS)
        logIndex = 0;
}

void setRelay(bool state){

    relayState = state;

    digitalWrite(RELAY_PIN,
        state ? RELAY_ON : RELAY_OFF);

    digitalWrite(LED_PIN,
        state ? HIGH : LOW);
}

bool validReading(float value, float minVal, float maxVal){

    if(isnan(value))
        return false;

    if(value < minVal || value > maxVal)
        return false;

    return true;
}

float movingAverage(float *buffer){

    float sum = 0;

    for(uint8_t i=0;i<5;i++)
        sum += buffer[i];

    return sum / 5.0f;
}

bool readPZEM(){

    float v = pzem.voltage();
    float c = pzem.current();
    float p = pzem.power();
    float e = pzem.energy();
    float f = pzem.frequency();
    float pf = pzem.pf();

    if(
        !validReading(v, 80, 300) ||
        !validReading(c, 0, 100) ||
        !validReading(p, 0, 25000) ||
        !validReading(f, 45, 70) ||
        !validReading(pf, 0, 1)
    ){

        invalidReadings++;

        if(invalidReadings >= INVALID_LIMIT){

            addLog("Falha leitura PZEM");
            return false;
        }

        return true;
    }

    invalidReadings = 0;

    voltageBuffer[filterIndex] = v;
    currentBuffer[filterIndex] = c;
    powerBuffer[filterIndex] = p;

    filterIndex++;

    if(filterIndex >= 5)
        filterIndex = 0;

    dataFiltered.voltage = movingAverage(voltageBuffer);
    dataFiltered.current = movingAverage(currentBuffer);
    dataFiltered.power = movingAverage(powerBuffer);

    dataFiltered.energy = e;
    dataFiltered.frequency = f;
    dataFiltered.pf = pf;

    lastPzemOk = millis();

    return true;
}

void checkProtection(){

    bool fault = false;

    if(dataFiltered.voltage > config.maxVoltage){

        addLog("Sobretensao");
        fault = true;
    }

    if(dataFiltered.voltage < config.minVoltage){

        addLog("Subtensao");
        fault = true;
    }

    if(dataFiltered.current > config.maxCurrent){

        addLog("Sobrecorrente");
        fault = true;
    }

    if(dataFiltered.current < config.minCurrent){

        addLog("Subcorrente");
        fault = true;
    }

    if(millis() - lastPzemOk > PZEM_TIMEOUT){

        addLog("Timeout PZEM");
        fault = true;
    }

    if(fault){

        if(protectionStart == 0)
            protectionStart = millis();

        if(millis() - protectionStart >= config.tripDelay){

            protectionTripped = true;

            setRelay(false);

            relayTripTime = millis();
        }

    }else{

        protectionStart = 0;
    }
}

void connectWiFi(){

    WiFi.mode(WIFI_STA);

    WiFi.begin("SU33", "12345678");

    uint32_t start = millis();

    while(WiFi.status() != WL_CONNECTED &&
          millis() - start < WIFI_TIMEOUT){

        delay(100);
    }

    wifiConnected = WiFi.status() == WL_CONNECTED;

    if(wifiConnected){

        addLog("WiFi conectado");

        MDNS.begin("protecaoesp32");
    }else{

        addLog("Falha WiFi");
    }
}

void handleRoot(){

    server.send_P(200, "text/html", MAIN_page);
}

void handleStatus(){

    StaticJsonDocument<512> doc;

    doc["voltage"] = dataFiltered.voltage;
    doc["current"] = dataFiltered.current;
    doc["power"] = dataFiltered.power;
    doc["energy"] = dataFiltered.energy;
    doc["frequency"] = dataFiltered.frequency;
    doc["pf"] = dataFiltered.pf;

    doc["relay"] = relayState;
    doc["tripped"] = protectionTripped;
    doc["wifi"] = wifiConnected;
    doc["pzem"] = pzemConnected;

    String out;
    serializeJson(doc, out);

    server.send(200, "application/json", out);
}

void handleLogs(){

    StaticJsonDocument<4096> doc;

    JsonArray arr = doc.to<JsonArray>();

    for(uint8_t i=0;i<MAX_LOGS;i++){

        if(strlen(logs[i]) > 0)
            arr.add(logs[i]);
    }

    String out;
    serializeJson(doc, out);

    server.send(200, "application/json", out);
}

void handleRelay(){

    if(server.hasArg("state")){

        String st = server.arg("state");

        if(st == "on"){

            if(protectionTripped){

                if(millis() - relayTripTime >= config.rearmDelay){

                    protectionTripped = false;

                    setRelay(true);

                    addLog("Rearme manual");
                }

            }else{

                setRelay(true);
            }

        }else{

            setRelay(false);
        }
    }

    server.send(200, "application/json",
                "{\"ok\":true}");
}

void handleReset(){

    protectionTripped = false;

    addLog("Alarmes resetados");

    server.send(200, "application/json",
                "{\"reset\":true}");
}

void handleConfig(){

    if(server.method() == HTTP_POST){

        if(server.hasArg("plain")){

            StaticJsonDocument<512> doc;

            deserializeJson(doc, server.arg("plain"));

            config.maxVoltage = doc["maxVoltage"];
            config.minVoltage = doc["minVoltage"];
            config.maxCurrent = doc["maxCurrent"];
            config.minCurrent = doc["minCurrent"];

            config.tripDelay = doc["tripDelay"];
            config.rearmDelay = doc["rearmDelay"];

            prefs.putBytes("cfg",
                           &config,
                           sizeof(config));

            addLog("Configuracao alterada");
        }
    }

    StaticJsonDocument<512> doc;

    doc["maxVoltage"] = config.maxVoltage;
    doc["minVoltage"] = config.minVoltage;
    doc["maxCurrent"] = config.maxCurrent;
    doc["minCurrent"] = config.minCurrent;

    doc["tripDelay"] = config.tripDelay;
    doc["rearmDelay"] = config.rearmDelay;

    String out;
    serializeJson(doc, out);

    server.send(200, "application/json", out);
}

void setupWebServer(){

    server.on("/", handleRoot);

    server.on("/api/status", handleStatus);

    server.on("/api/logs", handleLogs);

    server.on("/api/rele", handleRelay);

    server.on("/api/reset", handleReset);

    server.on("/api/config", HTTP_ANY, handleConfig);

    server.begin();
}

void loadConfig(){

    prefs.begin("protection", false);

    if(prefs.getBytesLength("cfg") ==
       sizeof(config)){

        prefs.getBytes("cfg",
                       &config,
                       sizeof(config));

    }else{

        config.maxVoltage = 245.0;
        config.minVoltage = 190.0;

        config.maxCurrent = 20.0;
        config.minCurrent = 0.0;

        config.tripDelay = 3000;
        config.rearmDelay = 10000;
    }
}

void watchdogLogic(){

    if(systemCritical){

        setRelay(false);
    }

    if(ESP.getFreeHeap() < 15000){

        addLog("Heap critica");

        setRelay(false);
    }
}

void setup(){

    Serial.begin(115200);

    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);

    setRelay(false);

    loadConfig();

    connectWiFi();

    setupWebServer();

    addLog("Sistema iniciado");
}

void loop(){

    server.handleClient();

    uint32_t now = millis();

    if(now - lastSensorRead >= SENSOR_INTERVAL){

        lastSensorRead = now;

        pzemConnected = readPZEM();

        checkProtection();
    }

    if(now - lastWiFiCheck >= 10000){

        lastWiFiCheck = now;

        if(WiFi.status() != WL_CONNECTED){

            wifiConnected = false;

            addLog("Reconectando WiFi");

            WiFi.disconnect();

            WiFi.reconnect();

        }else{

            wifiConnected = true;
        }
    }

    watchdogLogic();

    yield();
}