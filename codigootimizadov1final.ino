#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

const char* ssid = "SSID";
const char* password = "PASSWORD";

#define BOTtoken "API_TOKEN"

#ifdef ESP8266
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

#define SENSOR_PIN D15

int botRequestDelay = 100;
unsigned long lastTimeBotRan = 0;
unsigned long lastTimeNoWaterMessageSent = 0;
bool waterEmptyNotified = false;
bool waterAvailableNotified = false;
bool autoMessagesEnabled = true;

const String WATER_EMPTY_MESSAGE = "(Mensagem Automática) Acabou a água no primeiro andar do Ci, realize a troca do galão.";
const String WATER_AVAILABLE_MESSAGE = "(Mensagem Automática) Há água, o galão do primeiro andar do Ci acabou de ser trocado!";

void setup() {
  Serial.begin(115200);

#ifdef ESP8266
  configTime(0, 0, "pool.ntp.org");
  client.setTrustAnchors(&cert);
#endif

  pinMode(SENSOR_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
#ifdef ESP32
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
#endif
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println(WiFi.localIP());
}

void handleAutoMessages(String chat_id, bool enable) {
  autoMessagesEnabled = enable;
  bot.sendMessage(chat_id, (enable ? "Mensagens automáticas ativadas." : "Mensagens automáticas desativadas."), "");
}

void processAutoMessages() {
  if (!autoMessagesEnabled)
    return;

  int sensorValue = digitalRead(SENSOR_PIN);
  unsigned long currentTime = millis();
  
  // Verificar se o sensor mudou de 1 para 0
  if (sensorValue == 0 && waterAvailableNotified) {
    waterAvailableNotified = false;
  }

  if (sensorValue == 0) {
    if (!waterEmptyNotified || (currentTime - lastTimeNoWaterMessageSent >= 600000)) {
      bot.sendMessage(String(bot.messages[0].chat_id), WATER_EMPTY_MESSAGE, "");
      waterEmptyNotified = true;
      lastTimeNoWaterMessageSent = currentTime;
    }
  } else {
    if (!waterAvailableNotified) {
      bot.sendMessage(String(bot.messages[0].chat_id), WATER_AVAILABLE_MESSAGE, "");
      waterAvailableNotified = true;
      waterEmptyNotified = false; 
    }
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    if (text == "/start") {
      String welcomeMessage = "Bem-vindo ao GAMA!\n";
      welcomeMessage += "Use o comando /verificar para checar o estado do sensor, /ativarauto para ativar as atualizações automáticas e /desativarauto para desativá-las.";
      bot.sendMessage(chat_id, welcomeMessage, "");
      continue;
    }

    if (text == "/verificar") {
      int sensorValue = digitalRead(SENSOR_PIN);
      bot.sendMessage(chat_id, (sensorValue == 1 ? "Há água no primeiro andar do Ci, lembre-se de se hidratar!" : "Acabou a água no primeiro andar do Ci."), "");
    }

    if (text == "/ativarauto") {
      handleAutoMessages(chat_id, true);
    }

    if (text == "/desativarauto") {
      handleAutoMessages(chat_id, false);
    }
  }
}

void loop() {
  if (millis() > lastTimeBotRan + botRequestDelay) {
    processAutoMessages();

    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    if (numNewMessages) {
      handleNewMessages(numNewMessages);
    }

    lastTimeBotRan = millis();
  }
  delay(100);
}