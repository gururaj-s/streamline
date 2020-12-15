/* Initial Handshake using Flush+Reload between Sender & Receiver Adapted from "https://github.com/yshalabi/covert-channel-tutorial".
   Subsequent Streamline Attack based on code written by Gururaj Saileshwar, Georgia Tech. 
   Copyright (C) 2020, Gururaj Saileshwar
*/

#include "utils.hh" //Header for Streamline defines.
#include "fr_util.hh" //Header for Flush+Reload Handshake. (from "https://github.com/yshalabi/covert-channel-tutorial")

/* 
 * Receiver for Flush+Reload (Used for Initial Handshake with Sender)
 * Detects a bit by repeatedly measuring the access time of the load from config->addr
 * and counting the number of misses for the clock length of config->interval.
 *
 * Detect a bit 1 if misses >= hits
 * Detect a bit 0 otherwise
 */
bool detect_bit(struct config *config, int interval)
{
  int misses = 0;
  int hits = 0;

  // Sync with sender
  CYCLES start_t = cc_sync(config->CHANNEL_SYNC_TIMEMASK, config->CHANNEL_SYNC_JITTER);
  
  while ((get_time() - start_t) < interval) {
    // Load data from config->addr and measure latency
    CYCLES access_time = measure_one_block_access_time(config->addr); 

    // Ignore access times larger than 1000 cycles usually due to a disk miss.
    if (access_time < 1000) {
      // Count if it's a miss or hit depending on latency
      if (access_time > LLC_MISS_THRESHOLD_CYCLES) {
        misses++;
      } else {
        hits++;
      }
    }
  }
  return misses >= hits;
}


/* Variables for Streamline */
//Frequency to Print Progress Heartbeat
#define HEARTBEAT_FREQ (1000)

//-------- Access Pattern ----------
// Access every 3n cacheline alternating between page 0 and page 1, then continue to pages 2,3 .. 4,5 ... and so on.
#define PG_NUM(l) (((uint64_t)(((uint64_t)(l/2))*3/CL_IN_PAGE))*2 + l%2)
#define CL_NUM(l) ((((uint64_t)(l/2))*3 + 14) % CL_IN_PAGE)
#define BITID_2_ARRINDEX(l) ( PG_NUM(l)*ENTRY_PER_PAGE + CL_NUM(l)*ENTRY_PER_CL )

// Beating the LLC Replacement Policy (Access older lines)
#define TX_ACCESS_LAG_DELTA (5000)
#define TX_ACCESS_LAG       (1)

//-------- Synchronization ----------
// Rx Delay Per Sync Iteration
#define RX_SYNC_SLEEP  (1000)
// Frequency of Sync 
#ifndef SYNC_FREQ_SENSITIVITY
#define TX_SYNC_BITFREQ (200000)
#else
#define TX_SYNC_BITFREQ (SYNC_FREQ_SENSITIVITY)
#endif
// Gap Between TX and RX At Syncronization
#define TX_SYNC_LAG_DELTA (5000) /*cross-core */
// Timeout after which Rx exits sync
#define RX_SYNC_TIMEOUT (5*100*5000)

//Initial Synchronization
uint64_t RX_DELAY_CYCLES = 250000;

// Types of Ongoing Synchronization:
//1. Static-Delay based synchronization.
#define TX_DELAY_CYCLES (400000)

//2. Flush+Reload based synchronization
uint64_t* sync_rxready_page; //to be a page-size allocated page-aligned.
uint64_t* sync_rxready_page1; //to be a page-size allocated page-aligned.
uint64_t* sync_rxready_page2; //to be a page-size allocated page-aligned.

uint64_t* sync_txready_page; //to be a page-size allocated page-aligned.
uint64_t* sync_txready_page1; //to be a page-size allocated page-aligned.
uint64_t* sync_txready_page2; //to be a page-size allocated page-aligned.


// -------- Network Parameters  -------------
// LFSR Based Channel Encoding to modulate payloads (to allow pathological payloads)
std::tr1::mt19937 mt (42); //Mersenne Twister PRNG engine
std::tr1::uniform_int<int> channel_enc(0, 1); //uniform distribution [0,1]

// Error-Correction Parameters: (72,64) Hamming Code
#define DATABLK_BITLEN (64)
#define PARITY_BITLEN (8)


// -------- Transmission Parameters  -------------

//Threshold for LLC-Hit
uint64_t LLC_HIT_THRESHOLD_CYCLES_SYNC = LLC_MISS_THRESHOLD_CYCLES;
uint64_t LLC_HIT_THRESHOLD_CYCLES_COMM = LLC_MISS_THRESHOLD_CYCLES;

