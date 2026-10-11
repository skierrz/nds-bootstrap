/*
 * DScheevos (RetroAchievements) hook points in the DSi-mode bootloader:
 * load the module and the session file into the RAM the ROM cache gave up,
 * point the game's OS_Halt at the module, and switch on the card engine
 * hooks (ARM7 VBlank count + IPC sync, ARM9 IPC handler call). The module
 * is initialized by the bootloader's ARM9 before the game starts.
 *
 * Only DS (NTR-mode) games booted from SD use this.
 */
#include <nds/ndstypes.h>
#include <nds/memory.h>
#include "my_fat.h"
#include "debug_file.h"
#include "nds_header.h"
#include "cardengine_header_arm7.h"
#include "cardengine_header_arm9.h"
#include "common.h"
#include "value_bits.h"
#include "dscheevos_hooks.h"

extern u32 dscheevosModuleCluster;
extern u32 dscheevosSessionCluster;
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

	if (patchOsHalt(ndsHeader, header[DSCHEEVOS_H_HALT / 4]) == 0) {
		disable("no OS_Halt");
		return;
	}

	header[DSCHEEVOS_H_VBLANK_COUNT / 4] = 0;
	ce7->valueBits |= DSCHEEVOS_VALUEBIT;
	ce9->valueBits |= DSCHEEVOS_VALUEBIT;

	arm9_stateFlag = ARM9_DSCHEEVOS_INIT;
	while (arm9_stateFlag != ARM9_READY);
	dbg_printf("DScheevos: module ready\n");
}
