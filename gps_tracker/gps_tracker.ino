 #include <WiFi.h>
  #include <HTTPClient.h>
  #include <ArduinoJson.h>
  #include "arduino_secrets.h"

  // WiFi credentials
  const char* ssid = SECRET_SSID;
  const char* password = SECRET_PASS;

  // CORRECT PRODUCTION API endpoint - Heroku backend
  const char* serverURL = "https://sri-express-38573a31e4bc.herokuapp.com/api/tracking/update";

  const String deviceId = "68c0702b7d1cb7545d768ac2";  // bus-002 device
  const String vehicleId = "bus-002";
  const String routeId = "68c2cba549cec0f802e5e33a";   // Kottawa to Mount Lavinia route

  // GPS Serial
  HardwareSerial gpsSerial(2);

  // Timing variables
  unsigned long lastUpdate = 0;
  const unsigned long updateInterval = 10000; // Send data every 10 seconds

  // GPS data structure
  struct GPSData {
    float latitude = 0.0;
    float longitude = 0.0;
    float speed = 0.0;
    float heading = 0.0;
    bool isValid = false;
    String timestamp = "";
  };

  GPSData currentGPS;

  void setup() {
    Serial.begin(115200);
    delay(2000);

    // Initialize GPS
    gpsSerial.begin(9600, SERIAL_8N1, 2, 4); // RX=GPIO2, TX=GPIO4

    // Connect to WiFi
    connectToWiFi();

    Serial.println("=== ESP32 Bus Tracker Started ===");
    Serial.println("Device ID: " + deviceId);
    Serial.println("Vehicle ID: " + vehicleId);
    Serial.println("Route ID: " + routeId);
    Serial.println("API Endpoint: " + String(serverURL));
    Serial.println("Waiting for GPS data...");
  }

  void loop() {
    // Show all raw GPS data
    if (gpsSerial.available()) {
      String rawGPS = gpsSerial.readStringUntil('\n');
      rawGPS.trim();
      if (rawGPS.length() > 10) {
        Serial.println("RAW GPS: " + rawGPS);
      }
    }

    // Read and parse GPS data
    readGPSData();

    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, reconnecting...");
      connectToWiFi();
    }

    // Send data to server every 10 seconds
    if (millis() - lastUpdate >= updateInterval) {
      if (currentGPS.isValid && WiFi.status() == WL_CONNECTED) {
        sendLocationUpdate();
      } else {
        Serial.println("Skipping update - Invalid GPS or no WiFi");
        Serial.println("GPS Valid: " + String(currentGPS.isValid ? "YES" : "NO"));
        Serial.println("WiFi Connected: " + String(WiFi.status() == WL_CONNECTED ? "YES" : "NO"));

        if (!currentGPS.isValid) {
          Serial.println("Last GPS: Lat=" + String(currentGPS.latitude, 6) +
                        " Lon=" + String(currentGPS.longitude, 6));
        }
      }
      lastUpdate = millis();
    }

    delay(500);
  }

  void connectToWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(1000);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi Connected!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("");
      Serial.println("WiFi Connection Failed!");
    }
  }

  void readGPSData() {
    while (gpsSerial.available()) {
      String gpsLine = gpsSerial.readStringUntil('\n');
      gpsLine.trim();

      if (gpsLine.startsWith("$GNRMC") || gpsLine.startsWith("$GPRMC")) {
        Serial.println("🔍 Found GNRMC sentence: " + gpsLine);
        parseGNRMC(gpsLine);
      }
    }
  }

  void parseGNRMC(String nmea) {
    int commaIndex[15];
    int commaCount = 0;

    for (int i = 0; i < nmea.length() && commaCount < 15; i++) {
      if (nmea.charAt(i) == ',') {
        commaIndex[commaCount] = i;
        commaCount++;
      }
    }

    if (commaCount < 9) {
      Serial.println("❌ GNRMC: Not enough data fields (" + String(commaCount) + ")");
      return;
    }

    String status = nmea.substring(commaIndex[1] + 1, commaIndex[2]);
    Serial.println("📡 GPS Status: '" + status + "' " + (status == "A" ? "(VALID FIX)" : "(NO FIX)"));

    if (status != "A") {
      currentGPS.isValid = false;
      Serial.println("⏳ Waiting for GPS fix...");
      return;
    }

    String latStr = nmea.substring(commaIndex[2] + 1, commaIndex[3]);
    String latDir = nmea.substring(commaIndex[3] + 1, commaIndex[4]);
    String lonStr = nmea.substring(commaIndex[4] + 1, commaIndex[5]);
    String lonDir = nmea.substring(commaIndex[5] + 1, commaIndex[6]);
    String speedStr = nmea.substring(commaIndex[6] + 1, commaIndex[7]);
    String headingStr = nmea.substring(commaIndex[7] + 1, commaIndex[8]);

    Serial.println("📍 Raw: Lat=" + latStr + latDir + " Lon=" + lonStr + lonDir +
                  " Speed=" + speedStr + " Heading=" + headingStr);

    if (latStr.length() > 0 && lonStr.length() > 0) {
      float lat = convertDMtoDDFloat(latStr);
      float lon = convertDMtoDDFloat(lonStr);

      if (latDir == "S") lat = -lat;
      if (lonDir == "W") lon = -lon;

      currentGPS.latitude = lat;
      currentGPS.longitude = lon;
      currentGPS.speed = speedStr.toFloat() * 1.852;
      currentGPS.heading = headingStr.toFloat();
      currentGPS.isValid = true;
      currentGPS.timestamp = String(millis());

      Serial.println("✅ GPS VALID: " + String(lat, 6) + ", " + String(lon, 6) +
                     " Speed: " + String(currentGPS.speed, 1) + " km/h" +
                     " Heading: " + String(currentGPS.heading, 1) + "°");
    } else {
      Serial.println("❌ Empty coordinate fields");
    }
  }

  float convertDMtoDDFloat(String dmString) {
    if (dmString.length() < 4) return 0.0;

    int dotIndex = dmString.indexOf('.');
    if (dotIndex < 0) return 0.0;

    String degreesStr = dmString.substring(0, dotIndex - 2);
    String minutesStr = dmString.substring(dotIndex - 2);

    float degrees = degreesStr.toFloat();
    float minutes = minutesStr.toFloat();

    return degrees + (minutes / 60.0);
  }

  void sendLocationUpdate() {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi not connected - skipping update");
      return;
    }

    HTTPClient http;
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");

    // Add timeout settings for Heroku
    http.setTimeout(30000); // 30 second timeout

    DynamicJsonDocument doc(2048);

    doc["deviceId"] = deviceId;
    doc["vehicleId"] = vehicleId;

    JsonObject location = doc.createNestedObject("location");
    location["latitude"] = currentGPS.latitude;
    location["longitude"] = currentGPS.longitude;
    location["accuracy"] = 5.0;
    location["heading"] = currentGPS.heading;
    location["speed"] = currentGPS.speed;
    location["altitude"] = 50.0;

    JsonObject routeProgress = doc.createNestedObject("routeProgress");
    routeProgress["currentWaypoint"] = 0;
    routeProgress["distanceCovered"] = 0.0;
    routeProgress["estimatedTimeToDestination"] = 30;
    routeProgress["nextStopETA"] = "2025-01-10T15:30:00Z";
    routeProgress["progressPercentage"] = 50.0;

    JsonObject passengerLoad = doc.createNestedObject("passengerLoad");
    passengerLoad["currentCapacity"] = 25;
    passengerLoad["maxCapacity"] = 50;
    passengerLoad["boardingCount"] = 0;
    passengerLoad["alightingCount"] = 0;
    passengerLoad["loadPercentage"] = 50.0;

    JsonObject operationalInfo = doc.createNestedObject("operationalInfo");

    JsonObject driverInfo = operationalInfo.createNestedObject("driverInfo");
    driverInfo["driverId"] = "DRIVER_001";
    driverInfo["driverName"] = "Bus Driver";
    driverInfo["contactNumber"] = "+94771234567";

    JsonObject tripInfo = operationalInfo.createNestedObject("tripInfo");
    tripInfo["tripId"] = "TRIP_" + String(millis());
    tripInfo["scheduleId"] = "SCH_001";
    tripInfo["departureTime"] = "08:00";
    tripInfo["estimatedArrival"] = "2025-01-10T15:30:00Z";
    tripInfo["routeId"] = routeId;

    operationalInfo["status"] = "on_route";

    JsonObject delays = operationalInfo.createNestedObject("delays");
    delays["currentDelay"] = 0;
    delays["reason"] = "";

    JsonObject environmentalData = doc.createNestedObject("environmentalData");
    environmentalData["weather"] = "sunny";
    environmentalData["temperature"] = 28;
    environmentalData["trafficCondition"] = "moderate";

    String jsonString;
    serializeJson(doc, jsonString);

    Serial.println("🚀 Sending to HEROKU server...");
    Serial.println("📡 GPS: " + String(currentGPS.latitude, 6) + ", " + String(currentGPS.longitude, 6));
    Serial.println("🌐 URL: " + String(serverURL));

    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("HTTP Response: " + String(httpResponseCode));

      if (httpResponseCode == 200) {
        Serial.println("✅ Location updated successfully on HEROKU!");
        Serial.println("Response: " + response);
      } else {
        Serial.println("❌ Server error: " + String(httpResponseCode));
        Serial.println("Response: " + response);
      }
    } else {
      Serial.println("❌ HTTP Request failed: " + String(httpResponseCode));
      Serial.println("Error: " + http.errorToString(httpResponseCode));
    }

    http.end();
  }