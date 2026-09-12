#define _GNU_SOURCE

#include "arena.h"
#include "capture.h"
#include "test_framework.h"
#include "test_suites.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CAP_THREADS 8
#define CAP_ARENA   ((usize)(32u << 10))

typedef struct {
  u8    mem[CAP_ARENA];
  Arena arena;
  c8   *out;
  bool  ok;
} CapWorker;

static void *capture_uname_worker(
  void *arg
) {
  CapWorker *job    = arg;
  const c8  *argv[] = {"uname", "-r", nullptr};
  c8        *out    = nullptr;
  job->ok           = capture_argv(&job->arena, argv, true, &out);
  job->out          = out;
  return nullptr;
}

static void *resolve_uname_worker(
  void *arg
) {
  const c8 **slot = arg;
  *slot           = capture_resolve("uname");
  return nullptr;
}

static bool timed_join(
  pthread_t thread, i32 timeout_sec
) {
  struct timespec deadline;
  if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) return pthread_join(thread, nullptr) == 0;
  deadline.tv_sec += timeout_sec;
  return pthread_timedjoin_np(thread, nullptr, &deadline) == 0;
}

static bool source_has_pipe2_cloexec(
  void
) {
  FILE *src = fopen("src/capture.c", "r");
  if (!src) return false;
  c8   line[512];
  bool found = false;
  while (fgets(line, (i32)sizeof(line), src)) {
    if (strstr(line, "pipe2") && strstr(line, "O_CLOEXEC")) {
      found = true;
      break;
    }
  }
  fclose(src);
  return found;
}

void run_capture_tests(
  void
) {
  TEST_PRINTLN("capture");

  TEST("pipe2 O_CLOEXEC in shipped capture.c", source_has_pipe2_cloexec());

  capture_init();

  const c8 *uname_path = capture_resolve("uname");
  TEST("resolve uname", uname_path != nullptr && uname_path[0] == '/');

  const c8 *again = capture_resolve("uname");
  TEST("resolve uname stable pointer", again == uname_path);

  u8    serial_mem[CAP_ARENA];
  Arena serial_arena;
  arena_init(&serial_arena, serial_mem, sizeof(serial_mem));
  const c8 *serial_argv[] = {"uname", "-r", nullptr};
  c8       *serial_out    = nullptr;
  TEST("serial capture_argv uname", capture_argv(&serial_arena, serial_argv, true, &serial_out) && serial_out && serial_out[0]);

  const c8 *resolved[CAP_THREADS];
  pthread_t resolvers[CAP_THREADS];
  bool      resolve_live[CAP_THREADS] = {0};
  ZERO_MEMORY(resolved, sizeof(resolved));
  for (usize idx = 0; idx < CAP_THREADS; idx++) {
    if (pthread_create(&resolvers[idx], nullptr, resolve_uname_worker, &resolved[idx]) == 0)
      resolve_live[idx] = true;
    else
      resolved[idx] = capture_resolve("uname");
  }
  bool resolve_joined = true;
  for (usize idx = 0; idx < CAP_THREADS; idx++) {
    if (resolve_live[idx] && !timed_join(resolvers[idx], 5)) resolve_joined = false;
  }
  TEST("concurrent resolve joined", resolve_joined);
  bool resolve_same = true;
  for (usize idx = 0; idx < CAP_THREADS; idx++) {
    if (resolved[idx] != uname_path) resolve_same = false;
  }
  TEST("concurrent resolve same path", resolve_same);

  CapWorker workers[CAP_THREADS];
  pthread_t threads[CAP_THREADS];
  bool      live[CAP_THREADS] = {0};
  ZERO_MEMORY(workers, sizeof(workers));
  for (usize idx = 0; idx < CAP_THREADS; idx++) {
    arena_init(&workers[idx].arena, workers[idx].mem, sizeof(workers[idx].mem));
    if (pthread_create(&threads[idx], nullptr, capture_uname_worker, &workers[idx]) == 0)
      live[idx] = true;
    else
      capture_uname_worker(&workers[idx]);
  }
  bool joined = true;
  for (usize idx = 0; idx < CAP_THREADS; idx++) {
    if (live[idx] && !timed_join(threads[idx], 5)) joined = false;
  }
  TEST("concurrent capture_argv joined (no pipe hang)", joined);

  bool all_ok = joined;
  for (usize idx = 0; idx < CAP_THREADS; idx++) {
    if (!workers[idx].ok || !workers[idx].out || !workers[idx].out[0]) all_ok = false;
    if (serial_out && workers[idx].out && strcmp(workers[idx].out, serial_out) != 0) all_ok = false;
  }
  TEST("concurrent capture_argv matches serial uname -r", all_ok);

  capture_fini();
  TEST_SUMMARY("capture");
}
