#define _GNU_SOURCE

#include "gather.h"

#include "capture.h"
#include "format.h"
#include "parse.h"

#include <pthread.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum {
  W_CPU = 0,
  W_RAM,
  W_GPU,
  W_MOBO,
  W_DISKS,
  W_SWAPS,
  W_NET,
  W_OS,
};

typedef struct {
  Arena arena;
  u8    mem[WORKER_ARENA_SIZE];
  c8   *p0;
  c8   *p1;
  c8   *p2;
  i64   nproc;
} __attribute__((aligned(CACHE_LINE))) WorkerCtx;

_Static_assert(alignof(WorkerCtx) >= CACHE_LINE, "WorkerCtx must be cache-line aligned");
_Static_assert(WORKER_COUNT == 8, "worker id enum matches WORKER_COUNT");

static u8    g_doc_mem[ARENA_SIZE] __attribute__((aligned(CACHE_LINE)));
static Arena g_doc_arena;
static bool  g_runtime_ready = false;

static WorkerCtx g_workers[WORKER_COUNT] __attribute__((aligned(CACHE_LINE)));

void specs_runtime_init(
  void
) {
  if (LIKELY(g_runtime_ready)) return;
  arena_init(&g_doc_arena, g_doc_mem, sizeof(g_doc_mem));
  capture_init();
  for (usize idx = 0; idx < WORKER_COUNT; idx++) {
    arena_init(&g_workers[idx].arena, g_workers[idx].mem, sizeof(g_workers[idx].mem));
  }
  g_runtime_ready = true;
}

void specs_runtime_fini(
  void
) {
  capture_fini();
  g_runtime_ready = false;
}

Arena *specs_doc_arena(
  void
) {
  specs_runtime_init();
  return &g_doc_arena;
}

static c8 *cap1(
  Arena *arena, bool allow, const c8 *a0
) {
  const c8 *argv[] = {a0, nullptr};
  c8       *out    = nullptr;
  capture_argv(arena, argv, allow, &out);
  return out;
}

static c8 *cap2(
  Arena *arena, bool allow, const c8 *a0, const c8 *a1
) {
  const c8 *argv[] = {a0, a1, nullptr};
  c8       *out    = nullptr;
  capture_argv(arena, argv, allow, &out);
  return out;
}

static c8 *cap3(
  Arena *arena, bool allow, const c8 *a0, const c8 *a1, const c8 *a2
) {
  const c8 *argv[] = {a0, a1, a2, nullptr};
  c8       *out    = nullptr;
  capture_argv(arena, argv, allow, &out);
  return out;
}

static c8 *cap4(
  Arena *arena, bool allow, const c8 *a0, const c8 *a1, const c8 *a2, const c8 *a3
) {
  const c8 *argv[] = {a0, a1, a2, a3, nullptr};
  c8       *out    = nullptr;
  capture_argv(arena, argv, allow, &out);
  return out;
}

static void *work_cpu(
  void *arg
) {
  WorkerCtx *ctx = arg;
  ctx->p0        = cap1(&ctx->arena, false, "lscpu");
  ctx->p1        = capture_read_file(&ctx->arena, "/sys/devices/system/cpu/smt/active");
  i64 nproc      = sysconf(_SC_NPROCESSORS_ONLN);
  ctx->nproc     = nproc < 1 ? 1 : nproc;
  return nullptr;
}

static void *work_ram(
  void *arg
) {
  WorkerCtx *ctx = arg;
  ctx->p0        = capture_read_file(&ctx->arena, "/proc/meminfo");
  if (capture_resolve("inxi")) {
    const c8 *argv[] = {"inxi", "-c", "0", "-m", nullptr};
    capture_argv(&ctx->arena, argv, true, &ctx->p1);
  } else {
    ctx->p1 = arena_copy_str(&ctx->arena, "");
  }
  return nullptr;
}

static void *work_gpu(
  void *arg
) {
  WorkerCtx *ctx   = arg;
  const c8  *smi[] = {"nvidia-smi", "--query-gpu=name,memory.total", "--format=csv,noheader", nullptr};
  capture_argv(&ctx->arena, smi, true, &ctx->p0);
  ctx->p1 = cap1(&ctx->arena, true, "lspci");
  return nullptr;
}

static void *work_mobo(
  void *arg
) {
  WorkerCtx *ctx = arg;
  ctx->p0        = capture_read_file(&ctx->arena, "/sys/devices/virtual/dmi/id/board_vendor");
  ctx->p1        = capture_read_file(&ctx->arena, "/sys/devices/virtual/dmi/id/board_name");
  return nullptr;
}

