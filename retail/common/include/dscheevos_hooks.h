/*
 * DScheevos hook points (RetroAchievements). The module itself lives in the
 * DScheevos repository (dscheevos/module); these are the few addresses and
 * offsets nds-bootstrap needs to load it and call into it. They must match
 * dscheevos_module.h there.
 */
#ifndef DSCHEEVOS_HOOKS_H
#define DSCHEEVOS_HOOKS_H

#include <nds/ndstypes.h>

#define DSCHEEVOS_LOCATION        0x02D80000  // module image (2 MB reserved)
#define DSCHEEVOS_SIZE            0x00200000
#define DSCHEEVOS_IMAGE_MAX       0x00080000
#define DSCHEEVOS_SESSION         (DSCHEEVOS_LOCATION + DSCHEEVOS_IMAGE_MAX)
#define DSCHEEVOS_SESSION_MAX     0x00040000
#define DSCHEEVOS_CACHE_END       0x0CD80000  // ROM cache must end here (same RAM as DSCHEEVOS_LOCATION)
#define DSCHEEVOS_CACHE_RESERVE   0x00260000  // worst-case cut to the ROM cache size

#define DSCHEEVOS_MAGIC           0x56435344  // "DSCV"
#define DSCHEEVOS_VERSION         5
#define DSCHEEVOS_SESSION_MAGIC   0x53435344  // "DSCS"

// header words
#define DSCHEEVOS_H_MAGIC         0x00
#define DSCHEEVOS_H_VERSION       0x04
#define DSCHEEVOS_H_IMAGE_SIZE    0x08
#define DSCHEEVOS_H_INIT          0x0C
#define DSCHEEVOS_H_VBLANK        0x10
#define DSCHEEVOS_H_HALT          0x14
#define DSCHEEVOS_H_STATUS        0x1C
#define DSCHEEVOS_H_VBLANK_COUNT  0x24
#define DSCHEEVOS_H_RESET         0x28
#define DSCHEEVOS_H_NOTIFY        0x40  // unlock count, kept in RAM by the ARM9
#define DSCHEEVOS_H_NETRING       0x44  // the module's frame ring for the ARM7 SDIO shim (dscm_shim.h), 0 = none

// session file words
#define DSCHEEVOS_S_SIZE          0x04  // sizeof(dscm_boot_t)
#define DSCHEEVOS_S_FILE_SIZE     0x08
#define DSCHEEVOS_S_ROM_PATH      0x0C  // char[256]
#define DSCHEEVOS_S_HARDCORE      0x1CD // u8
#define DSCHEEVOS_S_NET_VALID     0x1F6 // u8: the game keeps the launcher's Wi-Fi connection
#define DSCHEEVOS_S_AP_SSID       0x210 // char[32]: the access point the launcher joined
#define DSCHEEVOS_S_AP_SSID_LEN   0x230 // u8
#define DSCHEEVOS_S_AP_SECURITY   0x238 // u8: 3 = WPA2
#define DSCHEEVOS_S_PMK_VALID     0x23B // u8, set here in RAM only
#define DSCHEEVOS_S_PMK           0x23C // u8[32], filled here in RAM only
#define DSCHEEVOS_S_BOOT_SIZE     0x25C
#define DSCHEEVOS_AP_WPA2         3

// valueBits flag in both ce7 and ce9: hooks are live
#define DSCHEEVOS_VALUEBIT        BIT(28)

// The module's status (dscm_status_t in dscheevos_module.h), read by the in-game menu
typedef struct DScheevosUnlock {
	u32 id;
	u32 points;
	u32 frame;
	char title[48];
} DScheevosUnlock;

typedef struct DScheevosStatus {
	u32 active;
	u32 hardcore;
	u32 framesSnapshotted;
	u32 framesEvaluated;
	u32 framesDropped;
	u32 framesMissed;
	u32 evalLines;
	u32 unlockCount;
	DScheevosUnlock recent[8];
	u32 pendingRequests;
	u32 error;
	char gameTitle[48];
	u32 achievements;
	u32 achievementsUnlocked;
	u32 points;
	u32 pointsUnlocked;
	u32 netState; // DSCHEEVOS_NET_*
	u32 netDetail;
	u32 unlocksSent;
	u32 tlsHandshakeMs;
	u32 netRejoins; // times the module reassociated (AP drop, group rekey)
} DScheevosStatus;

enum {
	DSCHEEVOS_NET_NONE = 0,
	DSCHEEVOS_NET_CONNECTING,
	DSCHEEVOS_NET_ONLINE,
	DSCHEEVOS_NET_SENDING,
	DSCHEEVOS_NET_PAUSED,
	DSCHEEVOS_NET_OFFLINE,
	DSCHEEVOS_NET_ERROR,
};

#endif // DSCHEEVOS_HOOKS_H
