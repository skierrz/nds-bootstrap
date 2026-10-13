/* DScheevos: rcheevos' RetroAchievements hash (DS ROMs only), built from the
 * rcheevos submodule unmodified, the same code the launcher hashes with. */
#define RC_NO_THREADS
#define RC_HASH_NO_DISC
#define RC_HASH_NO_ZIP
#define RC_HASH_NO_ENCRYPTED
/* nds-bootstrap has its own md5_* (polarssl) */
#define md5_init dscheevos_md5_init
#define md5_append dscheevos_md5_append
#define md5_finish dscheevos_md5_finish
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "../../../../../rcheevos/src/util/md5.c"
