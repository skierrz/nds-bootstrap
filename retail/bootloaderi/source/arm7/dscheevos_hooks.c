/*
 * DScheevos (RetroAchievements) hook points in the DSi-mode bootloader:
 * load the module and the session file into the RAM the ROM cache gave up,
 * point the game's OS_Halt at the module, and switch on the card engine
 * hooks (ARM7 VBlank count + IPC sync, ARM9 IPC handler call). The module
 * is initialized by the bootloader's ARM9 before the game starts.
 *
 * Only DS (NTR-mode) games booted from SD use this.
 */
#include <string.h>
#include <nds/ndstypes.h>
#include <nds/memory.h>
#include "tonccpy.h"
#include "my_fat.h"
#include "debug_file.h"
#include "nds_header.h"
#include "cardengine_header_arm7.h"
#include "cardengine_header_arm9.h"
#include "common.h"
#include "value_bits.h"
#include "dscheevos_hooks.h"

extern void readFirmware(u32 address, void* destination, u32 size); // libnds/firmware.c
extern u32 dscheevosModuleCluster;
extern u32 dscheevosSessionCluster;
extern u32 dscheevosPendingCluster;
extern u32 iUncompressedSize;

bool dscheevosWanted(void) {
	return dscheevosModuleCluster && dscheevosSessionCluster && !dsiModeConfirmed && !isDSiWare;
}

// The session keeps the launcher's Wi-Fi connection during play: the DSi
// Wi-Fi chip must stay selected (no DS-mode Wi-Fi at boot).
bool dscheevosNetWanted(void) {
	static int cached = -1;
	if (cached < 0) {
		u8 valid = 0;
		if (dscheevosWanted()) {
			aFile file;
			getFileFromCluster(&file, dscheevosSessionCluster, false);
			fileRead((char*)&valid, &file, DSCHEEVOS_S_NET_VALID, 1);
		}
		cached = valid ? 1 : 0;
	}
	return cached;
}

// NitroSDK OS_Halt: mov r0, #0; mcr p15, 0, r0, c7, c0, 4; bx lr
static int patchOsHalt(const tNDSHeader* ndsHeader, u32 haltEntry) {
	u32* p = (u32*)ndsHeader->arm9destination;
	u32* end = p + (iUncompressedSize ? iUncompressedSize : ndsHeader->arm9binarySize) / 4 - 2;
	int count = 0;
	for (; p < end; p++) {
		if (p[0] == 0xE3A00000 && p[1] == 0xEE070F90 && p[2] == 0xE12FFF1E) {
			p[0] = 0xE51FF004; // ldr pc, [pc, #-4]
			p[1] = haltEntry;
			dbg_printf("DScheevos: OS_Halt at ");
			dbg_hexa((u32)p);
			dbg_printf("\n");
			count++;
		}
	}
	return count;
}

static u16 crc16(const u8* data, int length) {
	u32 crc = 0;
	for (int i = 0; i < length; i++) {
		crc ^= data[i];
		for (int j = 0; j < 8; j++) {
			crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
		}
	}
	return crc;
}

