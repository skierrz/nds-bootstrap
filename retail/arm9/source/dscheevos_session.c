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

void dscheevosFindSession(configuration* conf) {
	conf->dscheevosModuleCluster = 0;
	conf->dscheevosSessionCluster = 0;
	conf->dscheevosPendingCluster = 0;
	if (conf->b4dsMode || conf->bootstrapOnFlashcard || conf->gameOnFlashcard || conf->isDSiWare) {
		return;
	}

	struct stat stModule, stSession;
	if (stat(DSCHEEVOS_DIR "dscheevos9.bin", &stModule) < 0 || stat(DSCHEEVOS_DIR "session.bin", &stSession) < 0) {
		return;
	}
	if (stModule.st_size > DSCHEEVOS_IMAGE_MAX || stSession.st_size > DSCHEEVOS_SESSION_MAX) {
		return;
	}

	u32 head[(DSCHEEVOS_S_HARDCORE + 4) / 4];
	FILE* f = fopen(DSCHEEVOS_DIR "session.bin", "rb");
	if (!f) {
		return;
	}
	const size_t got = fread(head, 1, sizeof(head), f);
	fclose(f);
	const char* romPath = (const char*)head + DSCHEEVOS_S_ROM_PATH;
	if (got != sizeof(head) || head[0] != DSCHEEVOS_SESSION_MAGIC
	 || head[DSCHEEVOS_S_FILE_SIZE / 4] != (u32)stSession.st_size
	 || strncmp(romPath, conf->ndsPath, 256) != 0) {
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
		conf->cheatSize = 0; // hardcore: no cheats (AP-fix patches still apply)
	}
}
