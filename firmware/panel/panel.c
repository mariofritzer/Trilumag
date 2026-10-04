/*
  Trilumag – Panel-Firmware für CH32V003 (Version 3)
  --------------------------------------------------
  Gebaut mit ch32fun (https://github.com/cnlohr/ch32fun).

  Pins (CH32V003F4P6):
    PD5  USART1 TX  -> MAX485 DI
    PD6  USART1 RX  <- MAX485 RO
    PD4  Senderichtung -> MAX485 DE und /RE
    PC3  SNS Kante 1 (über 100 Ohm)
    PC4  SNS Kante 2
    PC0  SNS Kante 3
    PC1  I2C SDA  -> Beschleunigungssensor LIS2DH12 (Touch)
    PC2  I2C SCL  -> LIS2DH12
    PD2  INT1 vom LIS2DH12 (High = Antippen erkannt)
    PC6  LED-Daten (SPI MOSI) -> WS2814, Kette Kante 1, 2, 3
    PD1  SWIO zum Programmieren

  Verhalten: startet dunkel und ohne Adresse, wird vom Hauptpanel gefunden,
  pulsiert dann blau, bis die erste Farbe kommt. Protokoll: docs/protokoll.md

  Updates: Im Boot-Bereich liegt der Trilumag-Bootloader (bootloader/). Auf den Befehl BOOT
  springt die Firmware dorthin, das Hauptpanel schreibt die neue Firmware über den Bus.
*/

#include "ch32fun.h"
#include <stdint.h>
#include <string.h>

#define WS2812DMA_IMPLEMENTATION
#define WSRAW
#define DMALEDS 4
#include "ws2812b_dma_spi_led_driver.h"

#ifndef FW_VERSION
#define FW_VERSION 3   // 2: FRAME3; 3: Touch, Bootloader, Einschalt-Kennung, neue SNS-Pins
#endif
#define NUM_SEGMENTS 3
#define BAUD 250000

// ---------- Protokoll ----------
#define SYNC        0xA5
#define ADDR_ALL    0x00
#define ADDR_BYID   0x7F
#define REPLY_FLAG  0x80

enum {
	CMD_PING = 0x01, CMD_COLOR = 0x02, CMD_EDGES = 0x03, CMD_FRAME = 0x04, CMD_PULSE = 0x05, CMD_ORDER = 0x06, CMD_FRAME3 = 0x07,
	CMD_TOUCH = 0x08,
	CMD_BEACON = 0x10, CMD_DISCOVER = 0x11, CMD_PROBE = 0x12, CMD_ASSIGN = 0x14, CMD_RESET = 0x15,
	CMD_BOOT = 0x20
};
enum { ST_DARK = 0, ST_PULSE = 1, ST_ACTIVE = 2 };
enum { CAP_TOUCH = 1, CAP_BOOT = 2 };

// Gemeinsam mit dem Bootloader: Kennung im Boot-Bereich und Sprungwunsch im RAM
#define BL_SIGNATURE_ADDR 0x1FFFF000u
#define BL_SIGNATURE      0x4C424C54u   // "TLBL"
#define BL_REQUEST_ADDR   0x20000500u
#define BL_REQUEST        0xB007B007u

// ---------- Zustand ----------
static uint32_t chipId;
static uint8_t myAddr = 0;            // 0 = noch nicht eingegliedert
static uint8_t state = ST_DARK;
static uint8_t beaconOn = 1;          // gilt, sobald eine Adresse vergeben ist
static int8_t probeEdge = -1;         // nur ohne Adresse: diese Kante auf Low ziehen
static uint8_t seg[NUM_SEGMENTS][4];  // R, G, B, W je Kante
static uint8_t order[4] = {0, 1, 2, 3};
static volatile uint8_t ledsDirty = 1;
static uint8_t powerOn = 0;           // 1 = frisch mit Strom versorgt (angeklipst), bis zur ersten Adresse
static uint8_t caps = 0;
static uint8_t touchThr = 0;          // 0 = Touch aus, sonst Schwelle in 16 mg
static uint8_t touchSeq = 0;          // zählt Ereignisse, Bits 0-1 = Art (1 = einmal, 2 = doppelt)
static uint32_t tapPendingAt = 0;     // einfaches Antippen: kurz warten, ob ein zweites folgt

