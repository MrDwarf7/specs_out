#define _GNU_SOURCE

#include "capture.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <spawn.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static pthread_mutex_t g_capture_mu    = PTHREAD_MUTEX_INITIALIZER;

static i32  g_proc_dirfd               = DIRFD_NONE;
static i32  g_sys_dirfd                = DIRFD_NONE;
static i32  g_etc_dirfd                = DIRFD_NONE;
static i32  g_dev_dirfd                = DIRFD_NONE;
static c8   g_path_cache[PATH_BUF_MAX] = {0};
static c8  *g_path_dirs[MAX_PATH_DIRS];
static i32  g_path_dirfds[MAX_PATH_DIRS];
static i32  g_path_dir_n    = -1;
static bool g_capture_ready = false;

typedef struct {
  const c8 *name;
  i8        found;
  c8        path[CMD_PATH_MAX];
} ATTR_PACKED_ALIGNED(8) CmdSlot;

_Static_assert(sizeof(CmdSlot) == 256, "CmdSlot packed to 4 cache lines");
_Static_assert(alignof(CmdSlot) >= 8, "CmdSlot name pointer stays aligned in the table");

static CmdSlot g_cmds[MAX_CMDS];
static usize   g_cmd_n = 0;

static i32 open_dirfd(
  const c8 *path
) {
  return open(path, O_RDONLY | O_DIRECTORY | O_PATH | O_CLOEXEC);
}

static void init_path_cache(
  void
) {
  if (LIKELY(g_path_dir_n != -1)) return;

  const c8 *path_env = getenv("PATH");
  if (UNLIKELY(!path_env)) {
    g_path_dir_n = 0;
    return;
  }

  strncpy(g_path_cache, path_env, sizeof(g_path_cache) - 1);
  g_path_cache[sizeof(g_path_cache) - 1] = '\0';
  PREFETCH(g_path_cache + PREFETCH_DISTANCE);

  g_path_dir_n    = 0;
  usize pos       = 0;
  usize cache_len = strlen(g_path_cache);
  while (pos < cache_len && (usize)g_path_dir_n < MAX_PATH_DIRS) {
    while (pos < cache_len && g_path_cache[pos] == ':')
      pos++;
    if (pos >= cache_len) break;
    g_path_dirs[g_path_dir_n] = &g_path_cache[pos];
    while (pos < cache_len && g_path_cache[pos] != ':')
      pos++;
    if (pos < cache_len) g_path_cache[pos++] = '\0';
    g_path_dirfds[g_path_dir_n] = open_dirfd(g_path_dirs[g_path_dir_n]);
    g_path_dir_n++;
  }
}

static const c8 *resolve_locked(const c8 *name);

void capture_init(
  void
) {
  pthread_mutex_lock(&g_capture_mu);
  if (LIKELY(g_capture_ready)) {
    pthread_mutex_unlock(&g_capture_mu);
    return;
  }
  init_path_cache();
  if (g_proc_dirfd == DIRFD_NONE) g_proc_dirfd = open_dirfd("/proc");
  if (g_sys_dirfd == DIRFD_NONE) g_sys_dirfd = open_dirfd("/sys");
  if (g_etc_dirfd == DIRFD_NONE) g_etc_dirfd = open_dirfd("/etc");
  if (g_dev_dirfd == DIRFD_NONE) g_dev_dirfd = open_dirfd("/dev");
  static const c8 *const prefetch[] = {
    "lscpu",
    "lsblk",
    "findmnt",
    "ip",
    "uname",
    "lspci",
    "nvidia-smi",
    "inxi",
    "du",
  };
  for (usize idx = 0; idx < ARRAY_SIZE(prefetch); idx++)
    (void)resolve_locked(prefetch[idx]);
  g_capture_ready = true;
  pthread_mutex_unlock(&g_capture_mu);
}

