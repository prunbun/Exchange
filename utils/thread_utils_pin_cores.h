#pragma once 

/*
    CREDITS TO: yshen@hybridkernel.com, Binding Threads to Cores on OSX
    This code helps to pin threads to specific cores on OSX
*/

#define SYSCTL_CORE_COUNT   "machdep.cpu.core_count"

#include <iostream>
#include <cstring>
#include <atomic> // access to thread-safe variables
#include <unistd.h> // access to functions such as sleep()
#include <sys/syscall.h> // access to CPU flags
#include <mach/mach.h> // access to macOS kernel API

typedef struct cpu_set {
  uint32_t    count;
} cpu_set_t;

static inline void
CPU_ZERO(cpu_set_t *cs) { cs->count = 0; }

static inline void
CPU_SET(int num, cpu_set_t *cs) { cs->count |= (1 << num); }

static inline int
CPU_ISSET(int num, cpu_set_t *cs) { return (cs->count & (1 << num)); }

inline int pthread_setaffinity_np(
    pthread_t thread, 
    size_t cpu_size,
    cpu_set_t *cpu_set
) {
  thread_port_t mach_thread;
  int core = 0;

  for (core = 0; core < 8 * cpu_size; core++) {
    if (CPU_ISSET(core, cpu_set)) break;
  }
  printf("binding to core %d\n", core);
  thread_affinity_policy_data_t policy = { core };
  mach_thread = pthread_mach_thread_np(thread);
  thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY,
                    (thread_policy_t)&policy, 1);
  return 0;
}