// ---------- Zeit ----------
#define TICKS_PER_US (FUNCONF_SYSTEM_CORE_CLOCK / 1000000)
#define TICKS_PER_MS (FUNCONF_SYSTEM_CORE_CLOCK / 1000)
static inline uint32_t ticks(void) { return SysTick->CNT; }

// ---------- CRC-8, Polynom 0x07 ----------
static uint8_t crc8(uint8_t c, uint8_t b) {
	c ^= b;
	for (int i = 0; i < 8; i++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
	return c;
}

// ---------- UART mit Empfangspuffer ----------
static volatile uint8_t rxBuf[256];
static volatile uint8_t rxHead = 0, rxTail = 0;

void USART1_IRQHandler(void) __attribute__((interrupt));
void USART1_IRQHandler(void) {
	if (USART1->STATR & USART_STATR_RXNE) {
		uint8_t b = (uint8_t)USART1->DATAR;
		rxBuf[rxHead++] = b;
	}
	if (USART1->STATR & USART_STATR_ORE) (void)USART1->DATAR;
}

static int rxRead(void) {
	if (rxHead == rxTail) return -1;
	return rxBuf[rxTail++];
}

static void uartInit(void) {
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1 | RCC_APB2Periph_AFIO;
	funPinMode(PD5, GPIO_CFGLR_OUT_10Mhz_AF_PP);
	funPinMode(PD6, GPIO_CFGLR_IN_PUPD);
	funDigitalWrite(PD6, FUN_HIGH);
	funPinMode(PD4, GPIO_CFGLR_OUT_10Mhz_PP);
	funDigitalWrite(PD4, FUN_LOW);
	USART1->BRR = FUNCONF_SYSTEM_CORE_CLOCK / BAUD;
	USART1->CTLR1 = USART_CTLR1_UE | USART_CTLR1_TE | USART_CTLR1_RE | USART_CTLR1_RXNEIE;
	NVIC_EnableIRQ(USART1_IRQn);
}

static void sendFrame(uint8_t addr, uint8_t cmd, const uint8_t* data, uint8_t len) {
	uint8_t c = 0;
	funDigitalWrite(PD4, FUN_HIGH);
	Delay_Us(5);
	const uint8_t head[4] = {SYNC, addr, cmd, len};
	for (int i = 0; i < 4; i++) {
		while (!(USART1->STATR & USART_STATR_TXE));
		USART1->DATAR = head[i];
		if (i) c = crc8(c, head[i]);
	}
	for (int i = 0; i < len; i++) {
		while (!(USART1->STATR & USART_STATR_TXE));
		USART1->DATAR = data[i];
		c = crc8(c, data[i]);
	}
	while (!(USART1->STATR & USART_STATR_TXE));
	USART1->DATAR = c;
	while (!(USART1->STATR & USART_STATR_TC));
	funDigitalWrite(PD4, FUN_LOW);
}

static void reply(uint8_t cmd, const uint8_t* data, uint8_t len) {
	Delay_Us(150);
	sendFrame(REPLY_FLAG | myAddr, cmd, data, len);
}

// ---------- SNS-Leitungen ----------
static const uint8_t snsPin[3] = {PC3, PC4, PC0};

static void snsApply(void) {
	for (int e = 0; e < 3; e++) {
		int low = myAddr ? beaconOn : (probeEdge == e);
		if (low) {
			funPinMode(snsPin[e], GPIO_CFGLR_OUT_10Mhz_PP);
			funDigitalWrite(snsPin[e], FUN_LOW);
		} else {
			funPinMode(snsPin[e], GPIO_CFGLR_IN_PUPD);
			funDigitalWrite(snsPin[e], FUN_HIGH);
		}
	}
}

// Liest, an welchen Kanten die Leitung Low ist. Kanten, die selbst treiben, zählen nicht.
static uint8_t snsMask(void) {
	uint8_t m = 0;
	for (int e = 0; e < 3; e++) {
		int driving = myAddr ? beaconOn : (probeEdge == e);
		if (!driving && !funDigitalRead(snsPin[e])) m |= (uint8_t)(1 << e);
	}
	return m;
}

// ---------- Beschleunigungssensor LIS2DH12 (Touch) ----------
#define ACC_ADDR 0x18          // SA0 auf GND
#ifdef PANEL_SIM
#define accWrite(r, v) simAccWrite(r, v)
#define accRead(r) simAccRead(r)
#define accIntHigh() simAccInt()
#else
#define I2C_EVT_MODE_SELECT  0x00030001u   // BUSY, MSL, SB
#define I2C_EVT_TX_SELECTED  0x00070082u   // BUSY, MSL, ADDR, TXE, TRA
#define I2C_EVT_RX_SELECTED  0x00030002u   // BUSY, MSL, ADDR
#define I2C_EVT_BYTE_SENT    0x00070084u   // TRA, BUSY, MSL, TXE, BTF

static int i2cWait(uint32_t evt) {
	for (uint32_t t = 0; t < 20000; t++) {
		uint32_t s = I2C1->STAR1 | ((uint32_t)I2C1->STAR2 << 16);
		if ((s & evt) == evt) return 1;
	}
	return 0;
}
static int i2cFail(void) { I2C1->CTLR1 |= I2C_CTLR1_STOP; return -1; }

static void i2cInit(void) {
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO;
	RCC->APB1PCENR |= RCC_APB1Periph_I2C1;
	funPinMode(PC1, GPIO_CFGLR_OUT_10Mhz_AF_OD);   // SDA
	funPinMode(PC2, GPIO_CFGLR_OUT_10Mhz_AF_OD);   // SCL
	funPinMode(PD2, GPIO_CFGLR_IN_FLOAT);        // INT1, der Sensor treibt aktiv
	RCC->APB1PRSTR |= RCC_APB1Periph_I2C1;
	RCC->APB1PRSTR &= ~RCC_APB1Periph_I2C1;
	I2C1->CTLR2 = (FUNCONF_SYSTEM_CORE_CLOCK / 1000000) & I2C_CTLR2_FREQ;
	I2C1->CKCFGR = (FUNCONF_SYSTEM_CORE_CLOCK / (2 * 100000)) & I2C_CKCFGR_CCR;   // 100 kHz
	I2C1->CTLR1 = I2C_CTLR1_PE;
}

static int accSelect(uint8_t reg) {
	I2C1->CTLR1 |= I2C_CTLR1_START;
	if (!i2cWait(I2C_EVT_MODE_SELECT)) return 0;
	I2C1->DATAR = ACC_ADDR << 1;
	if (!i2cWait(I2C_EVT_TX_SELECTED)) return 0;
	I2C1->DATAR = reg;
	return i2cWait(I2C_EVT_BYTE_SENT);
}

static int accWrite(uint8_t reg, uint8_t v) {
	if (!accSelect(reg)) return i2cFail();
	I2C1->DATAR = v;
	if (!i2cWait(I2C_EVT_BYTE_SENT)) return i2cFail();
	I2C1->CTLR1 |= I2C_CTLR1_STOP;
	return 0;
}

static int accRead(uint8_t reg) {
	if (!accSelect(reg)) return i2cFail();
	I2C1->CTLR1 &= ~I2C_CTLR1_ACK;                  // nur ein Byte: mit NACK beenden
	I2C1->CTLR1 |= I2C_CTLR1_START;
	if (!i2cWait(I2C_EVT_MODE_SELECT)) return i2cFail();
	I2C1->DATAR = (ACC_ADDR << 1) | 1;
	if (!i2cWait(I2C_EVT_RX_SELECTED)) return i2cFail();
	I2C1->CTLR1 |= I2C_CTLR1_STOP;
	for (uint32_t t = 0; t < 20000; t++) if (I2C1->STAR1 & I2C_STAR1_RXNE) return (uint8_t)I2C1->DATAR;
	return -1;
}
#define accIntHigh() funDigitalRead(PD2)
#endif

static void touchApply(void) {
	if (!(caps & CAP_TOUCH)) return;
	accWrite(0x20, touchThr ? 0x77 : 0x07);   // CTRL_REG1: 400 Hz, X/Y/Z – oder Schlafmodus
	accWrite(0x38, touchThr ? 0x3F : 0x00);   // CLICK_CFG: einfach und doppelt, alle Achsen
	accWrite(0x3A, 0x80 | (touchThr & 0x7F)); // CLICK_THS mit Halten bis zum Auslesen
}

static void touchInit(void) {
#ifndef PANEL_SIM
	i2cInit();
#endif
	if (accRead(0x0F) != 0x33) return;        // WHO_AM_I: kein Sensor bestückt
	caps |= CAP_TOUCH;
	accWrite(0x21, 0x04);                     // CTRL_REG2: Hochpass für Klicks (Schwerkraft raus)
	accWrite(0x22, 0x80);                     // CTRL_REG3: Klick auf INT1
	accWrite(0x23, 0x00);                     // CTRL_REG4: ±2 g
	accWrite(0x3B, 0x10);                     // TIME_LIMIT 40 ms
	accWrite(0x3C, 0x14);                     // TIME_LATENCY 50 ms
	accWrite(0x3D, 0x60);                     // TIME_WINDOW 240 ms
	touchApply();
}

static void touchEvent(uint8_t kind) { touchSeq = (uint8_t)(((touchSeq >> 2) + 1) << 2) | kind; }

static void touchLoop(void) {
	if (!(caps & CAP_TOUCH) || !touchThr) return;
	if (accIntHigh()) {
		int src = accRead(0x39);                 // CLICK_SRC lesen gibt INT1 wieder frei
		if (src > 0 && (src & 0x40)) {
			if (src & 0x20) { tapPendingAt = 0; touchEvent(2); }
			else if ((src & 0x10) && !tapPendingAt) tapPendingAt = ticks() | 1;
		}
	}
	if (tapPendingAt && (uint32_t)(ticks() - tapPendingAt) > 300 * TICKS_PER_MS) { tapPendingAt = 0; touchEvent(1); }
}

// ---------- Bootloader ----------
static int bootloaderPresent(void) {             // Kennung steht in den ersten Wörtern des Boot-Bereichs
#ifdef PANEL_SIM
	return simHasBoot();
#endif
	for (int i = 0; i < 16; i++) if (((volatile uint32_t*)BL_SIGNATURE_ADDR)[i] == BL_SIGNATURE) return 1;
	return 0;
}

static void enterBootloader(void) {
#ifdef PANEL_SIM
	simEnterBoot();
#else
	*(volatile uint32_t*)BL_REQUEST_ADDR = BL_REQUEST;
	FLASH->KEYR = FLASH_KEY1;
	FLASH->KEYR = FLASH_KEY2;
	FLASH->BOOT_MODEKEYR = FLASH_KEY1;
	FLASH->BOOT_MODEKEYR = FLASH_KEY2;
	FLASH->STATR = 1 << 14;                    // nach dem Neustart im Boot-Bereich beginnen
	FLASH->CTLR = CR_LOCK_Set;
	PFIC->SCTLR = 1 << 31;
	while (1);
#endif
}

// ---------- LEDs ----------
static uint8_t pulseLevel(void) {
	// Dreieck zwischen 6 und 32 (etwa 2 % bis 12 %), Periode ca. 1,6 s
	uint32_t ms = ticks() / TICKS_PER_MS;
	uint32_t p = ms % 1600;
	uint32_t t = p < 800 ? p : 1600 - p;
	return (uint8_t)(6 + (t * 26) / 800);
}

uint32_t WS2812BLEDCallback(int ledno) {
	uint8_t ch[4];
	if (state == ST_DARK || ledno >= NUM_SEGMENTS) { ch[0] = ch[1] = ch[2] = ch[3] = 0; }
	else if (state == ST_PULSE) { ch[0] = 0; ch[1] = 0; ch[2] = pulseLevel(); ch[3] = 0; }
	else memcpy(ch, seg[ledno], 4);
	uint8_t b0 = ch[order[0] & 3], b1 = ch[order[1] & 3], b2 = ch[order[2] & 3], b3 = ch[order[3] & 3];
	return (uint32_t)b0 | ((uint32_t)b1 << 8) | ((uint32_t)b2 << 16) | ((uint32_t)b3 << 24);
}

static void setAll(const uint8_t* rgbw) {
	for (int s = 0; s < NUM_SEGMENTS; s++) memcpy(seg[s], rgbw, 4);
	state = ST_ACTIVE;
	ledsDirty = 1;
}

// ---------- Befehle ----------
static uint32_t readId(const uint8_t* d) {
	return (uint32_t)d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24);
}