// WPA2 rekeys during play need the access point's PMK. The console keeps it
// with its own saved Wi-Fi settings (DSi slots 4-6 in the firmware flash):
// read it, read-only, into the session in RAM. It never goes to the SD card.
static void fillPmk(u8* session) {
	session[DSCHEEVOS_S_PMK_VALID] = 0;
	toncset(session + DSCHEEVOS_S_PMK, 0, 32);
	const u32 ssidLen = session[DSCHEEVOS_S_AP_SSID_LEN];
	if (!session[DSCHEEVOS_S_NET_VALID] || session[DSCHEEVOS_S_AP_SECURITY] != DSCHEEVOS_AP_WPA2
	 || ssidLen == 0 || ssidLen > 32) {
		return;
	}

	u16 settingsOffset = 0;
	readFirmware(0x20, &settingsOffset, sizeof(settingsOffset));
	const u32 base = settingsOffset * 8 - 0xA00;
	u8* slot = (u8*)(DSCHEEVOS_SESSION + DSCHEEVOS_SESSION_MAX); // module heap, not in use yet
	for (int i = 0; i < 3; i++) {
		readFirmware(base + i * 0x200, slot, 0x200);
		if (crc16(slot, 0x100) != 0 || crc16(slot + 0x100, 0x100) != 0
		 || slot[0xEF] == 0       // slot unused
		 || slot[0xE7] == 0       // not WPA
		 || memcmp(slot + 0x40, session + DSCHEEVOS_S_AP_SSID, ssidLen) != 0
		 || (ssidLen < 32 && slot[0x40 + ssidLen] != 0)) {
			continue;
		}
		tonccpy(session + DSCHEEVOS_S_PMK, slot + 0x100, 32);
		session[DSCHEEVOS_S_PMK_VALID] = 1;
		dbg_printf("DScheevos: PMK from Wi-Fi slot ");
		dbg_hexa(4 + i);
		dbg_printf("\n");
		break;
	}
	toncset(slot, 0, 0x200);
}

static void disable(const char* why) {
	*(vu32*)(DSCHEEVOS_LOCATION + DSCHEEVOS_H_MAGIC) = 0;
	dbg_printf("DScheevos off: ");
	dbg_printf(why);
	dbg_printf("\n");
}

void dscheevosSetup(cardengineArm7* ce7, cardengineArm9* ce9, const tNDSHeader* ndsHeader) {
	if (!dscheevosWanted()) {
		return;
	}

	aFile file;
	getFileFromCluster(&file, dscheevosModuleCluster, false);
	u32* header = (u32*)DSCHEEVOS_LOCATION;
	fileRead((char*)header, &file, 0, 0x28);
	const u32 imageSize = header[DSCHEEVOS_H_IMAGE_SIZE / 4];
	if (header[DSCHEEVOS_H_MAGIC / 4] != DSCHEEVOS_MAGIC || header[DSCHEEVOS_H_VERSION / 4] != DSCHEEVOS_VERSION
	 || imageSize < 0x28 || imageSize > DSCHEEVOS_IMAGE_MAX) {
		disable("bad module");
		return;
	}
	fileRead((char*)header, &file, 0, imageSize);

	getFileFromCluster(&file, dscheevosSessionCluster, false);
	u32* session = (u32*)DSCHEEVOS_SESSION;
	fileRead((char*)session, &file, 0, 0x10);
	const u32 sessionSize = session[DSCHEEVOS_S_FILE_SIZE / 4];
	if (session[0] != DSCHEEVOS_SESSION_MAGIC || sessionSize < 0x10 || sessionSize > DSCHEEVOS_SESSION_MAX) {
		disable("bad session");
		return;
	}
	fileRead((char*)session, &file, 0, sessionSize);
	if (session[DSCHEEVOS_S_SIZE / 4] != DSCHEEVOS_S_BOOT_SIZE || sessionSize < DSCHEEVOS_S_BOOT_SIZE) {
		disable("old session");
		return;
	}
	fillPmk((u8*)session);

	if (patchOsHalt(ndsHeader, header[DSCHEEVOS_H_HALT / 4]) == 0) {
		disable("no OS_Halt");
		return;
	}

	header[DSCHEEVOS_H_VBLANK_COUNT / 4] = 0;
	ce7->dscheevosPendingCluster = dscheevosPendingCluster;
	ce7->valueBits |= DSCHEEVOS_VALUEBIT;
	ce9->valueBits |= DSCHEEVOS_VALUEBIT;

	arm9_stateFlag = ARM9_DSCHEEVOS_INIT;
	while (arm9_stateFlag != ARM9_READY);
	dbg_printf("DScheevos: module ready\n");
	if (!dscheevosPendingCluster) {
		dbg_printf("DScheevos: no pending.bin, unsent unlocks won't be kept\n");
	}
}
