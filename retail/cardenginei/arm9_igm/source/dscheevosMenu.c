/*
 * DScheevos (RetroAchievements) page in the in-game menu: the game, the
 * mode, progress, and the unlocks of this session. Reads the module's
 * status; the module itself lives in the DScheevos repository.
 */
#ifndef B4DS
#include <stddef.h>
#include <nds/ndstypes.h>
#include <nds/input.h>
#include <nds/system.h>

#include "inGameMenu.h"
#include "dscheevos_hooks.h"

#define KEYS sharedAddr[5]

static const DScheevosStatus* dscheevosStatus(void) {
	if (*(u32*)DSCHEEVOS_LOCATION != DSCHEEVOS_MAGIC) {
		return NULL;
	}
	const u32 status = *(u32*)(DSCHEEVOS_LOCATION + DSCHEEVOS_H_STATUS);
	if (status < DSCHEEVOS_LOCATION || status >= DSCHEEVOS_LOCATION + DSCHEEVOS_IMAGE_MAX) {
		return NULL;
	}
	return (const DScheevosStatus*)status;
}

bool dscheevosHardcore(void) {
	const DScheevosStatus* s = dscheevosStatus();
	return s && s->hardcore;
}

// Copies at most `width` printable characters (UTF-8 and others become '?')
static void fit(unsigned char* out, const char* in, int width) {
	int n = 0;
	while (*in && n < width) {
		const unsigned char c = (unsigned char)*in++;
		if (c >= 0x80 && c < 0xC0) {
			continue; // UTF-8 continuation byte
		}
		out[n++] = (c >= 0x20 && c < 0x7F) ? c : '?';
	}
	out[n] = 0;
}

// Prints a number at its natural width; returns the x after it
static int printNum(int x, int y, u32 val, FontPalette palette) {
	int digits = 1;
	for (u32 v = val; v >= 10; v /= 10) {
		digits++;
	}
	printDec(x, y, val, digits, palette, false);
	return x + digits;
}

static void waitFrame(void) {
	while (REG_VCOUNT != 191);
	while (REG_VCOUNT == 191);
}

void dscheevosMenu(void) {
	const DScheevosStatus* s = dscheevosStatus();
	unsigned char line[33];
	int y;

	clearScreen(false);
	printCenter(16, 0, (const unsigned char*)"RetroAchievements", FONT_WHITE, false);
	if (!s) {
		printCenter(16, 3, (const unsigned char*)"Not active", FONT_LIGHT_GRAY, false);
	} else {
		fit(line, s->gameTitle, 30);
		printCenter(16, 1, line, FONT_LIGHT_GRAY, false);

		int x;
		print(1, 3, (const unsigned char*)(s->hardcore ? "HARDCORE" : "Softcore"), s->hardcore ? FONT_RED : FONT_LIGHT_BLUE, false);
		if (s->hardcore) {
			// hardcore rules always apply; whether RetroAchievements counts the
			// unlocks as hardcore depends on it recognising this client
			print(1, 4, (const unsigned char*)(s->hardcore == DSCHEEVOS_HARDCORE_REFUSED ?
				"Rules on, RA counts as softcore" : "Not yet confirmed by RA"), FONT_LIGHT_GRAY, false);
		}
		x = printNum(11, 3, s->achievementsUnlocked, FONT_WHITE);
		printChar(x, 3, '/', FONT_WHITE, false);
		printNum(x + 1, 3, s->achievements, FONT_WHITE);
		x = printNum(20, 3, s->pointsUnlocked, FONT_WHITE);
		printChar(x, 3, '/', FONT_WHITE, false);
		x = printNum(x + 1, 3, s->points, FONT_WHITE);
		print(x + 1, 3, (const unsigned char*)"pts", FONT_LIGHT_GRAY, false);

		print(1, 5, (const unsigned char*)"Unlocked this session", FONT_LIGHT_BLUE, false);
		if (s->unlockCount == 0) {
			print(2, 6, (const unsigned char*)"None yet", FONT_LIGHT_GRAY, false);
		}
		y = 6;
		for (u32 i = 0; i < s->unlockCount && i < 8; i++) {
			// newest first
			const DScheevosUnlock* u = &s->recent[(s->unlockCount - 1 - i) % 8];
			fit(line, u->title, 25);
			print(2, y, line, FONT_WHITE, false);
			printNum(28, y, u->points, FONT_LIME);
			y++;
		}

		if (s->error) {
			print(1, 15, (const unsigned char*)"Achievements stopped (error)", FONT_RED, false);
		} else {
			const char* net = "No network in this session";
			FontPalette pal = FONT_LIGHT_GRAY;
			switch (s->netState) {
				case DSCHEEVOS_NET_CONNECTING: net = "Connecting..."; break;
				case DSCHEEVOS_NET_ONLINE: net = "Online"; pal = FONT_LIME; break;
				case DSCHEEVOS_NET_SENDING: net = "Online, sending..."; pal = FONT_LIME; break;
				case DSCHEEVOS_NET_PAUSED: net = "Paused: game uses wireless"; break;
				case DSCHEEVOS_NET_OFFLINE: net = "Wi-Fi lost"; pal = FONT_RED; break;
				case DSCHEEVOS_NET_ERROR: net = "Can't reach the server"; pal = FONT_RED; break;
			}
			print(1, 15, (const unsigned char*)net, pal, false);
			print(1, 16, (const unsigned char*)"Sent", FONT_LIGHT_GRAY, false);
			int x = printNum(6, 16, s->unlocksSent, FONT_WHITE);
			if (s->pendingRequests) {
				print(x + 2, 16, (const unsigned char*)"waiting", FONT_LIGHT_GRAY, false);
				printNum(x + 10, 16, s->pendingRequests, FONT_WHITE);
			}
		}
		print(1, 18, (const unsigned char*)"Frames checked", FONT_DARKER_GRAY, false);
		printNum(16, 18, s->framesEvaluated, FONT_DARKER_GRAY);
		if (s->framesDropped) {
			print(1, 19, (const unsigned char*)"Frames skipped", FONT_RED, false);
			printNum(16, 19, s->framesDropped, FONT_RED);
		}
		if (s->netRejoins) {
			print(1, 20, (const unsigned char*)"Wi-Fi rejoins", FONT_DARKER_GRAY, false);
			printNum(16, 20, s->netRejoins, FONT_DARKER_GRAY);
		}
	}
	print(1, 22, (const unsigned char*)"B: Back", FONT_LIGHT_GRAY, false);

	do { waitFrame(); } while (KEYS & (KEY_A | KEY_B));
	do { waitFrame(); } while (!(KEYS & KEY_B));
	do { waitFrame(); } while (KEYS & KEY_B);
}
#endif