static void handleFrame(uint8_t addr, uint8_t cmd, const uint8_t* d, uint8_t len) {
	int forMe = (addr == ADDR_ALL) || (myAddr && addr == myAddr) ||
	            (addr == ADDR_BYID && len >= 4 && readId(d) == chipId);
	if (!forMe) return;

	switch (cmd) {
	case CMD_PING:
		if (addr != ADDR_ALL) { uint8_t r[5] = {state, snsMask(), FW_VERSION, touchSeq, caps}; reply(CMD_PING, r, 5); }
		break;
	case CMD_COLOR:
		if (len >= 4 && myAddr) setAll(d);
		break;
	case CMD_EDGES:
		if (len >= 12 && myAddr) { memcpy(seg, d, 12); state = ST_ACTIVE; ledsDirty = 1; }
		break;
	case CMD_FRAME:
		if (len >= 2 && myAddr && myAddr >= d[0] && myAddr < d[0] + d[1]) {
			uint16_t off = 2 + 4 * (myAddr - d[0]);
			if (off + 4 <= len) setAll(d + off);
		}
		break;
	case CMD_FRAME3:   // wie FRAME, aber drei Farben pro Panel (Kante 1 bis 3)
		if (len >= 2 && myAddr && myAddr >= d[0] && myAddr < d[0] + d[1]) {
			uint16_t off = 2 + 12 * (myAddr - d[0]);
			if (off + 12 <= len) { memcpy(seg, d + off, 12); state = ST_ACTIVE; ledsDirty = 1; }
		}
		break;
	case CMD_PULSE:
		if (myAddr) { state = ST_PULSE; ledsDirty = 1; }
		break;
	case CMD_ORDER:
		if (len >= 4) { memcpy(order, d, 4); ledsDirty = 1; }
		break;
	case CMD_TOUCH:    // [Schwelle], 0 = aus
		if (len >= 1 && d[0] != touchThr) { touchThr = d[0] & 0x7F; tapPendingAt = 0; touchApply(); }
		break;
	case CMD_BEACON:
		if (len >= 1) { beaconOn = d[0] ? 1 : 0; snsApply(); }
		break;
	case CMD_DISCOVER:
		if (!myAddr && len >= 2) {
			uint8_t m = snsMask();
			if (m) {
				uint8_t slots = d[1] ? d[1] : 1;
				uint32_t h = (chipId ^ ((uint32_t)d[0] * 0x9E3779B1u)) * 0x85EBCA6Bu;
				uint8_t slot = (uint8_t)((h >> 24) % slots);
				Delay_Us(slot * 1000);
				uint8_t r[5] = {(uint8_t)chipId, (uint8_t)(chipId >> 8), (uint8_t)(chipId >> 16), (uint8_t)(chipId >> 24), m};
				reply(CMD_DISCOVER, r, 5);
			}
		}
		break;
	case CMD_PROBE:
		if (!myAddr && len >= 5) {
			probeEdge = (d[4] < 3) ? (int8_t)d[4] : -1;
			snsApply();
			reply(CMD_PROBE, 0, 0);
		}
		break;
	case CMD_ASSIGN:
		if (len >= 5 && d[4] >= 1 && d[4] <= 0x3E) {
			uint8_t r[1] = {powerOn};               // 1 = seit dem Einschalten die erste Adresse: wirklich angeklipst
			powerOn = 0;
			myAddr = d[4];
			probeEdge = -1;
			beaconOn = 1;
			state = ST_DARK;
			snsApply();
			ledsDirty = 1;
			reply(CMD_ASSIGN, r, 1);
		}
		break;
	case CMD_RESET:
		myAddr = 0;
		probeEdge = -1;
		beaconOn = 1;
		state = ST_DARK;
		snsApply();
		ledsDirty = 1;
		break;
	case CMD_BOOT:     // nur an die eigene Kurzadresse: in den Bootloader springen
		if (addr != ADDR_ALL) {
			uint8_t ok = bootloaderPresent() ? 1 : 0;
			reply(CMD_BOOT, &ok, 1);
			if (ok) enterBootloader();
		}
		break;
	}
}

