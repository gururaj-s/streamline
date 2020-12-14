/* Initial Handshake of Sender & Receiver Adapted from Flush=Reload Code at "https://github.com/yshalabi/covert-channel-tutorial". */
/*
 */

#ifndef FR_UTIL_H_
#define FR_UTIL_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "utils.hh"

// ------ Variable Definitions  ----------

#define ADDR_PTR uint64_t
#define CYCLES uint32_t

#define CHANNEL_DEFAULT_INTERVAL            0x00008000
#define CHANNEL_SYNC_TIMEMASK_DEF           0x000FFFFF
#define CHANNEL_SYNC_JITTER_DEF             0x0100

#define DEFAULT_FILE_NAME ((char*) SHARED_READONLY_FILE_PATH)
#define DEFAULT_FILE_OFFSET	0x0
#define DEFAULT_FILE_SIZE	((uint64_t)(SHARED_ARRAY_SZ + 1024*1024))
#define CACHE_BLOCK_SIZE	64


struct config {
  ADDR_PTR addr;
  int sync_interval;
  int comm_interval;
  int CHANNEL_SYNC_JITTER;
  int CHANNEL_SYNC_TIMEMASK;
};

// ------ Function Definitions  ----------

CYCLES measure_one_block_access_time(ADDR_PTR addr)
{
    CYCLES cycles;

    asm volatile("mov %1, %%r8\n\t"
            "lfence\n\t"
            "rdtsc\n\t"
            "mov %%eax, %%edi\n\t"
            "mov (%%r8), %%r8\n\t"
            "lfence\n\t"
            "rdtsc\n\t"
            "sub %%edi, %%eax\n\t"
    : "=a"(cycles) /*output*/
    : "r"(addr)
    : "r8", "edi");

    return cycles;
}

/* 
 * Returns Time Stamp Counter 
 */
inline __attribute__((always_inline))
CYCLES rdtscp(void) {
	CYCLES cycles;
	asm volatile ("rdtscp"
	: /* outputs */ "=a" (cycles));

	return cycles;
}

/* 
 * Gets the value Time Stamp Counter 
 */
inline CYCLES get_time() {
    return rdtscp();
}

/* Synchronizes at the overflow of a counter
 *
 * Counter is created by masking the lower bits of the Time Stamp Counter
 * Sync done by spinning until the counter is less than CHANNEL_SYNC_JITTER
 */
inline __attribute__((always_inline))
CYCLES cc_sync(int CHANNEL_SYNC_TIMEMASK, int CHANNEL_SYNC_JITTER) {
  while((get_time() & CHANNEL_SYNC_TIMEMASK) > CHANNEL_SYNC_JITTER) {}
  return get_time();
}


/*
 * Flushes the cache block accessed by a virtual address out of the cache
 */
inline __attribute__((always_inline))
void clflush(ADDR_PTR addr)
{
    asm volatile ("clflush (%0)"::"r"(addr));
}


/*
 * Prints help menu
 */
void print_help() {
  printf("-f,\tFile to be shared between sender/receiver\n"
         "-o,\tSelected offset into shared file\n"
         "-i,\tTime interval for sending a single bit\n");
}

/*
 * Parses the arguments and flags of the program and initializes the struct config
 * with those parameters (or the default ones if no custom flags are given).
 */
void init_config(struct config *config, uint64_t& NUM_BITS,  int argc, char **argv)
{
	// Initialize default config parameters
	int offset = DEFAULT_FILE_OFFSET;
	config->sync_interval = CHANNEL_DEFAULT_INTERVAL;
    config->comm_interval = CHANNEL_DEFAULT_INTERVAL;
    config->CHANNEL_SYNC_TIMEMASK = CHANNEL_SYNC_TIMEMASK_DEF;
    config->CHANNEL_SYNC_JITTER = CHANNEL_SYNC_JITTER_DEF;
    
    char *filename = DEFAULT_FILE_NAME;

    
	// Parse the command line flags
	//      -f is used to specify the shared file 
	//      -i is used to specify the sending interval rate
	//      -o is used to specify the shared file offset
    //      -n is used to specify number of bits to transmit.
	int option;
	while ((option = getopt(argc, argv, "i:s:o:f:n:")) != -1) {
      switch (option) {
      case 'i':
        config->sync_interval = atoi(optarg);
        break;
      case 's':
        config->CHANNEL_SYNC_TIMEMASK = atoi(optarg);
        break;        
      case 'o':
        offset = atoi(optarg)*CACHE_BLOCK_SIZE;
        break;
      case 'f':
        filename = optarg;
        break;
      case 'n':
        NUM_BITS = strtol(optarg,NULL,10);
        break;        
      case 'h':
        print_help();
        exit(1);
      case '?':
        fprintf(stderr, "Unknown option character\n");
        print_help();
        exit(1);
      default:
        print_help();
        exit(1);
      }
	}

	// Map file to virtual memory and extract the address at the file offset
	if (filename != NULL) {
		int inFile = open(filename, O_RDONLY);
		if(inFile == -1) {
			printf("Failed to Open File\n");
			exit(1);
		}

		void *mapaddr = mmap(NULL,DEFAULT_FILE_SIZE,PROT_READ,MAP_SHARED,inFile,0);

		if (mapaddr == (void*) -1 ) {
			printf("Failed to Map Address\n");
			exit(1);
		}

		config->addr = (ADDR_PTR) mapaddr + offset;
	}
}

#endif
