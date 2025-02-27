
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <EEPROM.h>

MDNSResponder mdns;
ESP8266WebServer server(80);
int Led = 13;
int Relay = 2;
int button = 0;
bool relayState = false;
int relayStatus;
bool lastButtonState = LOW;
bool currentButtonState;
unsigned long buttonPressTime = 0;
bool isButtonHeld = false;
unsigned long startTime = 0;

// Function to save IP to EEPROM
void saveIPToEEPROM(IPAddress ip) {
  for (int i = 0; i < 4; i++) {
    EEPROM.write(i, ip[i]);
  }
  EEPROM.commit();
  Serial.print("IP saved to EEPROM: ");
  Serial.println(ip);
}

// Function to read IP from EEPROM
IPAddress readIPFromEEPROM() {
  IPAddress ip;
  for (int i = 0; i < 4; i++) {
    ip[i] = EEPROM.read(i);
  }
  return ip;
}

// Function to clear EEPROM
void clearEEPROM() {
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("EEPROM cleared.");
}
IPAddress staticIP, staticGateway, staticSubnet, staticDNS; 

void setup(void) {
  pinMode(Led, OUTPUT);
  pinMode(Relay, OUTPUT);
  digitalWrite(Relay, 0);
  pinMode(button, INPUT_PULLUP);
  relayState = false;
  relayStatus = 0;
  Serial.begin(115200);
  WiFiManager wifiManager;

  EEPROM.begin(512);

  IPAddress savedIP = readIPFromEEPROM();

  if (savedIP != IPAddress(0, 0, 0, 0)) {
    staticIP = savedIP;
    Serial.print("IP dibaca dari EEPROM: ");
    Serial.println(savedIP);
  } else {
    // Jika IP di EEPROM tidak valid, gunakan IP default
    Serial.println("Tidak ada IP di EEPROM, menggunakan IP default.");
    staticIP = IPAddress(192, 168, 0, 0); // IP default yang kamu inginkan
    staticGateway = IPAddress(192, 168, 0, 1); // Pastikan gateway yang benar
    staticSubnet = IPAddress(255, 255, 255, 0); // Pastikan subnet yang benar
  }

  // Set static IP untuk mode AP
  IPAddress apIP(192, 168, 123, 1);
  IPAddress apGateway(192, 168, 123, 1);
  IPAddress apSubnet(255, 255, 255, 0);
  wifiManager.setAPStaticIPConfig(apIP, apGateway, apSubnet);

  // Matikan DHCP dan gunakan static IP untuk mode STA
  wifiManager.setSTAStaticIPConfig(staticIP, staticGateway, staticSubnet, staticDNS);

  // Timeout WiFiManager
  wifiManager.setTimeout(120);

  // Auto connect dengan kredensial
  if (!wifiManager.autoConnect("BREAK MASTER", "$billpool24")) {
    Serial.println("Gagal terkoneksi atau timeout.");
    delay(3000);
    ESP.restart();
  }

  // Simpan IP ke EEPROM setelah berhasil terhubung ke WiFi
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Terhubung ke WiFi.");
    analogWrite(Led, 20);
    saveIPToEEPROM(WiFi.localIP()); // Simpan IP yang berhasil ke EEPROM
  } else {
    Serial.println("Gagal terhubung atau timeout. Berjalan dalam mode AP.");
    analogWrite(Led, 0);
  }

  // MDNS setup
  if (mdns.begin("BREAK MASTER", WiFi.localIP())) {
    Serial.println("MDNS responder dimulai");
  }
  // Web server setup
  server.on("/", []()
            { server.send(200, "text/plain", "Send /on or /off to control the relay."); });

  // Relay ON command
  server.on("/on", []()
            {
    if (server.hasArg("password") && server.arg("password") == "1d2084d04e5f80725466a21434438ba2") {
        relayState = true;
        digitalWrite(Relay, HIGH);
        analogWrite(Led, 0);   
        server.send(200, "text/plain", "Relay is ON");
    } else {
        server.send(403, "text/plain", "Unauthorized: Incorrect password");
    } });

  // Relay OFF command
  server.on("/off", []()
            {
    if (server.hasArg("password") && server.arg("password") == "1d2084d04e5f80725466a21434438ba2") {
        relayState = false;
        digitalWrite(Relay, LOW);
        analogWrite(Led, 1023);
        server.send(200, "text/plain", "Relay is OFF");
    } else {
        server.send(403, "text/plain", "Unauthorized: Incorrect password");
    } });

  // Route to provide WiFi status
  server.on("/status", []()
            {
  if (server.hasArg("password")) {
    String receivedPassword = server.arg("password");
    if (receivedPassword == "1d2084d04e5f80725466a21434438ba2") {
      int wifiStatus = (WiFi.status() == WL_CONNECTED) ? 1 : 0;
      server.send(200, "text/plain", String(wifiStatus));
    } else {
      server.send(401, "text/plain", "Unauthorized");
    }
  } else {
    server.send(400, "text/plain", "Bad Request");
  } });
  
//   server.on("/duration", []() {
//   // Cek apakah parameter password disertakan
//   if (server.hasArg("password") && server.arg("password") == "1d2084d04e5f80725466a21434438ba2") {
//     if (relayState) {
//       if (startTime == 0) {
//         startTime = millis();
//       }
      
