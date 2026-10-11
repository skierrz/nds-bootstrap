/*
 * DScheevos hook points (RetroAchievements). The module itself lives in the
 * DScheevos repository (dscheevos/module); these are the few addresses and
 * offsets nds-bootstrap needs to load it and call into it. They must match
 * dscheevos_module.h there.
 */
#ifndef DSCHEEVOS_HOOKS_H
#define DSCHEEVOS_HOOKS_H

#define DSCHEEVOS_LOCATION        0x02D80000  // module image (2 MB reserved)
#define DSCHEEVOS_SIZE            0x00200000
#define DSCHEEVOS_IMAGE_MAX       0x00040000
#define DSCHEEVOS_SESSION         (DSCHEEVOS_LOCATION + DSCHEEVOS_IMAGE_MAX)
#define DSCHEEVOS_SESSION_MAX     0x00060000
#define DSCHEEVOS_CACHE_END       0x0CD80000  // ROM cache must end here (same RAM as DSCHEEVOS_LOCATION)
#define DSCHEEVOS_CACHE_RESERVE   0x00260000  // worst-case cut to the ROM cache size

#define DSCHEEVOS_MAGIC           0x56435344  // "DSCV"
#define DSCHEEVOS_VERSION         2
#define DSCHEEVOS_SESSION_MAGIC   0x53435344  // "DSCS"

// header words
#define DSCHEEVOS_H_MAGIC         0x00
#define DSCHEEVOS_H_VERSION       0x04
#define DSCHEEVOS_H_IMAGE_SIZE    0x08
#define DSCHEEVOS_H_INIT          0x0C
#define DSCHEEVOS_H_VBLANK        0x10
#define DSCHEEVOS_H_HALT          0x14
#define DSCHEEVOS_H_VBLANK_COUNT  0x24

// session file words
#define DSCHEEVOS_S_FILE_SIZE     0x08
#define DSCHEEVOS_S_ROM_PATH      0x0C  // char[256]

// valueBits flag in both ce7 and ce9: hooks are live
#define DSCHEEVOS_VALUEBIT        BIT(28)

#endif // DSCHEEVOS_HOOKS_H
