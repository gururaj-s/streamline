/* Initial Handshake using Flush+Reload between Sender & Receiver Adapted from "https://github.com/yshalabi/covert-channel-tutorial".
   Subsequent Streamline Attack based on code written by Gururaj Saileshwar, Georgia Tech.
   Copyright (C) 2020, Gururaj Saileshwar
*/

#include "utils.hh" //Header for Streamline defines.
#include "fr_util.hh" //Header for Flush+Reload Handshake. (from "https://github.com/yshalabi/covert-channel-tutorial")

/* 
 * Function to send 0/1 via Flush+Reload channel to Receiver (for Initial Handshake)
 * For a predefined clock length,
 * - Sends a bit 1 to the receiver by repeatedly flushing the address
 * - Sends a bit 0 by doing nothing
 */
void send_bit_init_FR(bool one, struct config *config, int interval){
  
  // Synchronize with receiver
  CYCLES start_t = cc_sync(config->CHANNEL_SYNC_TIMEMASK, config->CHANNEL_SYNC_JITTER);
  
  if (one) {
    // Repeatedly flush addr
    ADDR_PTR addr = config->addr;
    while ((get_time() - start_t) < interval) {
      clflush(addr);
    }	

  } else {
    // Do Nothing
    while (get_time() - start_t < interval) {}
  }
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
// Gap Between TX and RX At Sync 
#define TX_SYNC_LAG_DELTA (5000)
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
uint64_t* SHARED_ARRAY ;  //Shared array used for communication  [**TODO**: Assign addresses from shared_file.txt ]
uint64_t SHARED_SEED = 42; // Starting point in the array.

//Cores that Tx and Rx will be pinned to.
#define tx_cpuid 1
#define rx_cpuid 0 

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
    // Initialize config and local variables
    struct config config;
    init_config(&config,NUM_BITS, argc, argv);

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
    printf("Streamline Covert Channel Sender with Num_Bits:%llu, %s,\
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
    rx_time_obs =  (uint64_t*)malloc(NUM_BITS_DEBUG_DTSTR*sizeof(uint64_t));
    tx_time_obs_timestamp = (uint64_t*)malloc(NUM_BITS_DEBUG_DTSTR*sizeof(uint64_t));
    rx_time_obs_timestamp = (uint64_t*)malloc(NUM_BITS_DEBUG_DTSTR*sizeof(uint64_t));
    //Bits to be transferred
    tx_payload = (bool*)malloc(TRANSMITTED_BITS*sizeof(bool));;
    rx_payload = (bool*)malloc(TRANSMITTED_BITS*sizeof(bool));

    //Initialize the data-structures used for transmission with random data
    srand(42);
    for(uint64_t i=0;i<TRANSMITTED_BITS;i++){
      tx_time_obs[i%NUM_BITS_DEBUG_DTSTR] = rand();
      rx_time_obs[i%NUM_BITS_DEBUG_DTSTR] = rand();
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
           1.0*NUM_BITS_DEBUG_DTSTR*sizeof(uint64_t)/1024/1024,1.0*TRANSMITTED_BITS*sizeof(bool)/1024/1024,1.0*TRANSMITTED_BITS*sizeof(bool)/1024/1024);


    //Set Core Affinity and Scheduler Parameters
    cpu_set_t mask;
    int status;
    CPU_ZERO(&mask);
    CPU_SET(tx_cpuid, &mask);
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
    printf("Sender Process - PID:%llu, TID:%lu, CPU:%d\n",getpid(), syscall(__NR_gettid) ,sched_getcpu());
    display_thread_sched_attr();
    fail_if_pthrattr_mismatch(SCHED_FIFO,sched_get_priority_max(SCHED_FIFO),tx_cpuid) ;
  
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

    //Local Private Array for Communication
    uint64_t TX_PRIVATE_ARRAY[PAGE_SZ] = {1} ; 
  
    //Flush SHARED_ARRAY
    for(uint64_t i=0;i<SHARED_ARRAY_NUMENTRIES; i++){
      _mm_clflush(&SHARED_ARRAY[i]);
    }

      
    //----------- Initial Syncronization --------------
    // Send a '10101011' bit sequence to tell the receiver
    // streamline is going to start
    for (int i = 0; i < 6; i++) {
      send_bit_init_FR(i % 2 == 0, &config,config.sync_interval);
    }
    send_bit_init_FR(true, &config,config.sync_interval);
    send_bit_init_FR(true, &config,config.sync_interval);

    // Initial Sync Done
    printf("Sender Ready: Done Initial Sync\n");


    // ----------- START STREAMLINE --------------------

    //Start Tx
    uint64_t bit_id = 0;
    unsigned int junk_temp_tx = 0;

    for(bit_id=0; bit_id<TRANSMITTED_BITS; bit_id++){

      //tx each iteration.
      int curr_payload = tx_payload[bit_id];
      //Get array index to communicate by getting curr_bitid -> curr_arrindex)
      uint64_t curr_bitid = SHARED_SEED + bit_id ;
      uint64_t curr_arrindex = (BITID_2_ARRINDEX(curr_bitid))%SHARED_ARRAY_NUMENTRIES + 4;

      //Based on Payload value (0/1), Mask = 0x0000000.. if Payload=0, or 0xFFFFFFF.. if Payload=1
      uint64_t payload_mask_0 = 0 - ((uint64_t)curr_payload & (uint64_t)1);
      uint64_t payload_mask_1 = ~payload_mask_0;
    
      //access corresponding array index
      uintptr_t addr_1 = (uintptr_t) &SHARED_ARRAY[curr_arrindex] & (uintptr_t)payload_mask_1; //will be 0x00 if Payload=1
      uintptr_t addr_0 = (uintptr_t) &TX_PRIVATE_ARRAY[0]         & (uintptr_t)payload_mask_0; //will be 0x00 if Payload=0
      volatile uint64_t* addr = (uint64_t*) (addr_1 | addr_0);

      unsigned int junk = 0;
      register uint64_t time0, delta_time0;

      time0 = __rdtscp( & junk); /* READ TIMER */
      uint64_t temp = *addr;

#if TX_ACCESS_LAG == 0     
      delta_time0 = __rdtscp( & junk) - time0; /* READ TIMER & COMPUTE ELAPSED TIME */
#endif
    
#if TX_ACCESS_LAG
      //Repeat Access to older line (N-behind)    
      int lag_bit_id = bit_id - TX_ACCESS_LAG_DELTA;
      if(lag_bit_id >0){
        int prev_payload = tx_payload[lag_bit_id];
        //Get array index to communicate by getting curr_bitid -> curr_arrindex)
        uint64_t prev_bitid = SHARED_SEED + lag_bit_id ;
        uint64_t prev_arrindex = (BITID_2_ARRINDEX(prev_bitid))%SHARED_ARRAY_NUMENTRIES + 4;

        //Based on Payload value (0/1), Mask = 0x0000000.. if Payload=0, or 0xFFFFFFF.. if Payload=1
        uint64_t prev_payload_mask_0 = 0 - ((uint64_t)prev_payload & (uint64_t)1);
        uint64_t prev_payload_mask_1 = ~prev_payload_mask_0;
    
        //access corresponding array index
        uintptr_t prev_addr_1 = (uintptr_t) &SHARED_ARRAY[prev_arrindex] & (uintptr_t)prev_payload_mask_1; //will be 0x00 if Payload=1
        uintptr_t prev_addr_0 = (uintptr_t) &TX_PRIVATE_ARRAY[0]         & (uintptr_t)prev_payload_mask_0; //will be 0x00 if Payload=0
        volatile uint64_t* prev_addr = (uint64_t*) (prev_addr_1 | prev_addr_0);

        //unsigned int junk = 0;
        //register uint64_t time0, delta_time0;
        //time0 = __rdtscp( & junk); /* READ TIMER */
      
        temp = *prev_addr;      
        //delta_time0 = __rdtscp( & junk) - time0; /* READ TIMER & COMPUTE ELAPSED TIME */
      }
#endif    

#ifdef PROGRESS_HEARTBEAT
      if( (bit_id % HEARTBEAT_FREQ) == (HEARTBEAT_FREQ - 1) ){
        uint64_t epoch_timestamp  = __rdtscp( & junk_temp_tx);
        tx_epoch_timestamp.push_back(epoch_timestamp);
        //printf("Tx-Epoch Curr-BitID:%d,Time-Epoch:%llu\n",bit_id,epoch_timestamp);
      }
#endif

#if VERBOSE
      //Print the index and addr accessed.
      printf("Tx: Curr-BitID:%d, Payload is:%d, Addr accessed:%#18x. Time=%d\n",curr_bitid,curr_payload,addr,delta_time0);
#endif
    
      tx_time_obs[bit_id%NUM_BITS_DEBUG_DTSTR] = delta_time0;
      tx_time_obs_timestamp[bit_id%NUM_BITS_DEBUG_DTSTR] = time0;

    
      //------- Synchronization every TX_SYNC_BITFREQ -------
      if( (bit_id % (TX_SYNC_BITFREQ)) == (TX_SYNC_BITFREQ - 1) ){

#ifdef DELAY_SYNC
        //1. Static Delays (400k cycles every 20k)
        uint64_t sleep_cycles = (TX_DELAY_CYCLES);
        delayloop(sleep_cycles);
#endif

#ifdef FR_BARRIER_SYNC
        //TX_SYNC
        //3. Flush-Reload based Synchronization
        volatile uint64_t* sync_rxready_addr =  (&sync_rxready_page[0]);
        volatile uint64_t* sync_rxready_addr1 =  (&sync_rxready_page1[0]);
        volatile uint64_t* sync_rxready_addr2 =  (&sync_rxready_page2[0]);

        volatile uint64_t* sync_txready_addr =  (&sync_txready_page[0]);
        volatile uint64_t* sync_txready_addr1 =  (&sync_txready_page1[0]);
        volatile uint64_t* sync_txready_addr2 =  (&sync_txready_page2[0]);
      
        unsigned int sync_junk = 0;
        uint64_t sync_temp = 0,sync_temp1=0,sync_temp2=0;
        register uint64_t sync_time0, sync_delta_time0,sync_time1, sync_delta_time1,sync_time2, sync_delta_time2;

        register uint64_t txsync_reached_donetime,txsync_complete_donetime;
      
        /* printf("%llu. \t Bit_Id:%d, Flush-Reload Sync Starts for Tx.\n",__rdtsc(), bit_id); */

        txsync_reached_donetime =  __rdtscp( &sync_junk);

        bool sync_complete = false;
        int llc_hit_count = 0;
        while(!sync_complete){
        
          //a. Flush addr where Rx will communicate it has reached barrier (sync_rxready_addr)
          _mm_clflush(&sync_rxready_page[0]);
          _mm_clflush(&sync_rxready_page1[0]);
          _mm_clflush(&sync_rxready_page2[0]);
        
          //z. Communicate that Tx has reached barrier
          sync_temp += *sync_txready_addr;
          sync_temp += *sync_txready_addr1;
          sync_temp += *sync_txready_addr2;

          //b. Sleep
          delayloop(1000);
        
          //c. Reload and Check latency of sync_rxready_addr
          sync_time0 = __rdtscp( &sync_junk); /* READ TIMER */
          sync_temp = *sync_rxready_addr;
          sync_delta_time0 = __rdtscp(&sync_junk) - sync_time0; /* READ TIMER & COMPUTE ELAPSED TIME */
        
          sync_time1 = __rdtscp( &sync_junk); /* READ TIMER */
          sync_temp1 = *sync_rxready_addr1;
          sync_delta_time1 = __rdtscp(&sync_junk) - sync_time1; /* READ TIMER & COMPUTE ELAPSED TIME */
        
          sync_time2 = __rdtscp( &sync_junk); /* READ TIMER */
          sync_temp2 = *sync_rxready_addr2;
          sync_delta_time2 = __rdtscp(&sync_junk) - sync_time2; /* READ TIMER & COMPUTE ELAPSED TIME */

          //d. Synchronization Condition
          //if rx_ready has 2 LLC hit, Rx reached barrier
          if(sync_delta_time0<LLC_HIT_THRESHOLD_CYCLES_SYNC)
            llc_hit_count++;
          if(sync_delta_time1<LLC_HIT_THRESHOLD_CYCLES_SYNC)
            llc_hit_count++;        
          if(sync_delta_time2<LLC_HIT_THRESHOLD_CYCLES_SYNC)
            llc_hit_count++;        
        
          if(llc_hit_count >= 2) 
            sync_complete=true;

        }
        txsync_complete_donetime =  __rdtscp( &sync_junk);

      
        txsync_reached_timevec.push_back(txsync_reached_donetime);
        txsync_complete_timevec.push_back(txsync_complete_donetime);
      
#endif
      }            
    }

    printf("Sender finished\n");
    return 0;
}











