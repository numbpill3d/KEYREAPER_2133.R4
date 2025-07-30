#include <WiFiS3.h>
#include <Wire.h>

// WiFi configuration - FIXED PASSWORD
const char* ssid = "KEYREAPER_2133.R4";
const char* password = "21332133";

// WiFi server
WiFiServer server(80);

// PN532 I2C address
#define PN532_I2C_ADDRESS 0x24

// Enhanced card history buffer with detailed data extraction
struct CardEntry {
  String uid;
  String cardType; 
  String timestamp;
  String rawHexData;
  String readableData;
  String blockData[16]; // Store up to 16 blocks of data
  int blocksRead;
  String accessLevel;
};

CardEntry cardBuffer[25]; // Fixed buffer size consistency
int bufferHead = 0;
int bufferCount = 0;

// System state
bool continuousScanning = false;
unsigned long lastScan = 0;
unsigned long scanInterval = 1000; // Slower for reliability
String systemStatus = "OPERATIONAL";
bool deepScanMode = true;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10); // Wait for serial
  
  Serial.println("[BOOT] KEYREAPER 2133.R4 initializing...");
  
  // Initialize I2C with proper pins
  Wire.begin();
  Wire.setClock(100000); // Slower I2C for reliability
  
  delay(1000); // Let I2C settle
  
  // Initialize PN532 with fixed sequence
  if (!initPN532()) {
    Serial.println("[CRITICAL] PN532 initialization failed - check wiring");
    systemStatus = "HARDWARE_FAULT";
  } else {
    Serial.println("[STATUS] PN532 operational - RFID surveillance active");
  }
  
  // Setup WiFi Access Point with correct password
  Serial.println("[NETWORK] Establishing access point...");
  
  WiFi.end();
  delay(2000);
  
  // Set static IP configuration
  WiFi.config(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  
  if (WiFi.beginAP(ssid, password) == WL_AP_LISTENING) {
    Serial.println("[NETWORK] Secure AP established - surveillance grid active");
    systemStatus = "SURVEILLANCE_ACTIVE";
  } else {
    Serial.println("[WARNING] AP failed - trying without password");
    WiFi.beginAP(ssid);
    systemStatus = "DEGRADED_MODE";
  }
  
  delay(3000);
  
  Serial.print("[NETWORK] AP Status: ");
  Serial.println(WiFi.status());
  Serial.print("[NETWORK] SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("[NETWORK] IP: ");
  Serial.println(WiFi.localIP());
  
  server.begin();
  Serial.println("[WEB] Command interface deployed on port 80");
  Serial.println("[SYSTEM] KEYREAPER 2133.R4 - Full operational status achieved");
}

void loop() {
  handleWebClients();
  
  if (continuousScanning && (millis() - lastScan > scanInterval)) {
    performEnhancedScan();
    lastScan = millis();
  }
  
  delay(10);
}

bool initPN532() {
  Serial.println("[INIT] Attempting PN532 initialization...");
  
  // Simplified wake-up sequence
  for (int attempts = 0; attempts < 5; attempts++) {
    Serial.print("[INIT] Attempt ");
    Serial.println(attempts + 1);
    
    // Send wake-up command
    Wire.beginTransmission(PN532_I2C_ADDRESS);
    Wire.write(0x55); Wire.write(0x55); Wire.write(0x00); Wire.write(0x00);
    Wire.write(0x00); Wire.write(0x00); Wire.write(0x00); Wire.write(0x00);
    Wire.write(0x00); Wire.write(0x00); Wire.write(0x00); Wire.write(0x00);
    Wire.write(0x00); Wire.write(0x00); Wire.write(0x00); Wire.write(0x00);
    
    if (Wire.endTransmission() == 0) {
      delay(500);
      
      // Send GetFirmwareVersion command
      Wire.beginTransmission(PN532_I2C_ADDRESS);
      Wire.write(0x00); Wire.write(0x00); Wire.write(0xFF);
      Wire.write(0x02); Wire.write(0xFE); Wire.write(0xD4);
      Wire.write(0x02); Wire.write(0x2A); Wire.write(0x00);
      
      if (Wire.endTransmission() == 0) {
        delay(500);
        
        // Try SAM configuration
        Wire.beginTransmission(PN532_I2C_ADDRESS);
        Wire.write(0x00); Wire.write(0x00); Wire.write(0xFF);
        Wire.write(0x05); Wire.write(0xFB); Wire.write(0xD4);
        Wire.write(0x14); Wire.write(0x01); Wire.write(0x17);
        Wire.write(0x00); Wire.write(0x00);
        
        if (Wire.endTransmission() == 0) {
          Serial.println("[INIT] PN532 responding - initialization successful");
          delay(500);
          return true;
        }
      }
    }
    delay(1000);
  }
  return false;
}

bool performEnhancedScan() {
  // Simplified InListPassiveTarget command for ISO14443A
  Wire.beginTransmission(PN532_I2C_ADDRESS);
  Wire.write(0x00); Wire.write(0x00); Wire.write(0xFF);
  Wire.write(0x04); Wire.write(0xFC); Wire.write(0xD4);
  Wire.write(0x4A); Wire.write(0x01); Wire.write(0x00);
  Wire.write(0xE1); Wire.write(0x00);
  
  if (Wire.endTransmission() != 0) {
    return false;
  }
  
  delay(200); // Give it time to scan
  
  // Request response
  Wire.requestFrom(PN532_I2C_ADDRESS, 30);
  
  if (Wire.available() < 15) {
    return false;  
  }
  
  uint8_t response[30];
  int bytesRead = 0;
  
  while (Wire.available() && bytesRead < 30) {
    response[bytesRead] = Wire.read();
    bytesRead++;
  }
  
  // Check for valid response
  if (bytesRead > 14 && response[6] == 0xD5 && response[7] == 0x4B && response[8] == 0x01) {
    int uidLength = response[12];
    
    if (uidLength > 0 && uidLength < 11 && (13 + uidLength) < bytesRead) {
      String uid = "";
      for (int i = 0; i < uidLength; i++) {
        if (response[13 + i] < 0x10) uid += "0";
        uid += String(response[13 + i], HEX);
      }
      uid.toUpperCase();
      
      String cardType = determineCardType(uidLength, response, bytesRead);
      String accessLevel = determineAccessLevel(response);
      
      // Create new card entry
      CardEntry newEntry;
      newEntry.uid = uid;
      newEntry.cardType = cardType;
      newEntry.timestamp = String(millis());
      newEntry.accessLevel = accessLevel;
      newEntry.blocksRead = 0;
      newEntry.rawHexData = "";
      newEntry.readableData = "";
      
      if (deepScanMode) {
        extractCardData(newEntry);
      }
      
      addEnhancedCardToBuffer(newEntry);
      
      Serial.print("[INTERCEPT] UID: ");
      Serial.print(uid);
      Serial.print(" | Type: ");
      Serial.print(cardType);
      Serial.print(" | Access: ");
      Serial.println(accessLevel);
      
      return true;
    }
  }
  
  return false;
}

String determineCardType(int uidLength, uint8_t* response, int bytesRead) {
  if (uidLength == 4) {
    // Check SAK for specific MIFARE variant
    if (bytesRead > 20) {
      uint8_t sak = response[20];
      if (sak == 0x08) return "MIFARE_CLASSIC_1K";
      if (sak == 0x18) return "MIFARE_CLASSIC_4K";
      if (sak == 0x28) return "MIFARE_CLASSIC_1K_EMULATED";
    }
    return "MIFARE_CLASSIC_1K";
  } else if (uidLength == 7) {
    return "MIFARE_ULTRALIGHT";
  } else if (uidLength == 10) {
    return "DESFIRE_EV1";
  }
  return "UNKNOWN_PROTOCOL";
}

String determineAccessLevel(uint8_t* response) {
  // Basic access level determination based on response patterns
  if (response[8] == 0x01) {
    if (response[12] == 4) return "STANDARD_ACCESS";
    if (response[12] == 7) return "ENHANCED_ACCESS";
    if (response[12] == 10) return "SECURE_ACCESS";
  }
  return "UNKNOWN_ACCESS";
}

void extractCardData(CardEntry& entry) {
  // Simplified data extraction - try to read a few blocks
  uint8_t defaultKey[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  
  for (int block = 1; block < 4 && entry.blocksRead < 3; block++) {
    if (authenticateAndReadBlock(block, defaultKey, entry)) {
      // Successfully read block
    }
  }
}

bool authenticateAndReadBlock(int blockNum, uint8_t* key, CardEntry& entry) {
  // Simplified authentication attempt
  Wire.beginTransmission(PN532_I2C_ADDRESS);
  Wire.write(0x00); Wire.write(0x00); Wire.write(0xFF);
  Wire.write(0x0F); Wire.write(0xF1); Wire.write(0xD4);
  Wire.write(0x40); Wire.write(0x01); Wire.write(0x60);
  Wire.write((uint8_t)blockNum);
  
  for (int i = 0; i < 6; i++) {
    Wire.write(key[i]);
  }
  
  Wire.write(0x00); Wire.write(0x00); Wire.write(0x00); Wire.write(0x00);
  
  if (Wire.endTransmission() != 0) return false;
  
  delay(100);
  
  // Try to read the block
  Wire.beginTransmission(PN532_I2C_ADDRESS);
  Wire.write(0x00); Wire.write(0x00); Wire.write(0xFF);
  Wire.write(0x05); Wire.write(0xFB); Wire.write(0xD4);
  Wire.write(0x40); Wire.write(0x01); Wire.write(0x30);
  Wire.write((uint8_t)blockNum); Wire.write(0x00);
  
  if (Wire.endTransmission() != 0) return false;
  
  delay(100);
  
  Wire.requestFrom(PN532_I2C_ADDRESS, 26);
  
  if (Wire.available() >= 22) {
    // Skip headers
    for (int i = 0; i < 8; i++) {
      if (Wire.available()) Wire.read();
    }
    
    String blockHex = "";
    String blockAscii = "";
    
    for (int i = 0; i < 16; i++) {
      if (Wire.available()) {
        uint8_t b = Wire.read();
        if (b < 0x10) blockHex += "0";
        blockHex += String(b, HEX);
        
        if (b >= 32 && b <= 126) {
          blockAscii += (char)b;
        } else {
          blockAscii += ".";
        }
      }
    }
    
    if (blockHex.length() > 0) {
      entry.blockData[entry.blocksRead] = "BLK" + String(blockNum) + ":" + blockHex + "|" + blockAscii;
      entry.blocksRead++;
      return true;
    }
  }
  
  return false;
}

void addEnhancedCardToBuffer(CardEntry& entry) {
  cardBuffer[bufferHead] = entry;
  bufferHead = (bufferHead + 1) % 25;
  if (bufferCount < 25) bufferCount++;
}

bool writeToCard(String hexData) {
  // First scan for a card
  if (!performEnhancedScan()) return false;
  
  // Prepare write data
  uint8_t writeData[16] = {0};
  int dataLen = min(32, (int)hexData.length()) / 2;
  
  for (int i = 0; i < dataLen; i++) {
    String byteString = hexData.substring(i * 2, i * 2 + 2);
    writeData[i] = (uint8_t)strtol(byteString.c_str(), NULL, 16);
  }
  
  uint8_t defaultKey[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  
  // Authenticate to block 4
  Wire.beginTransmission(PN532_I2C_ADDRESS);
  Wire.write(0x00); Wire.write(0x00); Wire.write(0xFF);
  Wire.write(0x0F); Wire.write(0xF1); Wire.write(0xD4);
  Wire.write(0x40); Wire.write(0x01); Wire.write(0x60);
  Wire.write(0x04); // Block 4
  
  for (int i = 0; i < 6; i++) {
    Wire.write(defaultKey[i]);
  }
  
  Wire.write(0x00); Wire.write(0x00); Wire.write(0x00); Wire.write(0x00);
  
  if (Wire.endTransmission() != 0) return false;
  
  delay(200);
  
  // Write to block 4
  Wire.beginTransmission(PN532_I2C_ADDRESS);
  Wire.write(0x00); Wire.write(0x00); Wire.write(0xFF);
  Wire.write(0x15); Wire.write(0xEB); Wire.write(0xD4);
  Wire.write(0x40); Wire.write(0x01); Wire.write(0xA0);
  Wire.write(0x04); // Block 4
  
  for (int i = 0; i < 16; i++) {
    Wire.write(writeData[i]);
  }
  
  Wire.write(0x00);
  
  return (Wire.endTransmission() == 0);
}

void handleWebClients() {
  WiFiClient client = server.available();
  if (!client) return;
  
  String request = "";
  String postData = "";
  bool isPost = false;
  
  // Read the request
  while (client.connected()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      line.trim();
      
      if (line.length() == 0) {
        // End of headers, read POST data if needed
        if (isPost && client.available()) {
          postData = client.readStringUntil('\n');
        }
        break;
      }
      
      if (request.length() == 0) {
        request = line;
        if (request.indexOf("POST") >= 0) {
          isPost = true;
        }
      }
    }
  }
  
  // Route the request
  if (request.indexOf("GET / ") >= 0) {
    sendEnhancedWebInterface(client);
  } else if (request.indexOf("GET /api/scan") >= 0) {
    handleManualScan(client);
  } else if (request.indexOf("GET /api/history") >= 0) {
    sendEnhancedCardHistory(client);
  } else if (request.indexOf("POST /api/write") >= 0) {
    handleEnhancedWriteRequest(client, postData);
  } else if (request.indexOf("GET /api/toggle") >= 0) {
    toggleContinuousScanning(client);
  } else if (request.indexOf("GET /api/status") >= 0) {
    sendSystemStatus(client);
  } else if (request.indexOf("GET /api/deepmode") >= 0) {
    toggleDeepScanMode(client);
  } else {
    send404(client);
  }
  
  delay(1);
  client.stop();
}

void sendEnhancedWebInterface(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  
  client.println("<!DOCTYPE html>");
  client.println("<html>");
  client.println("<head>");
  client.println("<title>KEYREAPER 2133.R4</title>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  client.println("<style>");
  client.println("* { box-sizing: border-box; margin: 0; padding: 0; }");
  client.println("body { font-family: 'Courier New', monospace; background: #000; color: #0f0; padding: 20px; min-height: 100vh; display: flex; flex-direction: column; }");
  client.println(".header { text-align: center; margin-bottom: 30px; }");
  client.println("h1 { color: #f00; font-size: 24px; margin-bottom: 10px; }");
  client.println(".panel { border: 2px solid #0f0; padding: 20px; margin: 10px 0; background: #111; border-radius: 5px; }");
  client.println("h2 { color: #fff; margin-bottom: 15px; font-size: 18px; }");
  client.println(".controls { display: flex; flex-wrap: wrap; gap: 10px; margin-bottom: 20px; }");
  client.println("button { background: #333; color: #0f0; border: 2px solid #0f0; padding: 12px 20px; cursor: pointer; font-family: inherit; border-radius: 3px; min-width: 120px; }");
  client.println("button:hover { background: #0f0; color: #000; }");
  client.println("button:active { background: #fff; }");
  client.println("input { background: #000; color: #0f0; border: 2px solid #0f0; padding: 12px; width: 100%; max-width: 300px; font-family: inherit; border-radius: 3px; }");
  client.println(".status { text-align: center; font-size: 16px; margin: 20px 0; padding: 15px; border: 1px solid #0f0; background: #222; border-radius: 3px; }");
  client.println(".history { max-height: 400px; overflow-y: auto; border: 2px solid #0f0; padding: 15px; background: #222; border-radius: 3px; }");
  client.println(".card { border-bottom: 1px solid #555; padding: 15px 0; }");
  client.println(".card:last-child { border-bottom: none; }");
  client.println(".uid { color: #fff; font-weight: bold; font-size: 16px; margin-bottom: 5px; }");
  client.println(".card-info { color: #0f0; font-size: 14px; }");
  client.println(".input-group { display: flex; flex-wrap: wrap; gap: 10px; align-items: center; }");
  client.println("@media (max-width: 768px) { .controls { flex-direction: column; } button, input { width: 100%; } }");
  client.println("</style>");
  client.println("</head>");
  client.println("<body>");
  
  client.println("<div class='header'>");
  client.println("<h1>KEYREAPER 2133.R4 - CLASSIFIED</h1>");
  client.println("</div>");
  
  client.println("<div class='panel'>");
  client.println("<h2>RFID CONTROL SYSTEM</h2>");
  client.println("<div class='controls'>");
  client.println("<button onclick='scan()'>MANUAL SCAN</button>");
  client.println("<button onclick='toggle()' id='toggleBtn'>START CONTINUOUS</button>");
  client.println("<button onclick='clearHistory()'>CLEAR HISTORY</button>");
  client.println("<button onclick='refreshStatus()'>REFRESH</button>");
  client.println("</div>");
  client.println("<div class='status' id='status'>SYSTEM READY</div>");
  client.println("<div class='input-group'>");
  client.println("<input type='text' id='hexInput' placeholder='HEX DATA TO WRITE (32 chars max)' maxlength='32'>");
  client.println("<button onclick='writeCard()'>WRITE TO CARD</button>");
  client.println("</div>");
  client.println("</div>");
  
  client.println("<div class='panel'>");
  client.println("<h2>RFID INTERCEPTS</h2>");
  client.println("<div class='history' id='history'>Awaiting intercepts...</div>");
  client.println("</div>");
  
  client.println("<script>");
  client.println("let continuousMode = false;");
  client.println("let refreshInterval;");
  
  client.println("function updateStatus(msg) {");
  client.println("  document.getElementById('status').innerHTML = msg;");
  client.println("}");
  
  client.println("function scan() {");
  client.println("  updateStatus('SCANNING FOR TARGETS...');");
  client.println("  fetch('/api/scan')");
  client.println("    .then(response => response.json())");
  client.println("    .then(data => {");
  client.println("      if (data.success) {");
  client.println("        updateStatus('TARGET ACQUIRED: ' + data.uid);");
  client.println("        refreshHistory();");
  client.println("      } else {");
  client.println("        updateStatus('NO TARGET IN RANGE');");
  client.println("      }");
  client.println("    })");
  client.println("    .catch(err => {");
  client.println("      updateStatus('SCAN ERROR: ' + err.message);");
  client.println("    });");
  client.println("}");
  
  client.println("function toggle() {");
  client.println("  fetch('/api/toggle')");
  client.println("    .then(response => response.json())");
  client.println("    .then(data => {");
  client.println("      continuousMode = data.continuous;");
  client.println("      document.getElementById('toggleBtn').innerHTML = continuousMode ? 'STOP CONTINUOUS' : 'START CONTINUOUS';");
  client.println("      updateStatus(continuousMode ? 'SURVEILLANCE MODE ACTIVE' : 'SURVEILLANCE MODE PAUSED');");
  client.println("      if (continuousMode) {");
  client.println("        startAutoRefresh();");
  client.println("      } else {");
  client.println("        stopAutoRefresh();");
  client.println("      }");
  client.println("    })");
  client.println("    .catch(err => {");
  client.println("      updateStatus('TOGGLE ERROR: ' + err.message);");
  client.println("    });");
  client.println("}");
  
  client.println("function writeCard() {");
  client.println("  let hexData = document.getElementById('hexInput').value.trim();");
  client.println("  if (!hexData) {");
  client.println("    updateStatus('NO HEX DATA PROVIDED');");
  client.println("    return;");
  client.println("  }");
  client.println("  if (hexData.length % 2 !== 0 || hexData.length > 32) {");
  client.println("    updateStatus('INVALID HEX DATA FORMAT');");
  client.println("    return;");
  client.println("  }");
  client.println("  updateStatus('WRITING TO TARGET...');");
  client.println("  fetch('/api/write', {");
  client.println("    method: 'POST',");
  client.println("    headers: { 'Content-Type': 'application/json' },");
  client.println("    body: JSON.stringify({ data: hexData })");
  client.println("  })");
  client.println("    .then(response => response.json())");
  client.println("    .then(data => {");
  client.println("      if (data.success) {");
  client.println("        updateStatus('WRITE SUCCESSFUL');");
  client.println("        document.getElementById('hexInput').value = '';");
  client.println("      } else {");
  client.println("        updateStatus('WRITE FAILED: ' + (data.error || 'UNKNOWN ERROR'));");
  client.println("      }");
  client.println("    })");
  client.println("    .catch(err => {");
  client.println("      updateStatus('WRITE ERROR: ' + err.message);");
  client.println("    });");
  client.println("}");
  
  client.println("function refreshHistory() {");
  client.println("  fetch('/api/history')");
  client.println("    .then(response => response.json())");
  client.println("    .then(data => {");
  client.println("      let historyHtml = '';");
  client.println("      if (data.cards && data.cards.length > 0) {");
  client.println("        data.cards.forEach(card => {");
  client.println("          let timestamp = new Date(parseInt(card.timestamp)).toLocaleTimeString();");
  client.println("          historyHtml += '<div class=\"card\">';");
  client.println("          historyHtml += '<div class=\"uid\">UID: ' + card.uid + '</div>';");
  client.println("          historyHtml += '<div class=\"card-info\">' + card.type + ' | ' + card.access + ' | ' + timestamp + '</div>';");
  client.println("          if (card.blockCount > 0) {");
  client.println("            historyHtml += '<div class=\"card-info\">Blocks Read: ' + card.blockCount + '</div>';");
  client.println("          }");
  client.println("          historyHtml += '</div>';");
  client.println("        });");
  client.println("      } else {");
  client.println("        historyHtml = 'No intercepts recorded';");
  client.println("      }");
  client.println("      document.getElementById('history').innerHTML = historyHtml;");
  client.println("    })");
  client.println("    .catch(err => {");
  client.println("      document.getElementById('history').innerHTML = 'History refresh error: ' + err.message;");
  client.println("    });");
  client.println("}");
  
  client.println("function clearHistory() {");
  client.println("  document.getElementById('history').innerHTML = 'History cleared';");
  client.println("  updateStatus('HISTORY CLEARED');");
  client.println("}");
  
  client.println("function refreshStatus() {");
  client.println("  fetch('/api/status')");
  client.println("    .then(response => response.json())");
  client.println("    .then(data => {");
  client.println("      updateStatus('STATUS: ' + data.status + ' | BUFFER: ' + data.buffer_used + '/25');");
  client.println("      continuousMode = data.continuous;");
  client.println("      document.getElementById('toggleBtn').innerHTML = continuousMode ? 'STOP CONTINUOUS' : 'START CONTINUOUS';");
  client.println("    })");
  client.println("    .catch(err => {");
  client.println("      updateStatus('STATUS ERROR: ' + err.message);");
  client.println("    });");
  client.println("}");
  
  client.println("function startAutoRefresh() {");
  client.println("  if (refreshInterval) clearInterval(refreshInterval);");
  client.println("  refreshInterval = setInterval(() => {");
  client.println("    if (continuousMode) {");
  client.println("      refreshHistory();");  
  client.println("    }");
  client.println("  }, 2000);");
  client.println("}");
  
  client.println("function stopAutoRefresh() {");
  client.println("  if (refreshInterval) {");
  client.println("    clearInterval(refreshInterval);");
  client.println("    refreshInterval = null;");
  client.println("  }");
  client.println("}");
  
  client.println("// Initialize on page load");
  client.println("document.addEventListener('DOMContentLoaded', function() {");
  client.println("  refreshHistory();");
  client.println("  refreshStatus();");
  client.println("});");
  
  client.println("</script>");
  client.println("</body></html>");
}

void handleManualScan(WiFiClient &client) {
  bool cardFound = performEnhancedScan();
  
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  
  if (cardFound && bufferCount > 0) {
    int lastIndex = (bufferHead - 1 + 25) % 25;
    client.print("{\"success\":true,\"uid\":\"");
    client.print(cardBuffer[lastIndex].uid);
    client.print("\",\"type\":\"");
    client.print(cardBuffer[lastIndex].cardType);
    client.print("\",\"access\":\"");
    client.print(cardBuffer[lastIndex].accessLevel);
    client.print("\",\"blocks\":");
    client.print(cardBuffer[lastIndex].blocksRead);
    client.println("}");
  } else {
    client.println("{\"success\":false,\"error\":\"No target in range\"}");
  }
}

void sendEnhancedCardHistory(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  
  client.print("{\"cards\":[");
  
  if (bufferCount > 0) {
    int startIndex = (bufferHead - bufferCount + 25) % 25;
    for (int i = bufferCount - 1; i >= 0; i--) { // Newest first
      if (i < bufferCount - 1) client.print(",");
      int index = (startIndex + i) % 25;
      
      client.print("{\"uid\":\"");
      client.print(cardBuffer[index].uid);
      client.print("\",\"type\":\"");
      client.print(cardBuffer[index].cardType);
      client.print("\",\"access\":\"");
      client.print(cardBuffer[index].accessLevel);
      client.print("\",\"timestamp\":\"");
      client.print(cardBuffer[index].timestamp);
      client.print("\",\"blockCount\":");
      client.print(cardBuffer[index].blocksRead);
      client.print(",\"blocks\":[");
      
      for (int j = 0; j < cardBuffer[index].blocksRead; j++) {
        if (j > 0) client.print(",");
        client.print("\"");
        client.print(cardBuffer[index].blockData[j]);
        client.print("\"");
      }
      client.print("]}");
    }
  }
  
  client.println("]}");
}

void handleEnhancedWriteRequest(WiFiClient &client, String postData) {
  int dataStart = postData.indexOf("\"data\":\"") + 8;
  int dataEnd = postData.indexOf("\"", dataStart);
  String hexData = "";
  
  if (dataStart > 7 && dataEnd > dataStart) {
    hexData = postData.substring(dataStart, dataEnd);
  }
  
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  
  if (hexData.length() > 0 && writeToCard(hexData)) {
    client.println("{\"success\":true,\"message\":\"Data injection successful\"}");
    Serial.print("[WRITE] Payload injected: ");
    Serial.println(hexData);
  } else {
    client.println("{\"success\":false,\"error\":\"Target secured against injection\"}");
  }
}

void toggleContinuousScanning(WiFiClient &client) {
  continuousScanning = !continuousScanning;
  
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  
  client.print("{\"continuous\":");
  client.print(continuousScanning ? "true" : "false");
  client.println("}");
  
  Serial.print("[SURVEILLANCE] Continuous mode: ");
  Serial.println(continuousScanning ? "ACTIVE" : "PAUSED");
}

void toggleDeepScanMode(WiFiClient &client) {
  deepScanMode = !deepScanMode;
  
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  
  client.print("{\"deepmode\":");
  client.print(deepScanMode ? "true" : "false");
  client.println("}");
  
  Serial.print("[DEEPSCAN] Enhanced extraction: ");
  Serial.println(deepScanMode ? "ENABLED" : "DISABLED");
}

void sendSystemStatus(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  
  client.print("{\"status\":\"");
  client.print(systemStatus);
  client.print("\",\"buffer_used\":");
  client.print(bufferCount);
  client.print(",\"buffer_max\":25");
  client.print(",\"continuous\":");
  client.print(continuousScanning ? "true" : "false");
  client.print(",\"deepmode\":");
  client.print(deepScanMode ? "true" : "false");
  client.println("}");
}

void send404(WiFiClient &client) {
  client.println("HTTP/1.1 404 Not Found");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.println("404 - Unauthorized Access Attempt Logged");
}
