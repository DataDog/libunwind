/* End-to-end test of local unw_step on AArch64 frames with bad unwind data.

   A worker thread spins in the leaf of each scenario (frames in
   aarch64-test-step-bad-frames-asm.S). A signal handler on that thread
   calls unw_init_local2 with UNW_INIT_SIGNAL_FRAME and calls unw_step
   until it returns <= 0, as a sampling profiler does. Each scenario runs
   in a forked child, so a crash in the unwinder fails only that scenario.  */

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct
{
  _Atomic uint32_t spinning;
  _Atomic uint32_t release;
} spin_control_t;
_Static_assert (offsetof (spin_control_t, release) == 4, "layout used by asm");

/* Unmapped stale x29 value seen in a customer crash.  */
#define POISON_FP ((uintptr_t) 0xffffff80a2419420ULL)

/* Generous, because CI runs this test under qemu-user.  */
#define WAIT_SECONDS 60

extern void poisoned_frame (spin_control_t *, uintptr_t);
extern void fp_based_frame (spin_control_t *, uintptr_t);
extern void sane_frame (spin_control_t *, uintptr_t);
extern void bad_x29_loc_frame (spin_control_t *, uintptr_t);

extern char spin_without_cfi[], spin_without_cfi_end[];
extern char gcc_style_frame_return[], poisoned_frame_return[];
extern char stale_fp_slot[], stale_fp_slot_end[], fp_based_frame_return[];
extern char sane_frame_return[];
extern char bad_x29_loc_leaf[], bad_x29_loc_leaf_end[];

#define MAX_FRAMES 64

static struct
{
  int init;
  int n;
  uintptr_t ip[MAX_FRAMES];
  int step[MAX_FRAMES];
} g;
static _Atomic uint32_t g_done;

static void
unwind_handler (int sig, siginfo_t *si, void *ctx)
{
  unw_cursor_t cursor;

  g.init = unw_init_local2 (&cursor, (unw_context_t *) ctx, UNW_INIT_SIGNAL_FRAME);
  while (g.init == 0 && g.n < MAX_FRAMES)
    {
      unw_word_t ip = 0;
      unw_cursor_snapshot_t snap;
      int i;

      if (unw_get_reg (&cursor, UNW_REG_IP, &ip) != 0 || ip == 0)
        break;
      i = g.n++;
      g.ip[i] = ip;
      g.step[i] = unw_step (&cursor);
      if (g.step[i] <= 0)
        break;
      memset (&snap, 0, sizeof snap);
      unw_get_cursor_snapshot (&cursor, &snap);
      if (snap.step_method == UNW_STEP_FALLBACK_LR_NO_PROC_INFO)
        break;
    }
  atomic_store (&g_done, 1);
}

struct scenario
{
  const char *name;
  void (*outer) (spin_control_t *, uintptr_t);
};

static const struct scenario scenarios[] = {
  { "A poisoned_frame -> gcc_style_frame -> spin_without_cfi", poisoned_frame },
  { "B fp_based_frame -> stale_fp_slot", fp_based_frame },
  { "C sane_frame -> spin_with_cfi", sane_frame },
  { "D bad_x29_loc_frame -> bad_x29_loc_leaf", bad_x29_loc_frame },
};

struct worker_arg
{
  const struct scenario *s;
  spin_control_t *ctl;
};

static void *
worker (void *p)
{
  struct worker_arg *a = p;
  a->s->outer (a->ctl, POISON_FP);
  return NULL;
}

static int
wait_for (_Atomic uint32_t *flag)
{
  struct timespec start, now;

  clock_gettime (CLOCK_MONOTONIC, &start);
  while (atomic_load (flag) == 0)
    {
      clock_gettime (CLOCK_MONOTONIC, &now);
      if (now.tv_sec - start.tv_sec > WAIT_SECONDS)
        return -1;
      usleep (1000);
    }
  return 0;
}

static int
in_range (uintptr_t ip, const char *lo, const char *hi)
{
  return ip >= (uintptr_t) lo && ip < (uintptr_t) hi;
}

static int
check (int scenario)
{
  int last = g.n - 1;
  int i;

  if (g.init != 0 || g.n == 0)
    return 0;
  switch (scenario)
    {
    case 0:
      return g.n == 3
             && in_range (g.ip[0], spin_without_cfi, spin_without_cfi_end)
             && g.ip[1] == (uintptr_t) gcc_style_frame_return
             && g.ip[2] == (uintptr_t) poisoned_frame_return
             && g.step[2] < 0;
    case 1:
      return g.n == 2
             && in_range (g.ip[0], stale_fp_slot, stale_fp_slot_end)
             && g.ip[1] == (uintptr_t) fp_based_frame_return
             && g.step[1] < 0;
    case 2:
      for (i = 0; i < last; i++)
        if (g.ip[i] == (uintptr_t) sane_frame_return)
          return g.step[last] == 0;
      return 0;
    case 3:
      return g.n == 1
             && in_range (g.ip[0], bad_x29_loc_leaf, bad_x29_loc_leaf_end)
             && g.step[0] < 0;
    }
  return 0;
}

static int
run_scenario (int scenario)
{
  struct rlimit no_core = { 0, 0 };
  struct sigaction sa;
  spin_control_t ctl;
  struct worker_arg arg = { &scenarios[scenario], &ctl };
  pthread_t th;
  int i, ok;

  setrlimit (RLIMIT_CORE, &no_core);

  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = unwind_handler;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset (&sa.sa_mask);
  sigaction (SIGUSR2, &sa, NULL);

  atomic_store (&ctl.spinning, 0);
  atomic_store (&ctl.release, 0);
  if (pthread_create (&th, NULL, worker, &arg) != 0)
    {
      printf ("    pthread_create failed\n");
      return 1;
    }
  if (wait_for (&ctl.spinning) != 0)
    {
      printf ("    worker did not reach its leaf\n");
      return 1;
    }
  pthread_kill (th, SIGUSR2);
  if (wait_for (&g_done) != 0)
    {
      printf ("    unwind did not finish\n");
      return 1;
    }
  atomic_store (&ctl.release, 1);
  pthread_join (th, NULL);

  printf ("    unw_init_local2 = %d\n", g.init);
  for (i = 0; i < g.n; i++)
    printf ("    #%d ip=0x%lx unw_step = %d\n", i, (unsigned long) g.ip[i], g.step[i]);
  ok = check (scenario);
  printf ("    %s\n", ok ? "PASS" : "FAIL: unexpected frames");
  fflush (stdout);
  return ok ? 0 : 1;
}

int
main (void)
{
  int failures = 0;
  size_t s;

  for (s = 0; s < sizeof scenarios / sizeof scenarios[0]; s++)
    {
      pid_t pid;
      int status = 0;

      printf ("[%s]\n", scenarios[s].name);
      fflush (stdout);
      pid = fork ();
      if (pid == 0)
        _exit (run_scenario (s));
      if (pid < 0 || waitpid (pid, &status, 0) != pid)
        {
          printf ("    FAIL: fork/waitpid\n");
          failures++;
        }
      else if (WIFSIGNALED (status))
        {
          printf ("    FAIL: child killed by signal %d\n", WTERMSIG (status));
          failures++;
        }
      else if (!WIFEXITED (status) || WEXITSTATUS (status) != 0)
        failures++;
    }
  printf ("%d of %zu scenarios failed\n", failures, s);
  return failures ? 1 : 0;
}