static void *work_disks(
  void *arg
) {
  WorkerCtx *ctx     = arg;
  const c8  *lsblk[] = {"lsblk", "-nP", "-o", LSBLK_COLS, nullptr};
  capture_argv(&ctx->arena, lsblk, false, &ctx->p0);

  ctx->p1      = cap4(&ctx->arena, true, "findmnt", "-no", "SOURCE", "/boot");
  c8 *boot_src = trim_copy(&ctx->arena, first_line_copy(&ctx->arena, ctx->p1));
  ctx->p1      = boot_src;
  if (boot_src[0]) {
    const c8 *pk[]   = {"lsblk", "-no", "PKNAME", boot_src, nullptr};
    c8       *pkname = nullptr;
    capture_argv(&ctx->arena, pk, true, &pkname);
    ctx->p2 = trim_copy(&ctx->arena, first_line_copy(&ctx->arena, pkname));
  } else {
    ctx->p2 = arena_copy_str(&ctx->arena, "");
  }
  return nullptr;
}

static void *work_swaps(
  void *arg
) {
  WorkerCtx *ctx = arg;
  ctx->p0        = capture_read_file(&ctx->arena, "/proc/swaps");
  return nullptr;
}

static void *work_net(
  void *arg
) {
  WorkerCtx *ctx = arg;
  ctx->p0        = cap3(&ctx->arena, true, "ip", "-br", "link");
  return nullptr;
}

static void *work_os(
  void *arg
) {
  WorkerCtx *ctx = arg;
  ctx->p0        = capture_read_file(&ctx->arena, "/etc/os-release");
  ctx->p1        = cap2(&ctx->arena, false, "uname", "-r");
  return nullptr;
}

static bool want_parallel(
  void
) {
  const c8 *jobs = getenv("SPECS_OUT_JOBS");
  return !(jobs && jobs[0] == '1' && jobs[1] == '\0');
}

static usize collect_swap_files(
  Arena *arena, const c8 *swaps, SwapFileHint *files
) {
  SwapTable table = swaps_parse(arena, swaps);
  usize     count = 0;
  for (usize idx = 0; idx < table.count && count < MAX_SWAP_FILES; idx++) {
    const c8 *path = table.paths[idx];
    if (!path || !path[0]) continue;
    if (strncmp(path, "/dev/", 5) == 0) continue;
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;
    const c8 *argv[] = {"du", "-h", path, nullptr};
    c8       *du     = nullptr;
    capture_argv(arena, argv, true, &du);
    c8 *line = first_line_copy(arena, du);
    c8 *end  = line;
    while (*end && *end != ' ' && *end != '\t')
      end++;
    *end = '\0';
    if (!line[0]) continue;
    files[count].path = path;
    files[count].size = line;
    count++;
  }
  return count;
}

c8 *specs_gather_document(
  Arena *arena
) {
  specs_runtime_init();

  typedef void *(*WorkFn)(void *);
  static const WorkFn jobs[WORKER_COUNT] = {
    work_cpu,
    work_ram,
    work_gpu,
    work_mobo,
    work_disks,
    work_swaps,
    work_net,
    work_os,
  };

  for (usize idx = 0; idx < WORKER_COUNT; idx++) {
    arena_reset(&g_workers[idx].arena);
    g_workers[idx].p0    = nullptr;
    g_workers[idx].p1    = nullptr;
    g_workers[idx].p2    = nullptr;
    g_workers[idx].nproc = 0;
  }

  /* PATH/cmd cache is filled on this thread in capture_init (via runtime_init)
   * before any worker posix_spawns, so resolve hits filled slots. */
  if (want_parallel()) {
    pthread_t threads[WORKER_COUNT];
    bool      live[WORKER_COUNT] = {};
    for (usize idx = 0; idx < WORKER_COUNT; idx++) {
      if (pthread_create(&threads[idx], nullptr, jobs[idx], &g_workers[idx]) == 0) {
        live[idx] = true;
      } else {
        jobs[idx](&g_workers[idx]);
      }
    }
    for (usize idx = 0; idx < WORKER_COUNT; idx++) {
      if (live[idx]) pthread_join(threads[idx], nullptr);
    }
  } else {
    for (usize idx = 0; idx < WORKER_COUNT; idx++)
      jobs[idx](&g_workers[idx]);
  }

  SwapFileHint files[MAX_SWAP_FILES] = {};
  usize        file_count            = collect_swap_files(arena, g_workers[W_SWAPS].p0, files);

  LsblkTable disks                   = lsblk_parse(arena, g_workers[W_DISKS].p0);
  const c8  *cpu                     = format_cpu(arena, g_workers[W_CPU].p0, g_workers[W_CPU].p1, g_workers[W_CPU].nproc);
  const c8  *ram                     = format_ram(arena, g_workers[W_RAM].p0, g_workers[W_RAM].p1);
  const c8  *gpu                     = format_gpu(arena, g_workers[W_GPU].p0, g_workers[W_GPU].p1);
  const c8  *mobo                    = format_mobo(arena, g_workers[W_MOBO].p0, g_workers[W_MOBO].p1);
  const c8  *disk                    = format_disks(arena, &disks, g_workers[W_DISKS].p2);
  const c8  *swap                    = format_swap(arena, g_workers[W_SWAPS].p0, &disks, files, file_count);
  const c8  *net                     = format_network(arena, g_workers[W_NET].p0);
  const c8  *os                      = format_os(arena, g_workers[W_OS].p0, g_workers[W_OS].p1);
  return specs_build_doc(arena, cpu, ram, gpu, mobo, disk, swap, net, os);
}
