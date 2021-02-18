# Streamline Covert Channel Attack (ASPLOS'21)

### Citation  
Gururaj Saileshwar, Christopher Fletcher and Moinuddin Qureshi. **Streamline: A Fast, Flushless Cache Covert-Channel Attack by Enabling Asynchronous Collusion**. In _Proceedings of the 26th International Conference on Architectural Support for Programming Languages and Operating Systems, ASPLOS 2021_.

### Hardware Requirements
* Intel CPU from Haswell or newer generation
    * You can check your CPU model with `cat /proc/cpuinfo | grep "model name"` and learn which generation it is by looking up the model on [Wikichip](http://wikichip.org) or [Intel's Repository](https://ark.intel.com).
* Native execution
* Sudo priviliges (to enable THP and set the CPU frequency to a constant value)  
  
### Software Requirements
* Command-line tool *cpupower* :
    * On Fedora, this can typically be installed with `sudo dnf install kernel-tools`
    * On Ubuntu, this can typically be installed with `sudo apt-get install linux-tools-<KERNEL-VERSION>-generic`
* Python3, Jupyter Notebook and Python3 Packages (pandas, matplotlib, seaborn) for Plotting & Visualizing results:
    * Jupyter Notebook can be installed using the instructions at the following links: using [anaconda](https://jupyter.readthedocs.io/en/latest/install/notebook-classic.html) or using [virtualenv](https://www.digitalocean.com/community/tutorials/how-to-set-up-jupyter-notebook-with-python-3-on-ubuntu-18-04) (on Ubuntu).
    * Python3 packages can be installed using the following links (e.g. using anaconda): [pandas](https://pandas.pydata.org/pandas-docs/stable/getting_started/install.html), [matplotlib](https://matplotlib.org/3.3.3/users/installing.html), [seaborn](https://seaborn.pydata.org/installing.html). 
    
### Steps to Run (and reproduce Fig-9, Tables-2,3,4,5 from the paper)

**1. Enable Transparent Huge Pages (THP):**
   - On Fedora and Ubuntu: Checking the status of THP: `cat /sys/kernel/mm/transparent_hugepage/enabled`   (note the default value)
   - To enable THP : `sudo -i ;` to enter root mode. Then, `echo "always" > /sys/kernel/mm/transparent_hugepage/enabled` (after completing the experiments, you may want to reset this to default by repeating `echo "<default value>" > ...`)

**2. Set the CPU Frequency to a stable value (this ensures stable bit-rate measurement):**
   - On Fedora and Ubuntu: First, check the default frequency governor set in the current policy: `cpupower frequency-info | grep governor`
   - Set the frequency governer to "performance":  `sudo cpupower frequency-set -g performance`
   - Read the frequency and make sure it is relatively stable: `for i in 1 2 3 4 5; do cpupower frequency-info | grep "current CPU frequency"; done`
   - Note the average frequency and set it in the next step in `src/utils.hh`

**3. Setting the System-Specific Parameters in `src/utils.hh`:**
   - Set the `SYS_FREQ_MHZ` (to the average system frequency in MHz, as measured above)
   - Set the `LLC_MISS_THRESHOLD_CYCLES` by profiling it as below: 
       - `cd system_config; ./run_profiling.sh`. The recommended `LLC_MISS_THRESHOLD_CYCLES` is printed at end of the output (and in results.txt).
       - Distribution of LLC-Hits vs LLC-Miss latencies can be visualized with python2 using `python plot_latency_dist.py` and otherwise with python3 using `jupyter notebook visualize_results.ipynb` and running the first script.
       - Note that we are measuring same-core LLC-Hit latencies; cross-core LLC-Hit latencies are much closer to the LLC-Miss latency. Hence we recommend using the min of the LLC-Miss latencies as the `LLC_MISS_THRESHOLD_CYCLES`.  
   - Set the `CACHE_SZ` in src/utils.hh (to LLC Size in _Bytes_). This is identified using `grep "cache size" /proc/cpuinfo`
       - If `CACHE_SZ > 12MB`, then the size of `shared_readonly_file.txt` has to be increased (git does not allow files larger than 100MB). This can be done by `head -c (1+8*<CACHE_SZ_IN_MB>)M </dev/urandom >shared_readonly_file.txt`
   - Set the `SHARED_READONLY_FILE_PATH` (to the full path of shared_readonly_file.txt in the repository)
   
**4. Building the Attack:**
   - To build the binaries for all the attack experiments, use `make all`
   - Otherwise, to only build the binaries for some of the experiments, use the following:
       - For the baseline attack (Figure-9, Table-2 in paper) : `make base`
       - For the attack with ECC enabled (Table-3 in paper) : `make ecc`
       - For the sensitivity study varying shared-array sizes (Table-4 in paper) : `make array_sz`
       - For the sensitivity study with varying synchronization-periods (Table-5 in paper) : `make sync_period`

**5. Testing the Base Attack:**
   - Run the command: `numbits=1000000; sudo ./bin/receiver.o -n $numbits & sudo ./bin/sender.o -n $numbits >>sender_out.log 2>&1`
   - The code requires sudo privilege to set core-affinity and scheduler-policy/priority for the program.
   - The test should run within a few seconds and print the following output:
       - First, the Bit-Period, Bit-rate, and bit-error-rates (with a breakup of 1->0 errors and 0->1 errors).
       - Then, the statistics per epoch of 200,0000 bits (the granularity at which synchronization occurs).
   - FinalCorrectSamples in the output should be close to 99% (i.e. error-rate close to 1%).
       - A high error-rate could indicate a misconfiguration of the attack parameters; subsequent experiments might fail as well.  
   
**6. Running the Experiments:**
   - While running the experiments, you may be prompted to enter the sudo password for each experiment run. To allow all the experiments to run uninterrupted, the password timeout can be extended to 3 hours by editing the `/etc/sudoers` file as described in this [link](https://www.tecmint.com/set-sudo-password-timeout-session-longer-linux/).
       - Do backup the `/etc/sudoers` file and make sure to avoid any errors while modifying it (mis-configuring this file can cause sudo to stop working).
   - All the experiments can be run using `./run_exp.sh` (completes in 2-3 hours).
   - Experiments can be run individually as follows:
       - For base attack (Figure-9, Table-2 in paper): `cd results/base; ./run_base.sh`
       - For the attack with ECC enabled (Table-3 in paper) : `cd results/ecc; ./run_ecc.sh`
       - For the sensitivity study varying shared-array sizes (Table-4 in paper) : `cd results/array_sz; ./run_array_sz.sh`
       - For the sensitivity study with varying synchronization-periods (Table-5 in paper) : `cd results/sync_period; ./run_sync_period.sh`

**7. Analyzing the Results:**
   - After the run-scripts complete, the results are saved in `results/*/*_results.txt` for each experiment.
   - To visualize the results (Fig-9, Tables 2, 3, 4, 5), use `jupyter notebook visualize_results.ipynb`
       - The Attack Bitrate graph (Fig-9) can alternatively also be generated with python2, using `cd results/base ; python plot_bitrate.py`
   - Note that the protocol in the artifact code is flipped (action on bit value 0 and 1 are opposite) compared to the paper. So the results script flips the 0->1 and 1->0 labels for Table-2. 


**8. Concluding Notes:**
   - We tested the attack originally on Intel Xeon E3-1270 v5 (Skylake) and subsequently on Intel Core i7-8700K (Kaby Lake), obtaining similar results.
   - The transmission error-rates can vary by up to 5% on a new system and the LLC-Miss-Threshold-Cycles may need to be tuned to bring it down.
   - Similarly, the Flush+Reload synchronization is sensitive to the LLC-Miss-Threshold-Cycles and loss of synchronization can lead to channel breakdown.