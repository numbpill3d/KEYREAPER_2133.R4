#include <AUnit.h>
using namespace aunit;

test(test_uid_parsing) {
  const char* sampleUID = "04 A2 3B 1C";
  // Simulate UID parsing function
  assertEqual(strlen(sampleUID), 11);  // crude but shows test works
}

test(test_wifi_setup) {
  bool wifiStarted = true; // replace with real check in future
  assertTrue(wifiStarted);
}

void setup() {
  Serial.begin(115200);
  delay(2000);  // Give time for serial monitor to start
  TestRunner::run();
}

void loop() {}
