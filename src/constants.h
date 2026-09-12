#ifndef SPECS_CONSTANTS_H
#define SPECS_CONSTANTS_H

#include "prelude.h"

#define MDASH "\xE2\x80\x94"

#define ARENA_SIZE        ((usize)(1u << 20))
#define WORKER_COUNT      ((usize)8)
#define WORKER_ARENA_SIZE ((usize)(128u << 10))
#define PATH_BUF_MAX      ((usize)4096)
#define CMD_PATH_MAX      247
#define CACHE_LINE        64
#define MAX_PATH_DIRS     ((usize)256)
#define MAX_LSBLK_ROWS    ((usize)192)
#define MAX_IFACES        ((usize)64)
#define MAX_PARTS         ((usize)32)
#define MAX_SWAP_FILES    ((usize)8)
#define MAX_CMDS          ((usize)16)
#define CAPTURE_CHUNK     ((usize)4096)
#define PREFETCH_DISTANCE 64
#define FULL_PATH_ALIGNMENT 16
#define ARENA_ALIGN       16
#define DIRFD_NONE        (-1)
#define BUF_INITIAL_CAP   ((usize)64)

#define ERR_PREFIX         "[ERR] "

#define SKIP_ZRAM  "zram"
#define SKIP_LOOP  "loop"
#define SKIP_RAM   "ram"
#define SKIP_ZRAM_LEN 4
#define SKIP_LOOP_LEN 4
#define SKIP_RAM_LEN  3

#define LSBLK_COLS "NAME,ROTA,SIZE,TRAN,TYPE,PKNAME,PARTTYPENAME,FSTYPE,MOUNTPOINT"

#endif /* SPECS_CONSTANTS_H */
