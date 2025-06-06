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
const char* serverUrl = "https://pruepas.onrender.com/api/historial";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -5 * 3600;
const int daylightOffset_sec = 0;

// Base de datos local de tarjetas autorizadas
struct TarjetaAutorizada {
  String uid;
  String nombre;
  String tipoAcceso;
};

const int MAX_TARJETAS = 50;
TarjetaAutorizada baseDeDatosTarjetas[MAX_TARJETAS] = {
  {"A1B2C3D4", "Juan Pérez", "admin"},
  {"B5C6D7E8", "María Gómez", "empleado"},
  {"C9D0E1F2", "Carlos Ruiz", "visitante"}
};

void setup() {
  Serial.begin(115200);
  while (!Serial);
  
  SPI.begin();
  mfrc522.PCD_Init();
  
  Serial.println("=== Lector RFID MIFARE Classic 1K Iniciado ===");
  conectarWiFi();
  configurarHora();
  
  // Mostrar base de datos cargada
  Serial.println("\nBase de Datos Cargada:");
  for (int i = 0; i < MAX_TARJETAS; i++) {
    if (baseDeDatosTarjetas[i].uid.length() > 0) {
      Serial.print("UID: " + baseDeDatosTarjetas[i].uid);
      Serial.print(" - Nombre: " + baseDeDatosTarjetas[i].nombre);
      Serial.println(" - Acceso: " + baseDeDatosTarjetas[i].tipoAcceso);
    }
  }
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

  // Buscar en la base de datos local
  bool autorizada = false;
  String nombre = "";
  String tipoAcceso = "";
  
  for (int i = 0; i < MAX_TARJETAS; i++) {
    if (baseDeDatosTarjetas[i].uid.equalsIgnoreCase(uid)) {
      autorizada = true;
      nombre = baseDeDatosTarjetas[i].nombre;
      tipoAcceso = baseDeDatosTarjetas[i].tipoAcceso;
      break;
    }
  }
  
  if (autorizada) {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║           ACCESO AUTORIZADO           ║");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.println("║ Nombre: " + nombre + String(40 - nombre.length() - 9, ' ') + "║");
    Serial.println("║ Tipo Acceso: " + tipoAcceso + String(40 - tipoAcceso.length() - 13, ' ') + "║");
    Serial.println("╚════════════════════════════════════════╝");
    
    // Registrar acceso en el servidor
    if (WiFi.status() == WL_CONNECTED) {
      registrarAccesoServidor(uid, nombre, tipoAcceso, fechaHora, true);
    }
  } else {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║           ACCESO DENEGADO             ║");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.println("║ Razón: Tarjeta no registrada          ║");
    Serial.println("╚════════════════════════════════════════╝");
    
    if (WiFi.status() == WL_CONNECTED) {
      registrarAccesoServidor(uid, "Desconocido", "no_autorizado", fechaHora, false);
    }
  }
}

void registrarAccesoServidor(String uid, String nombre, String tipoAcceso, String fechaHora, bool autorizado) {
  Serial.println("\n=== REGISTRANDO ACCESO EN SERVIDOR ===");
  
  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);

  StaticJsonDocument<300> doc;
  doc["uid"] = uid;
  doc["nombre"] = nombre;
  doc["tipo_acceso"] = tipoAcceso;
  doc["fecha_hora"] = fechaHora;
  doc["autorizado"] = autorizado;
  doc["fecha_iso"] = obtenerFechaHoraISO();
  doc["timestamp"] = millis();

  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println("URL: " + String(serverUrl));
  Serial.println("Datos JSON: " + jsonString);
  
  int httpCode = http.POST(jsonString);

  if (httpCode > 0) {
    String payload = http.getString();
    
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
      Serial.println("✓ Acceso registrado correctamente (" + String(httpCode) + ")");
    } else {
      Serial.println("⚠ Código de respuesta inesperado: " + String(httpCode));
      Serial.println("Respuesta servidor: " + payload);
    }
  } else {
    Serial.println("✗ Error de conexión HTTP: " + String(httpCode));
  }
  
  http.end();
  Serial.println("=======================================");
}