/*
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║                                                                  ║
 * ║                           YUMO  BUILDS                           ║
 * ║                       —  CAT CAR REMOTE  —                       ║
 * ║                                                                  ║
 * ║   ESP32-C3 Super Mini  |  v1.0                                   ║
 * ║   Joystick Remote · ESP-NOW · Auto-Pairing                       ║
 * ║                                                                  ║
 * ╚══════════════════════════════════════════════════════════════════╝
 *
 * Yumo Cat Car Remote - the handheld joystick controller for the Cat Car.
 *
 * What it does:
 *   Reads an analog joystick and sends the stick position to the Cat Car
 *   wirelessly over ESP-NOW (no router and no Wi-Fi network needed). The car
 *   turns the stick into proportional driving: push a little = slow, push all
 *   the way = fast, and a diagonal push drives in a curve. The remote finds
 *   and pairs with the car by itself - just power both on.
 *
 * Hardware (REMOTE side):
 *   - ESP32-C3 Super Mini: the remote's own board. The Cat Car has a
 *     separate ESP32-C3 Super Mini that runs Yumo_CatCar_Remote.ino.
 *   - Analog joystick module (5 pins: GND, +5V/VCC, VRx, VRy, SW)
 *   - 1x 3.7V small lithium battery (the car uses 2x 18650)
 *
 * Wiring:
 *   Joystick VRx          -> GPIO 3
 *   Joystick VRy          -> GPIO 1
 *   Joystick SW (button)  -> GPIO 0   (internal pull-up, pressed = LOW)
 *   Joystick +5V/VCC pin  -> 3.3V     (NOT 5V: ESP32 pins are 3.3V only)
 *   Joystick GND          -> GND
 *   Status LED            -> onboard, GPIO 8 (active LOW)
 *
 *   On this module's mounting the axes are rotated 90 degrees, so in software
 *   LEFT/RIGHT is read from GPIO 1 and UP/DOWN from GPIO 3 (PIN_JOY_X and
 *   PIN_JOY_Y below). If a direction is backwards on your module, flip
 *   INVERT_X / INVERT_Y.
 *
 * How it works:
 *   1. SEARCHING - broadcasts PACKET_PAIR_REQUEST every 250 ms on ESP-NOW
 *      channel 1. No joystick data is sent while searching.
 *   2. The Cat Car answers with PACKET_PAIR_ACK. The remote remembers the
 *      car's MAC address, adds it as a peer, and is CONNECTED.
 *   3. CONNECTED - sends PACKET_JOYSTICK every 20 ms (50 times per second),
 *      even when the stick is centred. The car answers about 10 times per
 *      second with PACKET_LINK_ACK (a heartbeat).
 *   4. If no heartbeat is heard for 1 second, the link is dropped and the
 *      remote goes back to SEARCHING by itself (car switched off, out of
 *      range, car rebooted...). The car independently stops its wheels after
 *      300 ms without a packet, so it never keeps driving on its own.
 *
 * What is sent:
 *   Each joystick packet carries the stick X and Y as raw 12-bit ADC values
 *   (0..4095, centre is about 2048; each reading is the average of 4 samples)
 *   and the button state (1 = pressed). The car applies the deadzone and the
 *   speed curve. The button is transmitted but has no function yet.
 *
 * Status LED (onboard, GPIO 8, active LOW):
 *   even fast blink                        = searching for the car
 *                                            (radio OK, no reply yet)
 *   mostly dark + short blip every second  = radio failed to send
 *   solid on                               = connected
 *
 * Tuning (constants below):
 *   INVERT_X / INVERT_Y        flip a stick direction
 *   YUMO_TX_POWER              radio power (8.5 dBm: Super Mini antennas
 *                              dislike full power)
 *   PAIR_REQUEST_INTERVAL_MS   how often it searches (250 ms)
 *   JOYSTICK_SEND_INTERVAL_MS  how often it sends the stick (20 ms)
 *   LINK_TIMEOUT_MS            time without a heartbeat before it drops the
 *                              link (1000 ms)
 *
 * Board settings (Arduino IDE) - for the REMOTE's ESP32:
 *   Board "ESP32C3 Dev Module". Flash size, partition scheme and tested
 *   versions for each board: see README.md. (The car's board needs the
 *   "Huge APP" partition; this small sketch has no animation data.)
 *   No extra libraries needed (WiFi and ESP-NOW come with the ESP32 core).
 *
 * IMPORTANT: the packet definition below MUST match Yumo_CatCar_Remote.ino.
 *
 * Project, wiring, build video and STL files: see README.md in the
 * repository root.
 *
 * ──────────────────────────────────────────────────────────────────────
 *  LICENSE
 * ──────────────────────────────────────────────────────────────────────
 *
 * Licensed under the PolyForm Noncommercial License 1.0.0.
 * See LICENSE.md in the repository root, or
 * https://polyformproject.org/licenses/noncommercial/1.0.0
 *
 * Required Notice: Copyright yumobuilds (https://github.com/yumobuilds)
 *
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ---------------- Pins ----------------
// Wires stay the same: VRx -> GPIO 3, VRy -> GPIO 1, SW -> GPIO 0.
// On this module's mounting the axes are rotated 90 degrees, so in software:
const int PIN_JOY_X = 1;   // stick LEFT/RIGHT arrives on the module's VRy pin (GPIO 1)
const int PIN_JOY_Y = 3;   // stick UP/DOWN arrives on the module's VRx pin (GPIO 3)
const int PIN_JOY_B = 0;   // SW (button)
const int PIN_STATUS_LED = 8;   // ESP32-C3 Super Mini onboard LED (active LOW)

// If a stick direction is backwards on your module, flip these.
const bool INVERT_X = true;    // right must read high -> turn right
const bool INVERT_Y = false;

// ---------------- Protocol (identical on the car) ----------------
#define YUMO_ESPNOW_CHANNEL 1
#define YUMO_TX_POWER WIFI_POWER_8_5dBm   // Super Mini antennas dislike full power
const uint8_t YUMO_ID[4] = { 'Y', 'U', 'M', 'O' };

enum YumoPacketType : uint8_t {
  PACKET_PAIR_REQUEST = 1,
  PACKET_PAIR_ACK     = 2,
  PACKET_JOYSTICK     = 3,
  PACKET_LINK_ACK     = 4
};

struct __attribute__((packed)) YumoPacket {
  uint8_t id[4];
  uint8_t type;
  uint8_t button;
  int16_t x;
  int16_t y;
};
static_assert(sizeof(YumoPacket) == 10, "YumoPacket size mismatch between car and remote");

// ---------------- Timing ----------------
const unsigned long PAIR_REQUEST_INTERVAL_MS = 250;
const unsigned long JOYSTICK_SEND_INTERVAL_MS = 20;
const unsigned long LINK_TIMEOUT_MS = 1000;

// ---------------- State ----------------
const uint8_t BROADCAST_MAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

enum LinkState { LINK_SEARCHING, LINK_CONNECTED };
LinkState linkState = LINK_SEARCHING;

uint8_t carMac[6] = {0, 0, 0, 0, 0, 0};
volatile unsigned long lastAckTime = 0;

volatile bool pendingPairAck = false;   // set in callback, handled in loop()
uint8_t pendingCarMac[6] = {0, 0, 0, 0, 0, 0};

unsigned long lastPairRequestTime = 0;
unsigned long lastJoystickSendTime = 0;

portMUX_TYPE yumoMux = portMUX_INITIALIZER_UNLOCKED;

// ---------------- Helpers ----------------
void printMac(const uint8_t *mac) {
  for (int i = 0; i < 6; i++) {
    if (i) Serial.print(":");
    if (mac[i] < 16) Serial.print("0");
    Serial.print(mac[i], HEX);
  }
}

bool addPeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = YUMO_ESPNOW_CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

volatile bool lastSendFailed = false;   // shown on the LED while searching

bool sendPacket(const uint8_t *mac, uint8_t type, int16_t x, int16_t y, uint8_t button) {
  YumoPacket p;
  memcpy(p.id, YUMO_ID, sizeof(YUMO_ID));
  p.type = type;
  p.button = button;
  p.x = x;
  p.y = y;
  bool ok = esp_now_send(mac, (const uint8_t *)&p, sizeof(p)) == ESP_OK;
  lastSendFailed = !ok;
  return ok;
}

int readAxis(int pin) {
  long sum = 0;
  for (int i = 0; i < 4; i++) sum += analogRead(pin);
  return (int)(sum / 4);   // 0..4095
}

// ---------------- Receive callback (Wi-Fi task: keep it short) ----------------
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != (int)sizeof(YumoPacket)) return;

  YumoPacket p;
  memcpy(&p, data, sizeof(p));
  if (memcmp(p.id, YUMO_ID, sizeof(YUMO_ID)) != 0) return;

  if (p.type == PACKET_PAIR_ACK) {
    portENTER_CRITICAL(&yumoMux);
    memcpy(pendingCarMac, info->src_addr, 6);
    pendingPairAck = true;
    portEXIT_CRITICAL(&yumoMux);
  } else if (p.type == PACKET_LINK_ACK) {
    bool fromCar;
    portENTER_CRITICAL(&yumoMux);
    fromCar = (linkState == LINK_CONNECTED) && (memcmp(info->src_addr, carMac, 6) == 0);
    portEXIT_CRITICAL(&yumoMux);
    if (fromCar) lastAckTime = millis();
  }
}

// ---------------- Link management ----------------
void handlePairAck() {
  if (!pendingPairAck) return;

  uint8_t mac[6];
  portENTER_CRITICAL(&yumoMux);
  memcpy(mac, pendingCarMac, 6);
  pendingPairAck = false;
  portEXIT_CRITICAL(&yumoMux);

  if (linkState == LINK_CONNECTED) {
    // Already linked: only a repeat ack from our own car counts.
    if (memcmp(mac, carMac, 6) == 0) lastAckTime = millis();
    return;
  }

  if (!addPeer(mac)) {
    Serial.println("Pairing: add_peer failed");
    return;
  }

  portENTER_CRITICAL(&yumoMux);
  memcpy(carMac, mac, 6);
  lastAckTime = millis();
  linkState = LINK_CONNECTED;
  portEXIT_CRITICAL(&yumoMux);

  lastJoystickSendTime = 0;
  Serial.print("Connected to Cat Car ");
  printMac(carMac);
  Serial.println();
}

void dropLink() {
  Serial.println("Link lost - searching for Cat Car...");
  portENTER_CRITICAL(&yumoMux);
  linkState = LINK_SEARCHING;
  portEXIT_CRITICAL(&yumoMux);
  esp_now_del_peer(carMac);
  lastPairRequestTime = 0;
}

void updateStatusLed(unsigned long now) {
  bool on;
  if (linkState == LINK_CONNECTED) on = true;
  else if (lastSendFailed) on = ((now % 1000) < 60);   // mostly dark, short blip = radio send error
  else on = ((now / 150) % 2) == 0;                    // even fast blink = searching
  digitalWrite(PIN_STATUS_LED, on ? LOW : HIGH);   // active LOW
}

// ---------------- Arduino ----------------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_JOY_B, INPUT_PULLUP);
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, HIGH);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);   // full 0..3.3 V range

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(YUMO_TX_POWER);

  // Same Wi-Fi channel as the car.
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(YUMO_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed - restarting");
    delay(1000);
    ESP.restart();
  }
  esp_now_register_recv_cb(onDataRecv);

  if (!addPeer(BROADCAST_MAC)) {
    Serial.println("Could not add broadcast peer");
  }

  Serial.print("Remote MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Searching for Cat Car...");
}

void loop() {
  unsigned long now = millis();

  handlePairAck();

  if (linkState == LINK_CONNECTED) {
    if (now - lastAckTime > LINK_TIMEOUT_MS) {
      dropLink();   // stop transmitting; the car has already stopped on its own
    } else if (now - lastJoystickSendTime >= JOYSTICK_SEND_INTERVAL_MS) {
      lastJoystickSendTime = now;

      int x = readAxis(PIN_JOY_X);
      int y = readAxis(PIN_JOY_Y);
      if (INVERT_X) x = 4095 - x;
      if (INVERT_Y) y = 4095 - y;
      uint8_t button = (digitalRead(PIN_JOY_B) == LOW) ? 1 : 0;

      sendPacket(carMac, PACKET_JOYSTICK, (int16_t)x, (int16_t)y, button);
    }
  } else {
    if (now - lastPairRequestTime >= PAIR_REQUEST_INTERVAL_MS) {
      lastPairRequestTime = now;
      sendPacket(BROADCAST_MAC, PACKET_PAIR_REQUEST, 0, 0, 0);
    }
  }

  updateStatusLed(now);
  delay(1);
}
