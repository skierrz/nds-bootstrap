/*
 * DScheevos (RetroAchievements) hook: find the module and a session file
 * the DScheevos launcher wrote for this ROM, and pass their clusters to the
 * bootloader. The session is renamed so it is used once: every play session
 * starts online through the launcher. The renamed file keeps its clusters.
 * pending.bin (the offline journal, created by the launcher at its full
 * size) is passed on too: the card engine writes it at safe points.
 */
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "configuration.h"
#include "dscheevos_hooks.h"

#define DSCHEEVOS_DIR "sd:/_nds/dscheevos/"
#define DSCHEEVOS_SESSION_HEAD ((DSCHEEVOS_S_HARDCORE + 4) & ~3)

// Reads the head of the session.bin the launcher wrote for this ROM.
// Returns 0 when there is none, it's for another ROM, or achievements can't
// run here: the boot is then stock.
static int readSession(const configuration* conf, u32* head, struct stat* stModule, struct stat* stSession) {
	if (conf->b4dsMode || conf->bootstrapOnFlashcard || conf->gameOnFlashcard || conf->isDSiWare) {
		return 0;
	}
	if (stat(DSCHEEVOS_DIR "dscheevos9.bin", stModule) < 0 || stat(DSCHEEVOS_DIR "session.bin", stSession) < 0) {
		return 0;
	}
	if (stModule->st_size > DSCHEEVOS_IMAGE_MAX || stSession->st_size > DSCHEEVOS_SESSION_MAX) {
		return 0;
	}
	FILE* f = fopen(DSCHEEVOS_DIR "session.bin", "rb");
	if (!f) {
		return 0;
	}
	const size_t got = fread(head, 1, DSCHEEVOS_SESSION_HEAD, f);
	fclose(f);
	const char* romPath = (const char*)head + DSCHEEVOS_S_ROM_PATH;
	return got == DSCHEEVOS_SESSION_HEAD && head[0] == DSCHEEVOS_SESSION_MAGIC
	 && head[DSCHEEVOS_S_FILE_SIZE / 4] == (u32)stSession->st_size
	 && strncmp(romPath, conf->ndsPath, 256) == 0;
}

// Hardcore: nds-bootstrap applies only its built-in AP-fix patches
// (nitro:/apfix.pck), never user files from _nds/nds-bootstrap/apFix/,
// which could patch the game after the launcher hashed the clean ROM.
// Called by loadApFix() before dscheevosFindSession(); both read the same file.
int dscheevosHardcorePending(const configuration* conf) {
	u32 head[DSCHEEVOS_SESSION_HEAD / 4];
	struct stat stModule, stSession;
	return readSession(conf, head, &stModule, &stSession) && ((const u8*)head)[DSCHEEVOS_S_HARDCORE];
}

// softResetParams.bin is how a reset that reboots the console hands over to
// the next boot. Two of its words make that boot run code the launcher never
// hashed: 'LOAD' at 8 boots ARM9/ARM7 binaries from pagefile.sys (a
// downloaded DS Download Play child), and a ROM offset at 0xC boots another
// SRL inside the ROM file. A DScheevos session always starts fresh from the
// launcher (each reboot goes through it and starts a new session), so a
// hardcore boot clears both before nds-bootstrap reads them; the game then
// starts from its own binaries. The reset parameters and boot info stay.
void dscheevosSanitizeSoftReset(const configuration* conf, const char* srParamsPath) {
	if (!dscheevosHardcorePending(conf)) {
		return;
	}
	FILE* f = fopen(srParamsPath, "rb+");
	if (!f) {
		return;
	}
	u32 words[4] = {0};
	if (fread(words, 1, sizeof(words), f) == sizeof(words) && (words[2] == 0x44414F4C || words[3] != 0)) { // 'LOAD'
		const u32 zero[2] = {0, 0};
		fseek(f, 8, SEEK_SET);
		fwrite(zero, 1, sizeof(zero), f);
	}
	fclose(f);
}

void dscheevosFindSession(configuration* conf) {
	conf->dscheevosModuleCluster = 0;
	conf->dscheevosSessionCluster = 0;
	conf->dscheevosPendingCluster = 0;

	u32 head[DSCHEEVOS_SESSION_HEAD / 4];
	struct stat stModule, stSession;
	if (!readSession(conf, head, &stModule, &stSession)) {
		return;
	}

	remove(DSCHEEVOS_DIR "session.used");
	if (rename(DSCHEEVOS_DIR "session.bin", DSCHEEVOS_DIR "session.used") != 0) {
		return;
	}
	conf->dscheevosModuleCluster = stModule.st_ino;
	conf->dscheevosSessionCluster = stSession.st_ino;
	struct stat stPending;
	if (stat(DSCHEEVOS_DIR "pending.bin", &stPending) == 0 && stPending.st_size == DSCHEEVOS_JOURNAL_FILE_SIZE) {
		conf->dscheevosPendingCluster = stPending.st_ino;
	}
	if (((const u8*)head)[DSCHEEVOS_S_HARDCORE]) {
		// hardcore: no user cheats, and no wideCheatData.bin (any AR codes put
		// there run like cheats). AP-fix: built-in patches only, see above.
		conf->cheatSize = 0;
		conf->wideCheatSize = 0;
	}
}