// ---------- Empfang: Rahmen zusammensetzen ----------
static uint8_t fr[208];
static uint16_t frPos = 0;
static uint32_t frLast = 0;

static void rxLoop(void) {
	int b;
	while ((b = rxRead()) >= 0) {
		if (frPos && (uint32_t)(ticks() - frLast) > 2 * TICKS_PER_MS) frPos = 0;   // Lücke: neu anfangen
		frLast = ticks();
		if (frPos == 0) { if (b == SYNC) fr[frPos++] = (uint8_t)b; continue; }
		fr[frPos++] = (uint8_t)b;
		if (frPos == 4 && fr[3] > 200) { frPos = 0; continue; }
		if (frPos >= 4 && frPos == (uint16_t)(5 + fr[3])) {
			uint8_t c = 0;
			for (int i = 1; i < frPos - 1; i++) c = crc8(c, fr[i]);
			if (c == fr[frPos - 1] && !(fr[1] & REPLY_FLAG)) handleFrame(fr[1], fr[2], fr + 4, fr[3]);
			frPos = 0;
		}
	}
}

int main(void) {
	SystemInit();
	funGpioInitAll();

#ifndef PANEL_SIM
	powerOn = (RCC->RSTSCKR & RCC_PORRSTF) ? 1 : 0;   // Strom kam gerade erst (nicht nur Neustart nach Update)
	RCC->RSTSCKR |= RCC_RMVF;
#endif
	chipId = *(volatile uint32_t*)0x1FFFF7E8 ^ (*(volatile uint32_t*)0x1FFFF7EC * 31u) ^ (*(volatile uint32_t*)0x1FFFF7F0 * 131u);
	if (chipId == 0) chipId = 1;
	if (bootloaderPresent()) caps |= CAP_BOOT;

	memset(seg, 0, sizeof seg);
	WS2812BDMAInit();
	snsApply();
	uartInit();

	uint32_t lastLed = ticks();
	for (int i = 0; i < 3; i++) { while (WS2812BLEDInUse); WS2812BDMAStart(NUM_SEGMENTS); Delay_Ms(2); }   // sofort dunkel
	touchInit();

	while (1) {
		rxLoop();
		touchLoop();
		uint32_t now = ticks();
		if ((uint32_t)(now - lastLed) > 20 * TICKS_PER_MS && (ledsDirty || state == ST_PULSE) && !WS2812BLEDInUse) {
			lastLed = now;
			ledsDirty = 0;
			WS2812BDMAStart(NUM_SEGMENTS);
		}
	}
}