//Number of Bits to be transmitted.
uint64_t NUM_BITS = 1; //Number of bits to be transmitted
uint64_t NUM_BITS_DEBUG_DTSTR = NUM_BITS ; //Number of bits to be transmitted
uint64_t TRANSMITTED_BITS ; //Includes data and parity bits 

//Array used for communication
uint64_t* SHARED_ARRAY ;  //Shared array used for communication
uint64_t SHARED_SEED = 42; // Starting point in the array.

//Cores that Tx and Rx will be pinned to.
#define tx_cpuid 1 
#ifdef SAMECORE
#define rx_cpuid 5  /* same-core */
#else
#define rx_cpuid 0    /* cross-core */
#endif


//Transmission & Receiver Data-Structures
uint64_t* tx_time_obs;
uint64_t* rx_time_obs;
uint64_t* tx_time_obs_timestamp;
uint64_t* rx_time_obs_timestamp;

//Bits to be transferred
bool* tx_payload;
bool* rx_payload;

//Starting time-stamp
uint64_t tx_start_timestamp = 0,rx_start_timestamp = 0;

//Debugging Data-Structures
std::vector<uint64_t> tx_epoch_timestamp;
std::vector<uint64_t> rx_epoch_timestamp;
std::vector<uint64_t> rxsync_reached_timevec,rxsync_start_timevec,rxsync_complete_timevec;
std::vector<uint64_t> txsync_reached_timevec,txsync_complete_timevec;
std::vector<uint64_t> debug_rxsync_time, debug_timeout_duration, debug_timeout_bitid;


//Global variable for tx_start
bool tx_started = false;


