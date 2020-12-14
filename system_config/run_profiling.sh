#Build load-latency profiling tool
make all ;
#Run Tool.
./rdtscp.o > results.txt
#Print Results (use LLC_MISS_THRESHOLD from last line of results.txt)
cat results.txt
#Visualize the Distribution of L2-Hit vs LLC Miss Latency 
python plot_latency_dist.py
