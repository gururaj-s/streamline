// Copyright (C) 2020, Gururaj Saileshwar


#define _GNU_SOURCE 1
#include <sched.h>

#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/types.h>
#include <sys/syscall.h>
#include <errno.h> 
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <vector>
#include <pthread.h>
#include <getopt.h>
#include <immintrin.h>

#ifdef _MSC_VER
#include <intrin.h> /* for rdtscp and clflush */
#pragma optimize("gt",on)
#else
#include <x86intrin.h> /* for rdtscp and clflush */
#endif


//General Defines
#define CACHE_SZ (8*1024*1024)
#define PAGE_SZ (4*1024)
#define CACHELINE_SZ (64)
#define CL_IN_PAGE (PAGE_SZ/CACHELINE_SZ)
#define ARRAY_SZ (10*CACHE_SZ)

//Maximum number of accesses to make in the sequence being tested.
#define NUM_ACCESSES_TEST (1000)

     //Data=structures used for load-latency measurement
uint8_t* CACHE_FILL_ARRAY;
uint64_t* time_obs;


int main(int argc, char** argv){

  //Allocate page-aligned data-structures.
  CACHE_FILL_ARRAY = (uint8_t*) aligned_alloc(PAGE_SZ, ARRAY_SZ);
  time_obs = (uint64_t*) aligned_alloc(PAGE_SZ, 2*NUM_ACCESSES_TEST*sizeof(uint64_t));

  //Make sure number of accesses less than size of the array
  assert(NUM_ACCESSES_TEST*PAGE_SZ <= ARRAY_SZ);

  
  //Initialize array
  srand(42);
  for(int i=0;i<ARRAY_SZ; i++){
    CACHE_FILL_ARRAY[i] = (uint8_t) (rand()%255);
  }
  for(int i=0;i<NUM_ACCESSES_TEST;i++)
    time_obs[i] = 0;
  
  //Flush array
  for(int i=0;i<ARRAY_SZ; i++){
    _mm_clflush(&CACHE_FILL_ARRAY[i]);
  }

  //initialize timer variables
  unsigned int junk = 0;
  uint64_t time0=0, delta_time0=0;
  int num_accesses = 0;
  uint8_t temp = 0;

  //BEGIN MISSES
  for (int j=0; j< NUM_ACCESSES_TEST;j++){
    //Generate address
    volatile uint8_t* addr = &(CACHE_FILL_ARRAY[j*PAGE_SZ]);
    //Measure load-start
    time0 = __rdtscp(&junk); /* READ TIMER */    
    //Load
    temp += *addr;
    //Measure load-end
    delta_time0 = __rdtscp (&junk) - time0; /* READ TIMER & COMPUTE ELAPSED TIME */
    time_obs[j] = delta_time0;
    //count accesses
    num_accesses++;    
  }  
  
  //BEGIN HITS
  //Install in LLC/L2/L1
  for (int j=0; j< NUM_ACCESSES_TEST;j++){
    volatile uint8_t* addr = &(CACHE_FILL_ARRAY[j*PAGE_SZ]);
    temp += *addr;
  }
  for (int j=0; j< NUM_ACCESSES_TEST;j++){
    volatile uint8_t* addr = &(CACHE_FILL_ARRAY[j*PAGE_SZ]);
    temp += *addr;
  }
  //Begin Measuring Hits
  for (int j=0; j< NUM_ACCESSES_TEST;j++){
    volatile uint8_t* addr = &(CACHE_FILL_ARRAY[j*PAGE_SZ]);
    //Measure load-start
    time0 = __rdtscp(&junk); /* READ TIMER */
    //Load
    temp += *addr;
    //Measure load-end
    delta_time0 = __rdtscp (&junk) - time0; /* READ TIMER & COMPUTE ELAPSED TIME */
    time_obs[j+NUM_ACCESSES_TEST] = delta_time0;
    //count accesses
    num_accesses++;    
  }

  //Print statistics of latency of Hits & Misses : average and min/max
  uint64_t sum_hit = 0;
  uint64_t min_hit = 999999, max_hit = 0;
  printf("\n ##### HITS ######\n");
  for(int i=NUM_ACCESSES_TEST;i<2*NUM_ACCESSES_TEST;i++){    
    printf("%lu, ",time_obs[i]);
    sum_hit += time_obs[i];
    if(min_hit > time_obs[i]){
      min_hit = time_obs[i];
    }
    if(max_hit < time_obs[i])
      max_hit = time_obs[i];       
  }
  uint64_t avg_hit = (uint64_t)(1.0*sum_hit/(num_accesses/2));
  printf("\n");

  uint64_t sum_miss = 0;
  uint64_t min_miss = 999999, max_miss = 0;
  printf("\n ##### MISSES ######\n");
  for(int i=0;i<NUM_ACCESSES_TEST;i++){    
    printf("%lu, ",time_obs[i]);
    sum_miss += time_obs[i];
    if(min_miss > time_obs[i])
      min_miss = time_obs[i];
    if(max_miss < time_obs[i])
      max_miss = time_obs[i];    
  }
  uint64_t avg_miss = (uint64_t)(1.0*sum_miss/(num_accesses/2));
  printf("\n\n");

  printf("HITS -> Avg: %lu . Min: %lu . Max: %lu \n",avg_hit,min_hit,max_hit);
  printf("MISS -> Avg: %lu . Min: %lu . Max: %lu \n",avg_miss,min_miss,max_miss);
  printf("Suggested LLC_MISS_THRESHOLD_CYCLES -> %lu \n",min_miss);
  
}
