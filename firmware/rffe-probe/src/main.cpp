// W2R Open — ESP32 RFFE probe firmware (clean-room).
//
// A from-scratch, MIT-licensed replacement for the proprietary rffe-esp32-1.0.1.bin
// that shipped with W2R Solutions. It speaks a documented line protocol over USB serial
// and bit-bangs a MIPI RFFE master to enumerate devices on a phone's RF front end.
//
// Host protocol (115200 8N1, one command per '\n'-terminated line):
//
//   Host -> probe        Probe -> host
//   ---------------------------------------------------------------
//   way2_rffe_           SCAN BEGIN
//                        DEV <usid> <reg0>        (one per device)
//                        SCAN END <count>
//   PING                 PONG
//   VER                  W2R-RFFE <version>
//   STOP                 (aborts a scan; SCAN END is still emitted)
//   HELP                 OK COMMANDS way2_rffe_ PING VER STOP HELP
//   <anything else>      ERR UNKNOWN <token>
//
// "way2_rffe_" is kept identical to the vendor command so the same host code drives
// either firmware.
//
// RFFE framing follows the MIPI RFFE specification (SSC, 4-bit USID, 3-bit command,
// odd parity, 5-bit address, odd parity, bus turnaround, 8-bit data, parity, ACK).
// Timing and pinout are compile-time configurable; see platformio.ini.

#include <Arduino.h>

#ifndef W2R_RFFE_VERSION
#define W2R_RFFE_VERSION "1.0.0"
#endif
#ifndef RFFE_SCLK_PIN
#define RFFE_SCLK_PIN 19
#endif
#ifndef RFFE_SDATA_PIN
#define RFFE_SDATA_PIN 18
#endif
#ifndef RFFE_VIO_PIN
#define RFFE_VIO_PIN -1
#endif