int main(int argc, char **argv)
{

  //----------- Initialize Variables --------------
  // Get command-line parameters
  struct config config; //for initial sync
  init_config(&config, NUM_BITS, argc, argv);

  //Initialize the Number of Transmitted Bits
#ifdef ECC
  assert((NUM_BITS%8 == 0) && "Number of Bits has to be a Multiple of 8\n" );
  TRANSMITTED_BITS = NUM_BITS*(DATABLK_BITLEN+PARITY_BITLEN)/DATABLK_BITLEN;    
  #else
  TRANSMITTED_BITS = NUM_BITS;
  #endif
  if(NUM_BITS > NUM_BITS_DEBUG_MAX)
    NUM_BITS_DEBUG_DTSTR = NUM_BITS_DEBUG_MAX;
  else
    NUM_BITS_DEBUG_DTSTR = TRANSMITTED_BITS ;

  //Printing Inputs
  #ifdef  RANDOM_PAYLOAD  
  std::string payload_type = "Random Payload";
  #endif    
  #ifdef CONSTANT_PAYLOAD_0
  std::string payload_type = "Constant-0 Payload";
  #endif    
  #ifdef CONSTANT_PAYLOAD_1
  std::string payload_type = "Constant-1 Payload";
  #endif
  printf("Streamline Covert Channel Receiver with Num_Bits:%llu, %s,\
 LLC-Hit-Threshold-Comm:%llu cycles. LLC-Hit-Threshold-Sync:%llu cycles. \
 Start Tx-Rx-Delay-Cycles: %llu\n",
         NUM_BITS,payload_type.c_str(),\
         LLC_HIT_THRESHOLD_CYCLES_COMM,LLC_HIT_THRESHOLD_CYCLES_SYNC,RX_DELAY_CYCLES);


  //Data-structures used for transmission:
  //Shared array used for transmission
  SHARED_ARRAY =  (uint64_t*) (config.addr + OFFSET_SHARED_ARRAY);

  // Stream Through Shared Array
  uint64_t temp = 0;
  for(uint64_t i=0;i<SHARED_ARRAY_NUMENTRIES;i++){    
    temp+= SHARED_ARRAY[i];
  }

  //Shared page used for synchronization
  sync_rxready_page =  (uint64_t*) (config.addr + OFFSET_FR_SYNC_REG_RX1);
  sync_txready_page =  (uint64_t*) (config.addr + OFFSET_FR_SYNC_REG_TX1);
  sync_rxready_page1 = (uint64_t*) (config.addr + OFFSET_FR_SYNC_REG_RX2);
  sync_txready_page1 = (uint64_t*) (config.addr + OFFSET_FR_SYNC_REG_TX2);
  sync_rxready_page2 = (uint64_t*) (config.addr + OFFSET_FR_SYNC_REG_RX3);
  sync_txready_page2 = (uint64_t*) (config.addr + OFFSET_FR_SYNC_REG_TX3);

  // Flush shared page used for synchronization
  for(uint64_t i=0;i<PAGE_SZ/sizeof(uint64_t);i++){
    _mm_clflush(&sync_rxready_page[i]);
    _mm_clflush(&sync_rxready_page1[i]);
    _mm_clflush(&sync_rxready_page2[i]);

  }
  for(uint64_t i=0;i<PAGE_SZ/sizeof(uint64_t);i++){
    _mm_clflush(&sync_txready_page[i]);
    _mm_clflush(&sync_txready_page1[i]);
    _mm_clflush(&sync_txready_page2[i]);
  }   
  
  //Transmission & Receiver Data-Structures
  tx_time_obs = (uint64_t*)malloc(NUM_BITS_DEBUG_DTSTR*sizeof(uint64_t));
  rx_time_obs =  (uint64_t*)malloc(TRANSMITTED_BITS*sizeof(uint64_t));
  tx_time_obs_timestamp = (uint64_t*)malloc(NUM_BITS_DEBUG_DTSTR*sizeof(uint64_t));
  rx_time_obs_timestamp = (uint64_t*)malloc(NUM_BITS_DEBUG_DTSTR*sizeof(uint64_t));
  //Bits to be transferred
  tx_payload = (bool*)malloc(TRANSMITTED_BITS*sizeof(bool));;
  rx_payload = (bool*)malloc(TRANSMITTED_BITS*sizeof(bool));

  //Initialize the data-structures used for transmission with random data
  srand(42);
  for(uint64_t i=0;i<TRANSMITTED_BITS;i++){
    tx_time_obs[i%NUM_BITS_DEBUG_DTSTR] = rand();    
    rx_time_obs[i] = rand();
    tx_time_obs_timestamp[i%NUM_BITS_DEBUG_DTSTR] = rand(); 
    rx_time_obs_timestamp[i%NUM_BITS_DEBUG_DTSTR] = rand();
    tx_payload[i] = rand()%2;
    rx_payload[i] = rand()%2;
  }

  //Print Preliminaries.
  printf("Array Size: %llu bytes (%.2f GB). Starting index is: %llu (%lluth page)\n",
         SHARED_ARRAY_NUMENTRIES*sizeof(SHARED_ARRAY[0]), SHARED_ARRAY_NUMENTRIES*sizeof(SHARED_ARRAY[0])/(1024*1024*1024.0),\
         BITID_2_ARRINDEX(SHARED_SEED), SHARED_SEED);

  printf("Other Data-Structures Sizes: rx_time_obs:%2f MB, tx_payload:%.2f MB, rx_payload:%.2f MB.\n",
         1.0*TRANSMITTED_BITS*sizeof(uint64_t)/1024/1024,1.0*TRANSMITTED_BITS*sizeof(bool)/1024/1024,1.0*TRANSMITTED_BITS*sizeof(bool)/1024/1024);


  //Set Core Affinity and Scheduler Parameters
  cpu_set_t mask;
  int status;
  CPU_ZERO(&mask);
  CPU_SET(rx_cpuid, &mask);
  status = sched_setaffinity(0, sizeof(mask), &mask);
  if (status != 0) {
    perror("sched_setaffinity");
  }

  //Setting Scheduler Priority & Policy
  int policy;
  struct sched_param param;
  pthread_getschedparam(pthread_self(), &policy, &param);
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
  printf("Receiver Process - PID:%llu, TID:%lu, CPU:%d\n",getpid(), syscall(__NR_gettid) ,sched_getcpu());
  display_thread_sched_attr();
  fail_if_pthrattr_mismatch(SCHED_FIFO,sched_get_priority_max(SCHED_FIFO),rx_cpuid) ;

  // Create Tx Payload.
  srand(42);
  for(uint64_t i=0;i<TRANSMITTED_BITS;i++){
#ifdef  RANDOM_PAYLOAD
    //random payload:
    int cur_payload = rand()%2;
#endif
    
#ifdef CONSTANT_PAYLOAD_0    
    int cur_payload = 0;
#endif
    
#ifdef CONSTANT_PAYLOAD_1
    int cur_payload = 1;
#endif
 
    //tx each iteration.
    tx_payload[i] = cur_payload ;

    //Add error-correction
#ifdef ECC
    if(i% (DATABLK_BITLEN+PARITY_BITLEN) == (DATABLK_BITLEN-1)){
      //datablk bool array of 64 bits => datablk_bytes encoding 8 byte data
      bool* datablk = &tx_payload[i-DATABLK_BITLEN+1];
      /* print_bool_array(datablk,64); */
      uint8_t datablk_bytes[8], enc_datablk_bytes[9] ;       
      conv_char(datablk,8,datablk_bytes);
      
      //encode data
      int enc_bytelen = fec_secded7264_encode(8,datablk_bytes, enc_datablk_bytes);
      assert(enc_bytelen == 9);
      
      //enc_datablk_bytes of 9 bytes => enc_datablk
      bool* enc_datablk = &tx_payload[i-DATABLK_BITLEN+1];
      string_to_binary(enc_datablk_bytes,enc_bytelen,enc_datablk);
      /* print_bool_array(enc_datablk,72); */

      //increment the bit-id
      i+=PARITY_BITLEN;  
    }                                             
#endif
  }
  
  //Modulate Payload with Channel Encoding.
  uint64_t num_pkts = 0;
  for(uint64_t i=0;i<TRANSMITTED_BITS;i++){

    if(i%TX_SYNC_BITFREQ == 0)
      mt.seed(42); //Mersenne Twister PRNG engine

    int channel_enc_i = channel_enc(mt);
    tx_payload[i] =  tx_payload[i] ^ channel_enc_i;
  }

 
  
  //Initialize Initial Synchronization Variables
  int flip_sequence = 4;
  bool current;
  bool previous = true;  
  printf("Listening...\n");
  fflush(stdout);


  //----------- Initial Syncronization --------------
  // Detect the sequence '101011' that indicates
  // sender is starting streamline
  while (1) {
    current = detect_bit(&config,config.sync_interval);	
    if (flip_sequence == 0 && current == 1 && previous == 1) {
      //Detected Sender has finished initial FR sync
      break;
    } else if (flip_sequence > 0 && current != previous) {
      flip_sequence--;
    } else if (current == previous) {
      flip_sequence = 4;
    }    
    previous = current;
  }

  // Initial Sync Done
  printf("Receiver Ready: Done Initial Sync\n");

  
  // ----------- START STREAMLINE --------------------

  //Start Rx
  delayloop(RX_DELAY_CYCLES);
  unsigned int junk_temp_rx = 0;

  rx_start_timestamp = __rdtscp(&junk_temp_rx);
  uint64_t rx_loop_count = 0;
  register uint64_t rx_start_time,rx_end_time;
  unsigned int junk_temp=0;

  //Mark Start Time
  rx_start_time = __rdtscp( & junk_temp);
  uint64_t timestamp_rxstart_cycles = __rdtscp( & junk_temp_rx);

  //Start Receiver Loop
  for (uint64_t rx_id = 0; rx_id < TRANSMITTED_BITS; rx_id++){
    unsigned int junk = 0;
    register uint64_t time0, delta_time0;

    uint64_t curr_bitid = SHARED_SEED + rx_loop_count ;
    uint64_t curr_arrindex = (BITID_2_ARRINDEX(curr_bitid))%SHARED_ARRAY_NUMENTRIES + 4;
    volatile uint64_t* addr0 = &SHARED_ARRAY[curr_arrindex];

    //Get time for reading addr0
    time0 = __rdtscp( & junk); /* READ TIMER */
    junk = *addr0;
    delta_time0 = __rdtscp( & junk) - time0; /* READ TIMER & COMPUTE ELAPSED TIME */
    
    //delta_time0 = junk % 512;
    //delta_time0++;

    //Store Time0 and Time1 in obs_time_array_0[], obs_time_array_1[]
    rx_time_obs[rx_id] = delta_time0;
    rx_time_obs_timestamp[rx_id%NUM_BITS_DEBUG_DTSTR] = time0;
    
#ifdef PROGRESS_HEARTBEAT
    if( (rx_id % HEARTBEAT_FREQ) == (HEARTBEAT_FREQ - 1) ){
      uint64_t epoch_timestamp  = __rdtscp( & junk_temp_rx);
      rx_epoch_timestamp.push_back(epoch_timestamp);
      //printf("Rx-Epoch Curr-BitID:%d,Timestamp:%llu\n\n",rx_id,epoch_timestamp);
    }
#endif

    //--- Syncrhonization every TX_SYNC_BITFREQ -------
    if( (rx_id % (TX_SYNC_BITFREQ)) == (TX_SYNC_BITFREQ - TX_SYNC_LAG_DELTA) ){

#ifdef FR_BARRIER_SYNC
      //RX_SYNC 
      //3. Flush-Reload based Synchronization
      volatile uint64_t* sync_rxready_addr =  (&sync_rxready_page[0]);
      volatile uint64_t* sync_rxready_addr1 =  (&sync_rxready_page1[0]);
      volatile uint64_t* sync_rxready_addr2 =  (&sync_rxready_page2[0]);

      unsigned int sync_junk = 0, sync_junk2 = 0;
      register uint64_t sync_time0, sync_delta_time0, sync_time0_2, sync_delta_time0_2;
      register uint64_t sync_time1, sync_delta_time1, sync_time2, sync_delta_time2;
      register uint64_t sync_time0_21, sync_delta_time0_21, sync_time0_22, sync_delta_time0_22;

      volatile uint64_t* sync_txready_addr =  (&sync_txready_page[0]);
      volatile uint64_t* sync_txready_addr1 =  (&sync_txready_page1[0]);
      volatile uint64_t* sync_txready_addr2 =  (&sync_txready_page2[0]);
      
      bool sync_start = false;
      int tx_ready_count = 0;
      
      bool sync_complete = false;
      int llc_hit_count = 0;
      uint64_t sync_temp = 0,sync_temp1 = 0,sync_temp2 = 0 ;

      register uint64_t rxsync_reached_donetime, rxsync_start_donetime, rxsync_complete_donetime;
      
      /* printf("%llu. \t Bit_Id:%d, Flush-Reload Sync Starts for Rx.\n",__rdtsc(), rx_id); */
      rxsync_reached_donetime =  __rdtscp( &sync_junk);

      //Check if Tx has reached barrier.
      uint64_t sleep_count = 0;    
      while(!(sync_start)){
        //a. Flush addr where Tx will communicate (sync_txready_addr)
        _mm_clflush(&sync_txready_page[0]);
        _mm_clflush(&sync_txready_page1[0]);
        _mm_clflush(&sync_txready_page2[0]);

        //b. Sleep
        delayloop(RX_SYNC_SLEEP);

        //c Reload and Check latency of sync_txready_addr
        sync_time0_2 = __rdtscp( &sync_junk2); /* READ TIMER */
        sync_temp += *sync_txready_addr;
        sync_delta_time0_2 = __rdtscp(&sync_junk) - sync_time0_2; /* READ TIMER & COMPUTE ELAPSED TIME */

        sync_time0_21 = __rdtscp( &sync_junk2); /* READ TIMER */
        sync_temp1 += *sync_txready_addr1;
        sync_delta_time0_21 = __rdtscp(&sync_junk) - sync_time0_21; /* READ TIMER & COMPUTE ELAPSED TIME */

        sync_time0_22 = __rdtscp( &sync_junk2); /* READ TIMER */
        sync_temp2 += *sync_txready_addr2;
        sync_delta_time0_22 = __rdtscp(&sync_junk) - sync_time0_22; /* READ TIMER & COMPUTE ELAPSED TIME */

        //If tx_ready has 2 LLC-Hit, means Tx has reached barrier.
        if(sync_delta_time0_2 <LLC_HIT_THRESHOLD_CYCLES_SYNC)
          tx_ready_count++;
        if(sync_delta_time0_21 <LLC_HIT_THRESHOLD_CYCLES_SYNC)
          tx_ready_count++;
        if(sync_delta_time0_22 <LLC_HIT_THRESHOLD_CYCLES_SYNC)
          tx_ready_count++;

        if(tx_ready_count >=2)
          sync_start = true;

#ifdef RX_SYNC_TIMEOUT      
        sleep_count++;

        if(sleep_count*RX_SYNC_SLEEP > RX_SYNC_TIMEOUT){
          sync_start = true;
          sync_complete = true;

          debug_rxsync_time.push_back(__rdtscp( &sync_junk) - rxsync_reached_donetime);
          debug_timeout_duration.push_back(RX_SYNC_TIMEOUT);
          debug_timeout_bitid.push_back(rx_id);
        }
        

#endif
      }

      rxsync_start_donetime =  __rdtscp( &sync_junk);
      
      //Speculation Barrier.
      int a,b;
      __asm__("cpuid"
              :"=a"(b)                 // EAX into b (output)
              :"0"(a)                  // a into EAX (input)
              :"%ebx","%ecx","%edx");  // clobbered registers

      //Communicate that Rx has reached barrier.
      while(!(sync_complete)){

        //a. Load sync_rxready_addr (to communicate Rx has reached barrier)
        sync_time0 = __rdtscp( &sync_junk); /* READ TIMER */
        sync_temp += *sync_rxready_addr;
        sync_delta_time0 = __rdtscp(&sync_junk) - sync_time0; /* READ TIMER & COMPUTE ELAPSED TIME */

        sync_time1 = __rdtscp( &sync_junk); /* READ TIMER */
        sync_temp1 += *sync_rxready_addr1;
        sync_delta_time1 = __rdtscp(&sync_junk) - sync_time1; /* READ TIMER & COMPUTE ELAPSED TIME */

        sync_time2 = __rdtscp( &sync_junk); /* READ TIMER */
        sync_temp2 += *sync_rxready_addr2;
        sync_delta_time2 = __rdtscp(&sync_junk) - sync_time2; /* READ TIMER & COMPUTE ELAPSED TIME */

        //Continue until 3/3 or 4/5 loads are LLC-Hits (Flush stops from Tx-side, so Tx has exited the barrier)
        /* llc_hit_count = (sync_delta_time0 <LLC_HIT_THRESHOLD_CYCLES_SYNC)?llc_hit_count+1:llc_hit_count-1 ; */
        /* if(llc_hit_count < 0) */
        /*   llc_hit_count = 0; */
        
        /* if(llc_hit_count == 3) */
        /*   sync_complete=true; */

        // Continue until 3/3 or 4/5 loads are LLC-Hits (Flush stops from Tx-side, so Tx has exited the barrier)
        if(sync_delta_time0<LLC_HIT_THRESHOLD_CYCLES_SYNC)
          llc_hit_count++;
        if(sync_delta_time1<LLC_HIT_THRESHOLD_CYCLES_SYNC)
          llc_hit_count++;        
        if(sync_delta_time2<LLC_HIT_THRESHOLD_CYCLES_SYNC)
          llc_hit_count++;        
        
        if(llc_hit_count >= 30) 
          sync_complete=true;
      }

      rxsync_complete_donetime =  __rdtscp( &sync_junk);
      /* printf("%llu. \t Bit_Id:%d, Flush-Reload Sync Ends for Rx. Rx-Delta:%llu\n",__rdtsc(), rx_id,sync_delta_time0); */

      rxsync_reached_timevec.push_back(rxsync_reached_donetime);
      rxsync_start_timevec.push_back(rxsync_start_donetime);
      rxsync_complete_timevec.push_back(rxsync_complete_donetime);      
#endif      
    }

#if VERBOSE
    printf("Rx: Curr-BitID:%d, Tx-Bit is:%d, Addr accessed:%#18x, PG_NUM:%d, CL_NUM:%d Time:%d, Rx-Bit:%d,%s\n",curr_bitid,tx_payload[rx_id],addr0,PG_NUM(curr_bitid),CL_NUM(curr_bitid),delta_time0,delta_time0>LLC_HIT_THRESHOLD_CYCLES_COMM?1:0,
           (delta_time0>LLC_HIT_THRESHOLD_CYCLES_COMM?1:0) == tx_payload[rx_id]?"":"Error");
#endif

    rx_loop_count++;
  }
 
  //Mark End Time
  rx_end_time = __rdtscp( & junk_temp);
  //Done Rx
  printf("Receiving Done\n");

  //------ Construct Rx-Payload -----------
  for(uint64_t i=0; i < rx_loop_count; i++){
    if(rx_time_obs[i] > LLC_HIT_THRESHOLD_CYCLES_COMM)
      rx_payload[i] = 1; //miss = 1
    else
      rx_payload[i] = 0; //hit = 0
  }

  //------ Error-Correction and Analysis --------
  //Calculate Bit Period.
  long bit_period_cycles = (rx_end_time - rx_start_time)/rx_loop_count*1.0; //cycles
  double freq_mhz = SYS_FREQ_MHZ; 
  double bit_period_us = (1.0*bit_period_cycles/freq_mhz);

  //Calculate the error-rate:
  uint64_t total_ones = 0;
  uint64_t total_samples = 0;
  uint64_t correct_samples = 0;
  uint64_t one2zero_error = 0;
  uint64_t zero2one_error = 0;
 
  uint64_t zero_bit_error_blks =0;  
  uint64_t one_bit_error_blks =0;
  uint64_t twoplus_bit_error_blks =0;
  uint64_t tot_blks =0;
  int bit_errors_in_blk =0;
  uint64_t bit_id =0, tx_samples=0,tx_correct_samples=0;

  uint64_t data_pkts = NUM_BITS/DATABLK_BITLEN;
  uint64_t packet_sz ;

#ifdef ECC
  packet_sz = DATABLK_BITLEN + PARITY_BITLEN;
#else
  packet_sz = DATABLK_BITLEN;
#endif
 
  for(uint64_t i=0;i< data_pkts;i++){
    if(i*packet_sz >= rx_loop_count)
      break;

    bool tx_packet_enc_bits[72], rx_packet_enc_bits[72];
    bool tx_packet_dec_bits[64], rx_packet_dec_bits[64];
    uint8_t tx_packet_enc_bytes[9], rx_packet_enc_bytes[9]; 
    uint8_t tx_packet_dec_bytes[8], rx_packet_dec_bytes[8];  

    //De-modulate Payload with Channel Encoding.
    for(int j=0;j<packet_sz;j++){
      if(bit_id%TX_SYNC_BITFREQ == 0)
        mt.seed(42); //Mersenne Twister PRNG engine
     
      int channel_enc_i = channel_enc(mt);
      tx_packet_enc_bits[j] = tx_payload[bit_id] ^ channel_enc_i;
      rx_packet_enc_bits[j] = rx_payload[bit_id] ^ channel_enc_i;
    
      if(tx_packet_enc_bits[j] == rx_packet_enc_bits[j])
        tx_correct_samples++;
      else
        tx_payload[bit_id]?one2zero_error++:zero2one_error++;

      if(tx_payload[bit_id])
        total_ones++;

      bit_id++;
    }
    tx_samples = bit_id;
   
    //Perform ECC-Decoding 
#ifdef ECC
    unsigned int errors;
    //convert bits to bytes
    conv_char(tx_packet_enc_bits,9,tx_packet_enc_bytes);
    conv_char(rx_packet_enc_bits,9,rx_packet_enc_bytes);
   
    //decode
    fec_secded7264_decode(9, tx_packet_enc_bytes, tx_packet_dec_bytes, &errors);
    fec_secded7264_decode(9, rx_packet_enc_bytes, rx_packet_dec_bytes, &errors);

    //convert bytes to bits
    string_to_binary(tx_packet_dec_bytes,8,tx_packet_dec_bits);
    string_to_binary(rx_packet_dec_bytes,8,rx_packet_dec_bits);   
#endif

    //Check for Errors
    for(int j=0; j<DATABLK_BITLEN;j++){  
#ifdef ECC
      int tx_act = tx_packet_dec_bits[j];
      int rx_act = rx_packet_dec_bits[j];
      int tx_transmitted = tx_packet_dec_bits[j];
#else
      int tx_act = tx_packet_enc_bits[j];
      int rx_act = rx_packet_enc_bits[j];
#endif
      if(tx_act == rx_act)
        correct_samples++;
      else{
        bit_errors_in_blk++;
      }
      total_samples++;
    }
   
    //Calculate type of error, at end of blk.
    if(bit_errors_in_blk == 0)
      zero_bit_error_blks++;
    else if (bit_errors_in_blk == 1)
      one_bit_error_blks++;
    else if (bit_errors_in_blk > 1)
      twoplus_bit_error_blks++;

    //reset bit_errors
    tot_blks++;
    bit_errors_in_blk = 0;
  }

  //Print Output
  printf("\n-----------------------------\n");
  printf("Bit Period: %llu cycles or %.4fus. Bits/Sec: %.4f bps.\
 FinalCorrectSamples=%.2f\% (%llu/%llu). Tx1_to_0_errors=%.2f\%, Tx0_to_1_errors=%.2f\% . Total-1s-Perc=%.2f\%\n",\
         bit_period_cycles,bit_period_us,                               \
         (1.0*DATABLK_BITLEN/packet_sz)*1000000.0/bit_period_us,100.0*correct_samples/total_samples,correct_samples,total_samples,\
         100.0*one2zero_error/tx_samples,100.0*zero2one_error/tx_samples,100.0*total_ones/tx_samples);
  
  printf("Packet-ErrorType: NoError, \t 1-Bit Error,\t >=2-Bit Errors: \t %.2f%%  \t %.2f%% \
 \t %.2f%% (%llu,%llu,%llu)/%llu. Bit-Error-Perc:(1-Bit,2+): %.2f%% \t %.2f%% \n",
         100.0*zero_bit_error_blks/tot_blks,100.0*one_bit_error_blks/tot_blks, \
         100.0*twoplus_bit_error_blks/tot_blks,zero_bit_error_blks,one_bit_error_blks,twoplus_bit_error_blks,tot_blks,
         100.0*one_bit_error_blks/total_samples,100-100.0*correct_samples/total_samples-100.0*one_bit_error_blks/total_samples);

  printf("Transmission Error Rates: TxCorrectRate=%.2f\% (%llu/%llu).\
 Tx1to0_errors=%.2f\%, Tx0to1_errors=%.2f\%\n",\
         100.0*tx_correct_samples/tx_samples,tx_correct_samples,tx_samples,\
         100.0*one2zero_error/tx_samples,100.0*zero2one_error/tx_samples);
  printf("-----------------------------\n\n");

#ifndef ECC
#ifdef PROGRESS_HEARTBEAT

  // Post-Processing Debugging Output for Epochs where Receiver Timeout used
  std::vector<uint64_t> rxsync_epoch_miss;
  uint64_t rxsync_miss = 0;

  for (int i=0;i<NUM_BITS/TX_SYNC_BITFREQ; i++){
   
    uint64_t rxsync_time = rxsync_complete_timevec[i] - rxsync_reached_timevec[i];
   
    if(rxsync_time > 0.9 * RX_SYNC_TIMEOUT ){
      rxsync_epoch_miss.push_back(1);
      rxsync_miss++;
    }
    else
      rxsync_epoch_miss.push_back(0);
  }
 

  //For each epoch, print the delta_Tx-Rx,error-rate.
  printf("BitID(in1000s), \t Correct-Tx-rate, \t 1->0.Error, \t 0->1.Error.\
 RX_SYNCREACH_TIME, RX_SYNCSTART_TIME, RX_SYNCCOMPLETE_TIME \n");

  uint64_t epoch_id=0;
  uint64_t epoch_correct_samples=0,epoch_num_samples=0;
  uint64_t epoch_one2zero_error = 0, epoch_zero2one_error = 0;

 
  for(uint64_t i=0; i<NUM_BITS; i++){
    //breaking condition.
    if(i >= rx_loop_count)
      break;

    // printf("%d\n",i);
    epoch_num_samples++;

    //Modulate Payload with Channel Encoding.
    if(i%TX_SYNC_BITFREQ == 0)
      mt.seed(42); //Mersenne Twister PRNG engine
     
    int channel_enc_i = channel_enc(mt);
    int tx_act = tx_payload[i] ^ channel_enc_i;
    int rx_act = rx_payload[i] ^ channel_enc_i;

    if(tx_act == rx_act)
      epoch_correct_samples++;
    else
      tx_payload[i]?epoch_one2zero_error++:epoch_zero2one_error++;

    //End of a epoch
    if( (i % HEARTBEAT_FREQ) == (HEARTBEAT_FREQ - 1) ) {

      if(epoch_id % (TX_SYNC_BITFREQ/HEARTBEAT_FREQ) == 0 ){
        int sync_id = i/TX_SYNC_BITFREQ;
        //BitID(in1000Bits), \t Correct-Tx-rate, \t 1->0.Error, \t 0->1.Error. \
        RX_SYNCREACH_TIME, RX_SYNCSTART_TIME, RX_SYNCCOMPLETE_TIME 
        printf("%llu \t %.2f\% \t %.2f\% \t %.2f\% \t %lld \t %lld \t %lld \t %d \n", \
               epoch_id,100.0*epoch_correct_samples/epoch_num_samples,
               100.0*epoch_one2zero_error/epoch_num_samples,100.0*epoch_zero2one_error/epoch_num_samples, \
               rxsync_reached_timevec[sync_id], \
               rxsync_start_timevec[sync_id]-rxsync_reached_timevec[sync_id],
               rxsync_complete_timevec[sync_id]-rxsync_reached_timevec[sync_id],rxsync_epoch_miss[sync_id]);
      }
      
      epoch_id++;
      epoch_correct_samples = 0;
      epoch_num_samples=0;
      epoch_zero2one_error =0;
      epoch_one2zero_error =0;
    }
    
  }

  printf("RXSync-Misses: %d in %llu bits\n",rxsync_miss,NUM_BITS);

  for(int j=0; j< debug_rxsync_time.size();j++){
    printf("Bit-ID:%llu, RX-Sync-Delay:%llu, Rx-Sync-Timeout:%llu\n",\
           debug_timeout_bitid[j],debug_rxsync_time[j],debug_timeout_duration[j]);
  }
 
#endif
#endif
 
  printf("Receiver finished\n");
  return 0;
}


