#define DEBUG true

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>

#define SERVICE_UUID              "12345678-1234-1234-1234-1234567890ab"
#define UMIDADE_CHAR_UUID         "abcd1234-5678-1234-5678-abcdef123456"
#define BUZZER_CHAR_UUID          "buzz4321-5678-1234-5678-abcdef654321"
#define SENSOR_CONFIG_CHAR_UUID   "conf9876-5678-1234-5678-abcdef456789"
#define LOCATION_CHAR_UUID        "loct5678-5678-1234-5678-abcdef123456"
#define POWER_MODE_CHAR_UUID      "powr3456-5678-1234-5678-abcdef123456"
#define HISTORICO_CHAR_UUID       "hist5432-5678-1234-5678-abcdef123456"
#define ALERTA_VISUAL_CHAR_UUID   "alrt6789-5678-1234-5678-abcdef123456"

#define SENSOR_1 34
#define SENSOR_2 35
#define BUZZER   27

BLECharacteristic *umidadeChar;
BLECharacteristic *buzzerChar;
BLECharacteristic *sensorConfigChar;
BLECharacteristic *locationChar;
BLECharacteristic *powerModeChar;
BLECharacteristic *historicoChar;
BLECharacteristic *alertaVisualChar;

Preferences preferences;
bool deviceConnected = false;
bool buzzerAtivo = false;
bool modoEco = false;

const int MAX_HIST = 10;
int hist1[MAX_HIST];
int hist2[MAX_HIST];
int histIndex = 0;

unsigned long lastSensorMillis = 0;
const unsigned long sensorInterval = 3000;
unsigned long buzzerOnTime = 0;
const unsigned long buzzerDuration = 5000;

void atualizarHistorico(int u1, int u2) {
  hist1[histIndex] = u1;
  hist2[histIndex] = u2;
  histIndex = (histIndex + 1) % MAX_HIST;
}

String formatarHistorico() {
  String hist = "";
  for (int i = 0; i < MAX_HIST; ++i) {
    hist += String(hist1[i]) + "," + String(hist2[i]);
    if (i < MAX_HIST - 1) hist += ";";
  }
  return hist;
}

void lerSensores(int &u1, int &u2) {
  u1 = analogRead(SENSOR_1);
  u2 = analogRead(SENSOR_2);
}

void verificarAlerta(int u1, int u2) {
  if (u1 < 300 || u2 < 300) {
    digitalWrite(BUZZER, HIGH);
    buzzerAtivo = true;
    buzzerOnTime = millis();
  }
}

void tratarComandoBLE(std::string comando) {
  if (comando == "desligar_buzzer") {
    digitalWrite(BUZZER, LOW);
    buzzerAtivo = false;
  } else if (comando.rfind("set_loc:", 0) == 0) {
    std::string local = comando.substr(8);
    if (local.length() < 30 && local.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 _-:") == std::string::npos) {
      preferences.putString("local", local.c_str());
      locationChar->setValue(local);
    }
  } else if (comando == "modo_economia") {
    preferences.putBool("eco", true);
    powerModeChar->setValue("Eco");
    powerModeChar->notify();
    modoEco = true;
  } else if (comando == "modo_normal") {
    preferences.putBool("eco", false);
    powerModeChar->setValue("Normal");
    powerModeChar->notify();
    modoEco = false;
  }
}

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    pServer->getAdvertising()->start();
  }
};

class ComandoCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) {
    std::string cmd = pChar->getValue();
    tratarComandoBLE(cmd);
  }
};

void setup() {
#if DEBUG
  Serial.begin(115200);
#endif
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  preferences.begin("config", false);
  modoEco = preferences.getBool("eco", false);

  String devName = preferences.getString("devname", "ESP32_UMID_SENSOR");
  BLEDevice::init(devName.c_str());

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  umidadeChar = pService->createCharacteristic(UMIDADE_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  umidadeChar->addDescriptor(new BLE2902());

  buzzerChar = pService->createCharacteristic(BUZZER_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  buzzerChar->addDescriptor(new BLE2902());

  sensorConfigChar = pService->createCharacteristic(SENSOR_CONFIG_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
  sensorConfigChar->setCallbacks(new ComandoCallback());

  locationChar = pService->createCharacteristic(LOCATION_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
  locationChar->setValue(preferences.getString("local", "Indefinido").c_str());

  powerModeChar = pService->createCharacteristic(POWER_MODE_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  powerModeChar->setValue(modoEco ? "Eco" : "Normal");

  historicoChar = pService->createCharacteristic(HISTORICO_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
  alertaVisualChar = pService->createCharacteristic(ALERTA_VISUAL_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);

  pService->start();
  pServer->getAdvertising()->start();
#if DEBUG
  Serial.println("BLE iniciado.");
#endif
}

void loop() {
  unsigned long now = millis();

  if (buzzerAtivo && now - buzzerOnTime > buzzerDuration) {
    digitalWrite(BUZZER, LOW);
    buzzerAtivo = false;
  }

  if (now - lastSensorMillis >= sensorInterval) {
    lastSensorMillis = now;
    int u1, u2;
    lerSensores(u1, u2);
    atualizarHistorico(u1, u2);
    verificarAlerta(u1, u2);

    if (deviceConnected) {
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "%d,%d", u1, u2);
      umidadeChar->setValue(buffer);
      umidadeChar->notify();

      buzzerChar->setValue(buzzerAtivo ? "1" : "0");
      buzzerChar->notify();

      historicoChar->setValue(formatarHistorico().c_str());
    }
  }
}
