/*
  Trilumag – Bootloader für die Panels (CH32V003, Boot-Bereich, höchstens 1920 Bytes)
  ---------------------------------------------------------------------------------
  Läuft nach jedem Einschalten zuerst (Option-Byte START_MODE). Ist die Firmware vollständig
  und hat sie nicht um ein Update gebeten, startet er sie sofort. Sonst wartet er am Bus auf
  das Hauptpanel, das die neue Firmware Seite für Seite (64 Bytes) schreibt.

  Bricht ein Update ab (Strom weg, Hauptpanel neu gestartet), bleibt die Firmware als
  unvollständig markiert. Das Panel bleibt dann im Bootloader, meldet sich auf BL_SCAN
  und bekommt die Firmware vom Hauptpanel automatisch neu.

  Befehle (Rahmen wie im normalen Protokoll, Antworten an 0x80):
    0x21 BL_INFO  [ID×4]                             -> [Version, Firmware ok]
    0x22 BL_WRITE [ID×4, Seite, 64 Bytes]            -> [1 = geschrieben und geprüft]
    0x23 BL_DONE  [ID×4, Seiten, CRC16 lo, hi, Ver.] -> [1 = passt], danach Start der Firmware
    0x24 BL_SCAN  an alle [Runde, Zeitschlitze]      -> im Zeitschlitz [ID×4, Firmware ok]
    0x25 BL_RUN   [ID×4]                             -> [1], danach Start der Firmware
*/

#include "ch32fun.h"
#include <stdint.h>
#include <string.h>

#define BL_VERSION 1
#define BAUD 250000
#ifdef PANEL_SIM                       // Simulation auf dem PC: Flash ist ein Feld im Speicher
#define APP_START ((uintptr_t)simFlash)
#else
#define APP_START ((uintptr_t)0x08000000u)
#endif
#define INFO_ADDR (APP_START + 0x3FC0u)   // letzte Seite: Markierung "Update unvollständig"
#define PAGE 64
#define DIRTY 0xD1A7D1A7u
#define BL_REQUEST_ADDR 0x20000500u
#define BL_REQUEST 0xB007B007u
#define SYNC 0xA5
#define ADDR_BYID 0x7F
#define TPMS (FUNCONF_SYSTEM_CORE_CLOCK / 1000)

#ifndef PANEL_SIM
// Sprung zum Start, danach die Kennung "TLBL" und die Version (die Firmware sucht sie am Anfang des Boot-Bereichs)
void InterruptVector() __attribute__((naked)) __attribute((section(".init")));
void InterruptVector() {
	asm volatile("\n.align 2\n.option push\n.option norvc\nj handle_reset\n.word 0x4C424C54\n.word 1\n.option pop");
}
#endif

static uint32_t chipId;
static uint8_t appOk, dirtySet;
static uint8_t fr[80];
static uint8_t pg[PAGE];
static uint8_t pos;
static uint32_t last, idleMs;