namespace {

// Half-bit period in microseconds. ~2 us gives roughly 250 kHz, a safe default for
// jumper wires; shorten once the wiring and level shifting are known good.
constexpr unsigned kHalfPeriodUs = 2;

// MIPI RFFE command codes (3-bit).
constexpr uint8_t kCmdRegRead = 0b010;

bool g_stop = false;

// --- Low-level bit-banging --------------------------------------------------------
// SDATA is open-drain: either driven low or released (pulled high externally).
inline void sclkHigh() { digitalWrite(RFFE_SCLK_PIN, HIGH); }
inline void sclkLow() { digitalWrite(RFFE_SCLK_PIN, LOW); }
inline void sdataLow()
{
	pinMode(RFFE_SDATA_PIN, OUTPUT);
	digitalWrite(RFFE_SDATA_PIN, LOW);
}
inline void sdataRelease() { pinMode(RFFE_SDATA_PIN, INPUT_PULLUP); }
inline bool sdataRead() { return digitalRead(RFFE_SDATA_PIN) == HIGH; }

void rffeIdle()
{
	sdataRelease();
	sclkHigh();
	delayMicroseconds(kHalfPeriodUs);
}

// Sequence Start Condition: SDATA falls while SCLK is high, then SCLK falls.
void rffeSsc()
{
	sdataRelease();
	sclkHigh();
	delayMicroseconds(kHalfPeriodUs);
	sdataLow();
	delayMicroseconds(kHalfPeriodUs);
	sclkLow();
	delayMicroseconds(kHalfPeriodUs);
}

void rffeWriteBit(bool one)
{
	if (one)
		sdataRelease();
	else
		sdataLow();
	delayMicroseconds(kHalfPeriodUs);
	sclkHigh();
	delayMicroseconds(kHalfPeriodUs);
	sclkLow();
	delayMicroseconds(kHalfPeriodUs);
}

bool rffeReadBit()
{
	sdataRelease();
	delayMicroseconds(kHalfPeriodUs);
	sclkHigh();
	delayMicroseconds(kHalfPeriodUs);
	const bool b = sdataRead();
	sclkLow();
	delayMicroseconds(kHalfPeriodUs);
	return b;
}

void rffeWriteBits(uint16_t value, uint8_t count)
{
	for (int i = count - 1; i >= 0; --i) rffeWriteBit((value >> i) & 1);
}

// Odd parity over the low `count` bits: returns the bit that makes the total number of
// ones odd.
bool oddParity(uint16_t value, uint8_t count)
{
	bool p = false;
	for (uint8_t i = 0; i < count; ++i) p ^= (value >> i) & 1;
	return !p;
}

// Reads an 8-bit register from the device at `usid`. Returns true when the device
// acknowledges (ACK bit low), i.e. a device is present. `out` receives the data byte.
bool rffeReadRegister(uint8_t usid, uint8_t address, uint8_t *out)
{
	rffeIdle();
	rffeSsc();

	rffeWriteBits(usid & 0x0F, 4);
	rffeWriteBits(kCmdRegRead, 3);
	rffeWriteBit(oddParity((uint16_t(usid & 0x0F) << 3) | kCmdRegRead, 7));
	rffeWriteBits(address & 0x1F, 5);
	rffeWriteBit(oddParity(address & 0x1F, 5));

	// Bus turnaround: master releases SDATA for one clock so the device can drive it.
	sdataRelease();
	delayMicroseconds(kHalfPeriodUs);
	sclkHigh();
	delayMicroseconds(kHalfPeriodUs);
	sclkLow();
	delayMicroseconds(kHalfPeriodUs);

	uint8_t data = 0;
	for (int i = 0; i < 8; ++i) data = uint8_t((data << 1) | (rffeReadBit() ? 1 : 0));
	(void)rffeReadBit();      // data parity
	const bool nack = rffeReadBit(); // 0 = ACK

	rffeIdle();
	if (out) *out = data;
	return !nack;
}

void scanBus()
{
	g_stop = false;
	Serial.println(F("SCAN BEGIN"));
	uint8_t count = 0;
	for (uint8_t usid = 0; usid < 16; ++usid) {
		if (g_stop) break;
		uint8_t reg0 = 0;
		// Register 0 is read-only and identifies the device (USID + manufacturer id).
		if (rffeReadRegister(usid, 0x00, &reg0)) {
			Serial.printf("DEV %u %02X\n", unsigned(usid), unsigned(reg0));
			++count;
		}
	}
	Serial.printf("SCAN END %u\n", unsigned(count));
}

void handleCommand(const String &raw)
{
	String cmd = raw;
	cmd.trim();
	if (cmd.isEmpty()) return;

	if (cmd == F("way2_rffe_")) {
		scanBus();
	} else if (cmd == F("PING")) {
		Serial.println(F("PONG"));
	} else if (cmd == F("VER")) {
		Serial.printf("W2R-RFFE %s\n", W2R_RFFE_VERSION);
	} else if (cmd == F("STOP")) {
		g_stop = true;
		Serial.println(F("OK STOP"));
	} else if (cmd == F("HELP")) {
		Serial.println(F("OK COMMANDS way2_rffe_ PING VER STOP HELP"));
	} else {
		Serial.printf("ERR UNKNOWN %s\n", cmd.c_str());
	}
}

} // namespace

void setup()
{
	Serial.begin(115200);
	pinMode(RFFE_SCLK_PIN, OUTPUT);
	sclkHigh();
	pinMode(RFFE_SDATA_PIN, INPUT_PULLUP);
#if RFFE_VIO_PIN >= 0
	pinMode(RFFE_VIO_PIN, OUTPUT);
	digitalWrite(RFFE_VIO_PIN, HIGH);
#endif
	delay(50);
	Serial.printf("W2R-RFFE %s\n", W2R_RFFE_VERSION);
}

void loop()
{
	static String line;
	while (Serial.available()) {
		const char c = char(Serial.read());
		if (c == '\n' || c == '\r') {
			handleCommand(line);
			line = "";
		} else if (line.length() < 64) {
			line += c;
		}
	}
}
