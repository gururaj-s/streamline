#ifndef __MASTIK_HH__
#define __MASTIK_HH__

/* 
 * This function is taken from Mastik: A Micro-Architectural Side-Channel Toolkit  (Yuval Yarom)
 * https://cs.adelaide.edu.au/~yval/Mastik/  
 */

void delayloop(uint32_t cycles) {
  unsigned int junk = 0;
  uint64_t start = __rdtscp(&junk);
  while ((__rdtscp(&junk) - start) < cycles)
    ;
}

#endif
// 
// mastik.hh ends here