static uint8_t crc8(uint8_t c, uint8_t b) {
	c ^= b;
	for (int i = 0; i < 8; i++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
	return c;
}

#ifdef PANEL_SIM
#define txByte(b) uartTx(b)
#define runApp() simRunApp()
static void flashPage(uintptr_t addr, const uint8_t* data) {
	memset((void*)addr, 0xFF, PAGE);
	if (data) memcpy((void*)addr, data, PAGE);
}
static void reply(uint8_t cmd, const uint8_t* d, uint8_t len) {
	uint8_t c = crc8(crc8(crc8(0, 0x80), cmd), len);
	txByte(SYNC); txByte(0x80); txByte(cmd); txByte(len);
	for (int i = 0; i < len; i++) { txByte(d[i]); c = crc8(c, d[i]); }
	txByte(c);
}
#else
static void txByte(uint8_t b) { while (!(USART1->STATR & USART_STATR_TXE)); USART1->DATAR = b; }

static void reply(uint8_t cmd, const uint8_t* d, uint8_t len) {
	uint32_t t = SysTick->CNT; while (SysTick->CNT - t < 150 * (TPMS / 1000));
	GPIOD->BSHR = 1 << 4;                       // Senden
	uint8_t c = crc8(crc8(crc8(0, 0x80), cmd), len);
	txByte(SYNC); txByte(0x80); txByte(cmd); txByte(len);
	for (int i = 0; i < len; i++) { txByte(d[i]); c = crc8(c, d[i]); }
	txByte(c);
	while (!(USART1->STATR & USART_STATR_TC));
	GPIOD->BSHR = 1 << (16 + 4);                // Empfangen
}

static void runApp(void) {
	FLASH->KEYR = FLASH_KEY1;
	FLASH->KEYR = FLASH_KEY2;
	FLASH->BOOT_MODEKEYR = FLASH_KEY1;
	FLASH->BOOT_MODEKEYR = FLASH_KEY2;
	FLASH->STATR = 0;                            // nach dem Neustart die Firmware im normalen Flash
	FLASH->CTLR = CR_LOCK_Set;
	PFIC->SCTLR = 1u << 31;
	while (1);
}

static void flashWait(void) { while (FLASH->STATR & FLASH_STATR_BSY); }

// Seite löschen und, wenn data gesetzt ist, neu schreiben
static void flashPage(uintptr_t addr, const uint8_t* data) {
	FLASH->KEYR = FLASH_KEY1;
	FLASH->KEYR = FLASH_KEY2;
	FLASH->MODEKEYR = FLASH_KEY1;
	FLASH->MODEKEYR = FLASH_KEY2;
	FLASH->CTLR = CR_PAGE_ER;
	FLASH->ADDR = addr;
	FLASH->CTLR = CR_STRT_Set | CR_PAGE_ER;
	flashWait();
	if (data) {
		FLASH->CTLR = CR_PAGE_PG;
		FLASH->CTLR = CR_BUF_RST | CR_PAGE_PG;
		FLASH->ADDR = addr;
		flashWait();
		for (int i = 0; i < PAGE / 4; i++) {
			uint32_t w;
			memcpy(&w, data + 4 * i, 4);
			((volatile uint32_t*)addr)[i] = w;
			FLASH->CTLR = CR_PAGE_PG | FLASH_CTLR_BUF_LOAD;
			flashWait();
		}
		FLASH->CTLR = CR_PAGE_PG | CR_STRT_Set;
		flashWait();
	}
	FLASH->CTLR = CR_LOCK_Set;
}
#endif

static uint16_t crc16(uint32_t n) {
	uint16_t c = 0xFFFF;
	const uint8_t* p = (const uint8_t*)APP_START;
	while (n--) {
		c ^= (uint16_t)(*p++) << 8;
		for (int i = 0; i < 8; i++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
	}
	return c;
}

static void handle(uint8_t addr, uint8_t cmd, const uint8_t* d, uint8_t len) {
	uint8_t r[5];
	if (addr == 0 && cmd == 0x24 && len >= 2) {          // BL_SCAN: Zeitschlitz wie bei DISCOVER
#ifndef PANEL_SIM
		uint32_t h = (chipId ^ ((uint32_t)d[0] * 0x9E3779B1u)) * 0x85EBCA6Bu;   // Zeitschlitze: Zweierpotenz
		uint32_t wait = ((h >> 24) & (uint8_t)(d[1] - 1)) * TPMS, t = SysTick->CNT;
		while (SysTick->CNT - t < wait);
#endif
		memcpy(r, &chipId, 4); r[4] = appOk;
		reply(cmd, r, 5);
		return;
	}
	if (addr != ADDR_BYID || len < 4 || memcmp(d, &chipId, 4)) return;
	idleMs = 0;
	r[0] = 0;
	switch (cmd) {
	case 0x21:                                          // BL_INFO
		r[0] = BL_VERSION; r[1] = appOk;
		reply(cmd, r, 2);
		return;
	case 0x22: {                                        // BL_WRITE
		uintptr_t a = APP_START + (uint32_t)d[4] * PAGE;
		if (len == 5 + PAGE && a < INFO_ADDR) {
			if (!dirtySet) {                                 // vor dem ersten Schreiben: als unvollständig markieren
				memset(pg, 0xFF, PAGE);
				*(uint32_t*)pg = DIRTY;
				flashPage(INFO_ADDR, pg);
				dirtySet = 1; appOk = 0;
			}
			flashPage(a, d + 5);
			r[0] = memcmp((const void*)a, d + 5, PAGE) == 0;
		}
		reply(cmd, r, 1);
		return;
	}
	case 0x23:                                          // BL_DONE
		if (len >= 7 && d[4] && (uint32_t)d[4] * PAGE <= 0x3FC0u &&
		    crc16((uint32_t)d[4] * PAGE) == (uint16_t)(d[5] | (d[6] << 8))) {
			flashPage(INFO_ADDR, 0);                         // Markierung löschen: Firmware vollständig
			appOk = 1; dirtySet = 0;
			r[0] = 1;
			reply(cmd, r, 1);
			runApp();
			return;
		}
		reply(cmd, r, 1);
		return;
	case 0x25:                                          // BL_RUN
		r[0] = appOk;
		reply(cmd, r, 1);
		if (appOk) runApp();
		return;
	}
}

// Ein empfangenes Byte: Rahmen zusammensetzen und auswerten
static void blByte(uint8_t b) {
	uint32_t now = SysTick->CNT;
	if (pos && now - last > 2 * TPMS) pos = 0;
	last = now;
	if (!pos) { if (b == SYNC) fr[pos++] = b; return; }
	fr[pos++] = b;
	if (pos == 4 && fr[3] > sizeof fr - 5) { pos = 0; return; }
	if (pos >= 4 && pos == 5 + fr[3]) {
		uint8_t c = 0;
		for (int i = 1; i < pos - 1; i++) c = crc8(c, fr[i]);
		pos = 0;
		if (c == fr[4 + fr[3]] && !(fr[1] & 0x80)) handle(fr[1], fr[2], fr + 4, fr[3]);
	}
}

static uint32_t makeChipId(void) {
	uint32_t id = *(volatile uint32_t*)0x1FFFF7E8 ^ (*(volatile uint32_t*)0x1FFFF7EC * 31u) ^ (*(volatile uint32_t*)0x1FFFF7F0 * 131u);
	return id ? id : 1;
}

static int appValid(void) { return *(volatile uint32_t*)APP_START != 0xFFFFFFFFu && *(volatile uint32_t*)INFO_ADDR != DIRTY; }

#ifndef PANEL_SIM
int main(void) {
	SystemInit();
	uint32_t req = *(volatile uint32_t*)BL_REQUEST_ADDR;
	*(volatile uint32_t*)BL_REQUEST_ADDR = 0;
	appOk = appValid();
	if (appOk && req != BL_REQUEST) runApp();          // Normalfall: sofort weiter zur Firmware

	chipId = makeChipId();
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1 | RCC_APB2Periph_AFIO;
	// PD4 Ausgang (Senderichtung), PD5 TX alternativ, PD6 RX Eingang mit Pull-up
	GPIOD->CFGLR = (GPIOD->CFGLR & ~(0xFFFu << 16)) | (0x3u << 16) | (0xBu << 20) | (0x8u << 24);
	GPIOD->BSHR = (1 << 6) | (1 << (16 + 4));
	USART1->BRR = FUNCONF_SYSTEM_CORE_CLOCK / BAUD;
	USART1->CTLR1 = USART_CTLR1_UE | USART_CTLR1_TE | USART_CTLR1_RE;

	uint32_t tick = SysTick->CNT;
	while (1) {
		if (SysTick->CNT - tick >= TPMS) { tick += TPMS; idleMs++; }
		if (appOk && !dirtySet && idleMs > 5000) runApp(); // Hauptpanel meldet sich nicht: zurück zur Firmware
		if (USART1->STATR & USART_STATR_ORE) (void)USART1->DATAR;
		if (USART1->STATR & USART_STATR_RXNE) blByte((uint8_t)USART1->DATAR);
	}
}
#endif
