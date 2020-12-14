all: create_folder base ecc array_sz sync_period

create_folder:
	mkdir -p  bin/sensitivity
base: sender receiver 
ecc: sender_ECC receiver_ECC
array_sz: sender_arraysz_4X receiver_arraysz_4X  sender_arraysz_2X receiver_arraysz_2X \
		  sender_arraysz_1X receiver_arraysz_1X
sync_period: sender_sync_25000 receiver_sync_25000   sender_sync_50000  receiver_sync_50000 \
		     sender_sync_100000 receiver_sync_100000 sender_sync_500000 receiver_sync_500000
clean:
	rm -rf bin/*.o ; rm -rf bin/sensitivity/*.o
clean_results:
	rm -rf results/*/*.txt results/*/*.log;

#-------------------------
# DEFINES
#-------------------------
CC=g++
CFLAGS=-ggdb -std=c++0x -O0 -g -pthread
DEFINES=-DRANDOM_PAYLOAD -DPROGRESS_HEARTBEAT -DFR_BARRIER_SYNC
DEFINES_ECC=-DECC

#-------------------------
# BASE ATTACK (Figure-9, Table-2 in paper)
#-------------------------
sender: src/fr_util.hh src/sender.cc
	$(CC) $(CFLAGS) $(DEFINES) src/sender.cc src/fec_secded7264.cc -o bin/sender.o
receiver: src/fr_util.hh src/receiver.cc
	$(CC) $(CFLAGS) $(DEFINES) src/receiver.cc src/fec_secded7264.cc -o bin/receiver.o

#-------------------------
# ATTACK WITH ECC (Table-3 in paper)
#-------------------------
sender_ECC: src/fr_util.hh src/sender.cc
	$(CC) $(CFLAGS) $(DEFINES) $(DEFINES_ECC) src/sender.cc src/fec_secded7264.cc -o bin/sender_ECC.o
receiver_ECC: src/fr_util.hh src/receiver.cc
	$(CC) $(CFLAGS) $(DEFINES) $(DEFINES_ECC) src/receiver.cc src/fec_secded7264.cc -o bin/receiver_ECC.o

#------------------------
# SENSITIVITY TO VARYING SHARED-ARRAY SIZE (Table-4 in paper)
#------------------------
#Default Array Size is 64MB (8X of Default LLC-Size of 8MB)
#Array Size 4X (32MB)
sender_arraysz_4X: src/fr_util.hh src/sender.cc
	$(CC) $(CFLAGS) $(DEFINES) -DARRAYSZ_PER_CACHESZ=4 src/sender.cc src/fec_secded7264.cc -o bin/sensitivity/sender_arraysz_4X.o
receiver_arraysz_4X: src/fr_util.hh src/receiver.cc
	$(CC) $(CFLAGS) $(DEFINES) -DARRAYSZ_PER_CACHESZ=4 src/receiver.cc src/fec_secded7264.cc -o bin/sensitivity/receiver_arraysz_4X.o
#Array Size 2X (16MB)
sender_arraysz_2X: src/fr_util.hh src/sender.cc
	$(CC) $(CFLAGS) $(DEFINES) -DARRAYSZ_PER_CACHESZ=2 src/sender.cc src/fec_secded7264.cc -o bin/sensitivity/sender_arraysz_2X.o
receiver_arraysz_2X: src/fr_util.hh src/receiver.cc
	$(CC) $(CFLAGS) $(DEFINES) -DARRAYSZ_PER_CACHESZ=2 src/receiver.cc src/fec_secded7264.cc -o bin/sensitivity/receiver_arraysz_2X.o
#Array Size 1X (8MB)
sender_arraysz_1X: src/fr_util.hh src/sender.cc
	$(CC) $(CFLAGS) $(DEFINES) -DARRAYSZ_PER_CACHESZ=1 src/sender.cc src/fec_secded7264.cc -o bin/sensitivity/sender_arraysz_1X.o
receiver_arraysz_1X: src/fr_util.hh src/receiver.cc
	$(CC) $(CFLAGS) $(DEFINES) -DARRAYSZ_PER_CACHESZ=1 src/receiver.cc src/fec_secded7264.cc -o bin/sensitivity/receiver_arraysz_1X.o

#------------------------
# SENSITIVITY TO VARYING SYNCHRONIZATION-PERIOD (Table-5 in paper)
#------------------------
#Default: Sync Every 200,000 bits
#Sync Every 25,000 bits
sender_sync_25000: src/fr_util.hh src/sender.cc
	$(CC) $(CFLAGS) $(DEFINES) -DSYNC_FREQ_SENSITIVITY=25000 src/sender.cc src/fec_secded7264.cc -o bin/sensitivity/sender_sync_25000.o
receiver_sync_25000: src/fr_util.hh src/receiver.cc
	$(CC) $(CFLAGS) $(DEFINES) -DSYNC_FREQ_SENSITIVITY=25000 src/receiver.cc src/fec_secded7264.cc -o bin/sensitivity/receiver_sync_25000.o
#Sync Every 50,000 bits
sender_sync_50000: src/fr_util.hh src/sender.cc
	$(CC) $(CFLAGS) $(DEFINES) -DSYNC_FREQ_SENSITIVITY=50000 src/sender.cc src/fec_secded7264.cc -o bin/sensitivity/sender_sync_50000.o
receiver_sync_50000: src/fr_util.hh src/receiver.cc
	$(CC) $(CFLAGS) $(DEFINES) -DSYNC_FREQ_SENSITIVITY=50000 src/receiver.cc src/fec_secded7264.cc -o bin/sensitivity/receiver_sync_50000.o
#Sync Every 100,000 bits
sender_sync_100000: src/fr_util.hh src/sender.cc
	$(CC) $(CFLAGS) $(DEFINES) -DSYNC_FREQ_SENSITIVITY=100000 src/sender.cc src/fec_secded7264.cc -o bin/sensitivity/sender_sync_100000.o
receiver_sync_100000: src/fr_util.hh src/receiver.cc
	$(CC) $(CFLAGS) $(DEFINES) -DSYNC_FREQ_SENSITIVITY=100000 src/receiver.cc src/fec_secded7264.cc -o bin/sensitivity/receiver_sync_100000.o
#Sync Every 500,000 bits
sender_sync_500000: src/fr_util.hh src/sender.cc
	$(CC) $(CFLAGS) $(DEFINES) -DSYNC_FREQ_SENSITIVITY=500000 src/sender.cc src/fec_secded7264.cc -o bin/sensitivity/sender_sync_500000.o
receiver_sync_500000: src/fr_util.hh src/receiver.cc
	$(CC) $(CFLAGS) $(DEFINES) -DSYNC_FREQ_SENSITIVITY=500000 src/receiver.cc src/fec_secded7264.cc -o bin/sensitivity/receiver_sync_500000.o
