#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

#define RST_PIN 15
#define SS_PIN 5

MFRC522 mfrc522(SS_PIN, RST_PIN);

const char* ssid = "V:";
const char* password = "12345678";

// URLs de tu API
const char* verificarTarjetaUrl = "https://tu-api.onrender.com/api/verificar-tarjeta";
const char* registrarAccesoUrl = "https://tu-api.onrender.com/api/registrar-acceso";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -5 * 3600;
const int daylightOffset_sec = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  
  SPI.begin();
  mfrc522.PCD_Init();
  
  Serial.println("=== Lector RFID con Base de Datos Externa ===");
  conectarWiFi();
  configurarHora();
  
  Serial.println("\nSistema listo para leer tarjetas RFID");
}

void loop() {
  verificarConexionWiFi();
  
  if (!mfrc522.PICC_IsNewCardPresent()) {
    delay(100);
    return;
  }
  
  if (!mfrc522.PICC_ReadCardSerial()) {
    Serial.println("Error: No se pudo leer el serial de la tarjeta");
    delay(500);
    return;
  }

  procesarTarjeta();
  
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  
  Serial.println("\n--- Esperando próxima tarjeta ---");
  delay(3000);
}

// ===== FUNCIONES AUXILIARES =====

void conectarWiFi() {
  WiFi.begin(ssid, password);
  Serial.println("\nConectando a WiFi: " + String(ssid));
  
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 30) {
    delay(500);
    Serial.print(".");
    intentos++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n✗ Error: No se pudo conectar al WiFi");
  } else {
    Serial.println("\n✓ WiFi conectado correctamente");
    Serial.print("IP asignada: ");
    Serial.println(WiFi.localIP());
  }
  
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

void configurarHora() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Configurando hora desde servidor NTP...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    struct tm timeinfo;
    int intentos = 0;
    while (!getLocalTime(&timeinfo) && intentos < 10) {
      Serial.print(".");
      delay(1000);
      intentos++;
    }
    
    if (getLocalTime(&timeinfo)) {
      Serial.println("\n✓ Hora sincronizada correctamente");
      Serial.println("Hora actual: " + obtenerFechaHoraFormateada());
    } else {
      Serial.println("\n⚠ No se pudo sincronizar la hora");
    }
  } else {
    Serial.println("⚠ Sin WiFi - Usando hora interna del sistema");
  }
}

String obtenerFechaHoraFormateada() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Hora no disponible";
  }
  
  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

String obtenerFechaHoraISO() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "";
  }
  
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  return String(buffer);
}

void verificarConexionWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long ultimoIntento = 0;
    if (millis() - ultimoIntento > 30000) {
      Serial.println("WiFi desconectado. Reintentando...");
      WiFi.reconnect();
      ultimoIntento = millis();
    }
  }
}

String obtenerUID() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void procesarTarjeta() {
  String fechaHora = obtenerFechaHoraFormateada();
  String uid = obtenerUID();
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║           TARJETA DETECTADA            ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║ UID: " + uid + String(40 - uid.length() - 6, ' ') + "║");
  Serial.println("║ Fecha/Hora: " + fechaHora + String(40 - fechaHora.length() - 12, ' ') + "║");
  Serial.println("╚════════════════════════════════════════╝");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n✗ Sin conexión WiFi - No se puede verificar la tarjeta");
    return;
  }

  // Verificar tarjeta en la base de datos
  bool autorizada = verificarTarjetaEnBD(uid);
  
  if (autorizada) {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║           ACCESO AUTORIZADO            ║");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.println("║ UID: " + uid + String(40 - uid.length() - 6, ' ') + "║");
    Serial.println("╚════════════════════════════════════════╝");
    
    // Registrar acceso exitoso
    registrarAccesoEnBD(uid, fechaHora, true);
  } else {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║           ACCESO DENEGADO              ║");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.println("║ Razón: UID no autorizada               ║");
    Serial.println("║ UID: " + uid + String(40 - uid.length() - 6, ' ') + "║");
    Serial.println("╚════════════════════════════════════════╝");
    
    // Registrar intento de acceso denegado
    registrarAccesoEnBD(uid, fechaHora, false);
  }
}

bool verificarTarjetaEnBD(String uid) {
  Serial.println("\n=== VERIFICANDO UID EN BASE DE DATOS ===");
  
  HTTPClient http;
  http.begin(verificarTarjetaUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);

  StaticJsonDocument<200> doc;
  doc["uid"] = uid;

  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println("URL: " + String(verificarTarjetaUrl));
  Serial.println("Datos JSON: " + jsonString);
  
  int httpCode = http.POST(jsonString);
  bool autorizada = false;

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Código de respuesta: " + String(httpCode));
    Serial.println("Respuesta servidor: " + payload);
    
    if (httpCode == HTTP_CODE_OK) {
      StaticJsonDocument<300> respuesta;
      DeserializationError error = deserializeJson(respuesta, payload);
      
      if (!error) {
        autorizada = respuesta["autorizada"] | false;
        if (respuesta.containsKey("mensaje")) {
          Serial.println("Mensaje: " + String(respuesta["mensaje"].as<const char*>()));
        }
      } else {
        Serial.println("Error al parsear respuesta JSON");
      }
    }
  } else {
    Serial.println("✗ Error de conexión HTTP: " + String(httpCode));
  }
  
  http.end();
  Serial.println("========================================");
  
  return autorizada;
}

void registrarAccesoEnBD(String uid, String fechaHora, bool autorizado) {
  Serial.println("\n=== REGISTRANDO ACCESO EN BASE DE DATOS ===");
  
  HTTPClient http;
  http.begin(registrarAccesoUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);

  StaticJsonDocument<400> doc;
  doc["uid"] = uid;
  doc["fecha_hora"] = fechaHora;
  doc["fecha_iso"] = obtenerFechaHoraISO();
  doc["autorizado"] = autorizado;
  doc["timestamp"] = millis();

  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println("URL: " + String(registrarAccesoUrl));
  Serial.println("Datos JSON: " + jsonString);
  
  int httpCode = http.POST(jsonString);

  if (httpCode > 0) {
    String payload = http.getString();
    
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
      Serial.println("✓ Acceso registrado correctamente (" + String(httpCode) + ")");
      Serial.println("Respuesta: " + payload);
    } else {
      Serial.println("⚠ Código de respuesta inesperado: " + String(httpCode));
      Serial.println("Respuesta servidor: " + payload);
    }
  } else {
    Serial.println("✗ Error de conexión HTTP: " + String(httpCode));
  }
  
  http.end();
  Serial.println("===========================================");
}