//       // Hitung durasi dalam detik
//       unsigned long elapsedTime = millis() - startTime;
//       int durationSeconds = elapsedTime / 1000; // Konversi dari milidetik ke detik
      
//       // Konversi durasi ke jam, menit, dan detik
//       int hours = durationSeconds / 3600;
//       int minutes = (durationSeconds % 3600) / 60;
//       int seconds = durationSeconds % 60;
      
//       // Format durasi dalam jam:menit:detik
//       String durationStr = String(hours) + " : " + String(minutes) + " : " + String(seconds) ;
      
//       // Cetak durasi ke Serial Monitor
//       Serial.println("Relay ON for " + durationStr + ".");
      
//       server.send(200, "text/plain", durationStr);
//     } else {
//       // Jika relay OFF, set startTime ke 0
//       startTime = 0;
//       server.send(200, "text/plain", "TIME OFF");
//     }
//   } else {
//     // Jika password salah atau tidak disertakan
//     server.send(403, "text/plain", "Unauthorized access. Invalid or missing password.");
//   }
// });


 server.on("/duration", []() {
  // Cek apakah parameter password disertakan
  if (server.hasArg("password") && server.arg("password") == "1d2084d04e5f80725466a21434438ba2") {
    if (relayState) {
      if (startTime == 0) {
        startTime = millis();
      }
      
      // Hitung durasi dalam detik
      unsigned long elapsedTime = millis() - startTime;
      int durationSeconds = elapsedTime / 1000; // Konversi dari milidetik ke detik
       // Cetak durasi ke Serial Monitor
      Serial.println("Relay ON for " + String(durationSeconds) + " second(s).");
      server.send(200, "text/plain", "Relay ON for " + String(durationSeconds) + " second(s).");
    } else {
      // Jika relay OFF, set startTime ke 0
      startTime = 0;
      server.send(200, "text/plain", "Relay is OFF, no duration to report.");
    }
  } else {
    // Jika password salah atau tidak disertakan
    server.send(403, "text/plain", "Unauthorized access. Invalid or missing password.");
  }
});

  // Route to provide relay status
  server.on("/relayStatus", []()
            {
  if (server.hasArg("password")) {
    String receivedPassword = server.arg("password");
    if (receivedPassword == "1d2084d04e5f80725466a21434438ba2") {
      int relayStatus = relayState ? 1 : 0; // 1 if relay is ON, 0 if relay is OFF
      server.send(200, "text/plain", String(relayStatus));
    } else {
      server.send(401, "text/plain", "Unauthorized");
    }
  } else {
    server.send(400, "text/plain", "Bad Request");
  } });

  // Warning route: check relayState, if ON, turn off for specified seconds then turn it back on
  // /warning?duration=10&password=y"1d2084d04e5f80725466a21434438ba2"
  server.on("/warning", []()
            {
  if (server.hasArg("password") && server.arg("password") ==  "1d2084d04e5f80725466a21434438ba2") 
  { // Check if password parameter is present and correct
    if (relayState)
    { // Check if the relay is ON
      if (server.hasArg("duration"))
      {
        int duration = server.arg("duration").toInt(); // Get duration from parameter
        digitalWrite(Relay, LOW);
        analogWrite(Led, 1023);
        delay(duration * 1000);
        digitalWrite(Relay, HIGH);
        analogWrite(Led, 0);
        relayState = true;
        server.send(200, "text/plain", "Relay was ON, turned off for " + String(duration) + " seconds, then turned back on.");
      }
      else
      {
        server.send(400, "text/plain", "Missing duration parameter.");
      }
    }
    else
    {
      // If relay is OFF, no action is needed
      server.send(200, "text/plain", "Relay is already OFF, no action taken.");
    }
  } 
  else 
  {
    server.send(403, "text/plain", "Unauthorized access. Invalid or missing password."); // Send error if password is incorrect
  } });

  server.begin();
  Serial.println("HTTP server started");

  relayStatus = relayState ? 1 : 0;

}

void loop(void) {
  server.handleClient();

  currentButtonState = digitalRead(button);

  // Check if button is pressed (button is active low)
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    buttonPressTime = millis(); // Store the time the button was pressed
    isButtonHeld = false;       // Reset the flag
  }

  // If button is being held
  if (currentButtonState == LOW && millis() - buttonPressTime > 5000 && !isButtonHeld) {
    Serial.println("Button held for 5 seconds. Resetting WiFi credentials...");
    WiFiManager wifiManager;
    wifiManager.resetSettings(); // Erase WiFi credentials
    clearEEPROM();
    ESP.restart();       // Restart the ESP to enter AP mode
    isButtonHeld = true; // Set the flag to prevent repeated action
  }

  // // Button is released
  if (currentButtonState == LOW && lastButtonState == HIGH)
  {
    delay(50);
    if (digitalRead(button) == LOW && !isButtonHeld)
    {
      relayState = !relayState;
      digitalWrite(Relay, relayState ? LOW : HIGH);
    }
  }

  lastButtonState = currentButtonState;
}