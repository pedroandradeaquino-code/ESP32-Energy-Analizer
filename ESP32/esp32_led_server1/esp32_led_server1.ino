#include <WiFi.h>
#include <WebServer.h>

// Configuração WiFi
const char* ssid = "SU33";
const char* password = "12345678";
const int LED_PIN = LED_BUILTIN; // Pino do LED (trocar se necessário)

WebServer server(80);

void setup() {
  Serial.begin(115200);
  delay(100);
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  Serial.println("\n\nIniciando ESP32...");
  
  // Conectar ao WiFi
  Serial.print("Conectando a WiFi: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFalha ao conectar no WiFi!");
  }
  
  // Configurar rotas do servidor web
  server.on("/", handleRoot);
  server.begin();
  
  Serial.println("Servidor web iniciado!");
}

void loop() {
  server.handleClient();
}

void handleRoot() {
  // Verificar parâmetro 'led'
  if (server.hasArg("led")) {
    String comando = server.arg("led");
    
    if (comando == "on") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED: LIGADO");
      server.send(200, "text/plain", "LED ligado");
    } 
    else if (comando == "off") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED: DESLIGADO");
      server.send(200, "text/plain", "LED desligado");
    }
    else {
      server.send(400, "text/plain", "Comando inválido. Use ?led=on ou ?led=off");
    }
  } 
  else {
    String html = "<!DOCTYPE html><html><head><title>ESP32 LED</title>";
    html += "<style>";
    html += "body { font-family: Arial; text-align: center; padding: 40px; background: #f0f0f0; }";
    html += "h1 { color: #333; margin-bottom: 20px; }";
    html += ".container { background: white; padding: 30px; border-radius: 10px; max-width: 400px; margin: 0 auto; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
    html += "button { padding: 15px 30px; margin: 10px; font-size: 16px; cursor: pointer; border: none; border-radius: 5px; font-weight: bold; transition: 0.3s; }";
    html += ".on { background-color: #4CAF50; color: white; }";
    html += ".on:hover { background-color: #45a049; }";
    html += ".off { background-color: #f44336; color: white; }";
    html += ".off:hover { background-color: #da190b; }";
    html += ".info { color: #666; font-size: 14px; margin-top: 20px; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>Controle de LED</h1>";
    html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
    html += "<button class='on' onclick='fetch(\\\"/?led=on\\\")'>Ligar LED</button>";
    html += "<button class='off' onclick='fetch(\\\"/?led=off\\\")'>Desligar LED</button>";
    html += "<div class='info'><p>Aguarde a resposta...</p></div>";
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
  }
}