void capture_fini(
  void
) {
  pthread_mutex_lock(&g_capture_mu);
  if (g_proc_dirfd >= 0) {
    close(g_proc_dirfd);
    g_proc_dirfd = DIRFD_NONE;
  }
  if (g_sys_dirfd >= 0) {
    close(g_sys_dirfd);
    g_sys_dirfd = DIRFD_NONE;
  }
  if (g_etc_dirfd >= 0) {
    close(g_etc_dirfd);
    g_etc_dirfd = DIRFD_NONE;
  }
  if (g_dev_dirfd >= 0) {
    close(g_dev_dirfd);
    g_dev_dirfd = DIRFD_NONE;
  }
  for (i32 idx = 0; idx < g_path_dir_n; idx++) {
    if (g_path_dirfds[idx] >= 0) close(g_path_dirfds[idx]);
    g_path_dirfds[idx] = DIRFD_NONE;
  }
  g_path_dir_n    = -1;
  g_capture_ready = false;
  g_cmd_n         = 0;
  pthread_mutex_unlock(&g_capture_mu);
}

static CmdSlot *cmd_slot(
  const c8 *name
) {
  for (usize idx = 0; idx < g_cmd_n; idx++) {
    if (strcmp(g_cmds[idx].name, name) == 0) return &g_cmds[idx];
  }
  if (g_cmd_n >= MAX_CMDS) return nullptr;
  CmdSlot *slot = &g_cmds[g_cmd_n++];
  slot->name    = name;
  slot->path[0] = '\0';
  slot->found   = -1;
  return slot;
}

static const c8 *resolve_locked(
  const c8 *name
) {
  CmdSlot *slot = cmd_slot(name);
  if (slot && slot->found == 1) return slot->path;
  if (slot && slot->found == 0) return nullptr;

  usize cmd_len = strlen(name);
  c8    full_path[PATH_BUF_MAX] __attribute__((aligned(FULL_PATH_ALIGNMENT)));

  for (i32 idx = 0; idx < g_path_dir_n; idx++) {
    if (UNLIKELY(idx + 1 < g_path_dir_n)) PREFETCH(g_path_dirs[idx + 1]);
    const c8 *dir     = g_path_dirs[idx];
    usize     dir_len = strlen(dir);
    if (UNLIKELY(dir_len + cmd_len + 2 > CMD_PATH_MAX)) continue;
    if (UNLIKELY(dir_len + cmd_len + 2 >= sizeof(full_path))) continue;

    i32 dirfd = g_path_dirfds[idx];
    if (dirfd >= 0) {
      if (faccessat(dirfd, name, X_OK, 0) != 0) continue;
    } else {
      memcpy(full_path, dir, dir_len);
      full_path[dir_len] = '/';
      memcpy(full_path + dir_len + 1, name, cmd_len + 1);
      if (access(full_path, X_OK) != 0) continue;
    }

    memcpy(full_path, dir, dir_len);
    full_path[dir_len] = '/';
    memcpy(full_path + dir_len + 1, name, cmd_len + 1);
    if (slot) {
      usize packed = dir_len + cmd_len + 2;
      COPY_MEMORY(slot->path, full_path, packed);
      slot->found = 1;
      return slot->path;
    }
    return nullptr;
  }
  if (slot) slot->found = 0;
  return nullptr;
}

const c8 *capture_resolve(
  const c8 *name
) {
  capture_init();
  if (UNLIKELY(!name || !name[0])) return nullptr;
  if (strchr(name, '/')) {
    if (access(name, X_OK) == 0) return name;
    return nullptr;
  }

  pthread_mutex_lock(&g_capture_mu);
  const c8 *path = resolve_locked(name);
  pthread_mutex_unlock(&g_capture_mu);
  return path;
}

