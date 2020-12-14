// Copyright (C) 2020, Gururaj Saileshwar

// Commentary: 
// Generic Header file with defines.
// 
// 
// 

#ifndef UTILS_H_
#define UTILS_H_

#define _GNU_SOURCE 1
#include <sched.h>

#include <string>
#include <stdio.h>
#include <cstdint>
#include <pthread.h>
#include <unistd.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h> 
#include <sys/types.h>
#include <sys/syscall.h>
#include <errno.h> 
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <tr1/random>

#include <vector>

/* 
 * Header Files
 */
#ifdef _MSC_VER
#include <intrin.h> /* for rdtscp and clflush */
#pragma optimize("gt",on)
#else
#include <x86intrin.h> /* for rdtscp and clflush */
#endif

#include "mastik.hh" /* for a helper function: delayloop(cycles)  */
#include "bits_util.hh" /* for converting string to bits. */
#include "fec_secded7264.hh"  /* for ECC (Hamming Codes 72,64) */

#define VERBOSE (0)

/* 
 * Other Defines
 */

//SYSTEM-SPECIFIC DEFINES
#define CACHE_SZ (8*1024*1024)
#define SYS_FREQ_MHZ (3900)
#define LLC_MISS_THRESHOLD_CYCLES (180)
#define SHARED_READONLY_FILE_PATH ("/home/gururaj/Documents/Research/streamline/PUBLIC_ASPLOS21_REPO/streamline_two/shared_readonly_file.txt")

// Other Sizes
#define PAGE_SZ (4*1024)
#define CACHELINE_SZ (64)
#define CL_IN_PAGE (PAGE_SZ/CACHELINE_SZ)
#define ARRENTRY_SZ (8)
#ifndef ARRAYSZ_PER_CACHESZ
#define SHARED_ARRAY_SZ ((uint64_t)((2*CACHE_SZ*4)))
#else
#define SHARED_ARRAY_SZ ((uint64_t)((ARRAYSZ_PER_CACHESZ*CACHE_SZ)))
#endif
#define SHARED_ARRAY_NUMENTRIES ((uint64_t)(SHARED_ARRAY_SZ/ARRENTRY_SZ))
#define ENTRY_PER_PAGE (PAGE_SZ/ARRENTRY_SZ)
#define ENTRY_PER_CL (CACHELINE_SZ/ARRENTRY_SZ)
// Offsets for Addresses in Shared File (used for communication)
#define OFFSET_FR_SYNC_INIT (0)
#define OFFSET_FR_SYNC_REG_RX1 (1*4096)
#define OFFSET_FR_SYNC_REG_TX1 (2*4096)
#define OFFSET_FR_SYNC_REG_RX2 (3*4096)
#define OFFSET_FR_SYNC_REG_TX2 (4*4096)
#define OFFSET_FR_SYNC_REG_RX3 (5*4096)
#define OFFSET_FR_SYNC_REG_TX3 (6*4096)
#define OFFSET_SHARED_ARRAY    (7*4096)
// For restricting size of debugging data-structures
#define NUM_BITS_DEBUG_MAX (1000000)


/* 
 * Function to Print Scheduling Policy Adopted for current Thread.
 */
static int display_thread_sched_attr(void)
{
  int policy, s;
  struct sched_param param;

  s = pthread_getschedparam(pthread_self(), &policy, &param);
  if (s != 0) { printf("pthread_getschedparam"); return 1; }

  printf("policy=%s, priority=%d\n",
         (policy == SCHED_FIFO)  ? "SCHED_FIFO" : (policy == SCHED_RR)    ? "SCHED_RR" : (policy == SCHED_OTHER) ? "SCHED_OTHER" : "???",
         param.sched_priority);

  return 0;
}

/* 
 * Function to Check if Scheduling Policy Correctly Applied
 */
static int fail_if_pthrattr_mismatch(int check_policy, int check_prio, int cpu)
{
  int policy, s;
  struct sched_param param;

  s = pthread_getschedparam(pthread_self(), &policy, &param);
  if (s != 0) { printf("Error:pthread_getschedparam\n"); exit(1); }
  if (policy != check_policy) {printf("Error:sched_policy mismatch\n"); exit(1); }
  if (param.sched_priority != check_prio) {printf("Error:sched_priority mismatch\n"); exit(1); }
  if (sched_getcpu() != cpu) {printf("Error:sched_priority mismatch\n"); exit(1); }

  return 0;
}



#endif

// 
// utils.hh ends here