bool capture_argv(
  Arena *arena, const c8 *const *argv, bool allow_failure, c8 **out
) {
  capture_init();
  if (UNLIKELY(!argv || !argv[0] || !out)) {
    if (out) *out = arena_copy_str(arena, "");
    return allow_failure;
  }

  const c8 *path = capture_resolve(argv[0]);
  if (!path) {
    *out = arena_copy_str(arena, "");
    if (allow_failure) return true;
    specs_die("could not run %s: %s", argv[0], strerror(ENOENT));
  }

  i32 pipefd[2];
  /* CLOEXEC: 8 gather threads posix_spawn at once. Without it, child A can
   * inherit child B's write end so B's read-loop never sees EOF. */
  if (pipe2(pipefd, O_CLOEXEC) != 0) {
    *out = arena_copy_str(arena, "");
    if (allow_failure) return true;
    specs_die("could not run %s: %s", argv[0], strerror(errno));
  }

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
  posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
  posix_spawn_file_actions_addclose(&actions, pipefd[0]);
  posix_spawn_file_actions_addclose(&actions, pipefd[1]);

  c8   *spawn_argv[16];
  usize argc = 0;
  while (argc < ARRAY_SIZE(spawn_argv) - 1 && argv[argc]) {
    spawn_argv[argc] = (c8 *)argv[argc];
    argc++;
  }
  spawn_argv[argc] = nullptr;

  pid_t child      = 0;
  i32   spawned    = posix_spawn(&child, path, &actions, nullptr, spawn_argv, environ);
  posix_spawn_file_actions_destroy(&actions);
  close(pipefd[1]);

  if (spawned != 0) {
    close(pipefd[0]);
    *out = arena_copy_str(arena, "");
    if (allow_failure) return true;
    specs_die("could not run %s: %s", argv[0], strerror(spawned));
  }

  Buf captured;
  buf_init(&captured, arena);
  c8 chunk[CAPTURE_CHUNK] __attribute__((aligned(64)));
  for (;;) {
    PREFETCH(chunk + PREFETCH_DISTANCE);
    ssize_t nread = read(pipefd[0], chunk, sizeof(chunk));
    if (nread > 0) {
      buf_append_n(&captured, chunk, (usize)nread);
      continue;
    }
    if (nread == 0) break;
    if (errno == EINTR) continue;
    break;
  }
  close(pipefd[0]);

  i32 status = 0;
  waitpid(child, &status, 0);
  if (status != 0 && !allow_failure) { specs_die("command failed: %s", argv[0]); }
  *out = captured.data;
  return true;
}

static bool path_is_prefix(
  const c8 *path, const c8 *prefix, usize prefix_len
) {
  return strncmp(path, prefix, prefix_len) == 0 && path[prefix_len] == '/';
}

c8 *capture_read_file(
  Arena *arena, const c8 *path
) {
  capture_init();
  if (UNLIKELY(!path || !path[0])) return arena_copy_str(arena, "");

  i32 fd = -1;
  if (path_is_prefix(path, "/proc", 5) && g_proc_dirfd >= 0) {
    fd = openat(g_proc_dirfd, path + 6, O_RDONLY | O_CLOEXEC);
  } else if (path_is_prefix(path, "/sys", 4) && g_sys_dirfd >= 0) {
    fd = openat(g_sys_dirfd, path + 5, O_RDONLY | O_CLOEXEC);
  } else if (path_is_prefix(path, "/etc", 4) && g_etc_dirfd >= 0) {
    fd = openat(g_etc_dirfd, path + 5, O_RDONLY | O_CLOEXEC);
  } else if (path_is_prefix(path, "/dev", 4) && g_dev_dirfd >= 0) {
    fd = openat(g_dev_dirfd, path + 5, O_RDONLY | O_CLOEXEC);
  }
  if (fd < 0) fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) return arena_copy_str(arena, "");

  Buf out;
  buf_init(&out, arena);
  c8 chunk[CAPTURE_CHUNK] __attribute__((aligned(64)));
  for (;;) {
    ssize_t nread = read(fd, chunk, sizeof(chunk));
    if (nread > 0) {
      buf_append_n(&out, chunk, (usize)nread);
      continue;
    }
    if (nread == 0) break;
    if (errno == EINTR) continue;
    break;
  }
  close(fd);
  return out.data;
}
