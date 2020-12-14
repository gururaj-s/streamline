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
    *On Fedora, this can typically be installed with `sudo dnf install kernel-tools`
    *On Ubuntu, this can typically be installed with `sudo apt-get install linux-tools-<KERNEL-VERSION>-generic`
* Python3 and Jupyter Notebooks for Plotting & Visualizing results:
    *On Fedora, this can be typically installed with `sudo dnf install python3 python3-jupyter_core`
    *On Ubuntu, you can refer this [link](https://www.digitalocean.com/community/tutorials/how-to-set-up-jupyter-notebook-with-python-3-on-ubuntu-18-04) for instructions.
    
### Steps to Run

1. Enable Transparent Huge Pages (THP):
   - On Fedora and Ubuntu: Checking the status of THP: `cat /sys/kernel/mm/transparent_hugepage/enabled`   (note the default value)
   - To enable THP : `sudo -i ;` to enter root mode. Then, `echo "always" > /sys/kernel/mm/transparent_hugepage/enabled` (after completing the experiments, you may want to reset this to default by repeating `echo "<default value>" > ...`)

2. Set the CPU Frequency to a stable value (this ensures stable bit-rate measurement):
   - On Fedora and Ubuntu: First, check the default frequency governor set in the current policy: `cpupower frequency-info | grep governor`
   - Set the frequency governer to "performance" - `sudo cpupower frequency-set -g performance`
   - Read the frequency and make sure it is relatively stable - `for i in 1 2 3 4 5; do cpupower frequency-info | grep "current CPU frequency"; done`
   - Note the average frequency and set it in the next step in `src/params.hh`

3. Setting the System-Specific Parameters in `src/params.hh`:
   - Set the SYS_FREQ_MHZ in src/utils.hh (set it to the average system frequency in MHz measured above)
   - Set the DEFAULT_FILENAME path in src/fr_util.hh (set it to the full path of shared_readonly_file.txt in the repository)
   - **TODO: Fix SegFault with this Step**:- Set the CACHE_SZ in src/utils.hh (set it to the LLC Size in Bytes). The size in KB can be identified using `grep "cache size" /proc/cpuinfo`
       - If CACHE_SZ > 12MB, then size of shared_readonly_file.txt needs to be increased (to beyond 8*CACHE_SZ). This can be done with `head -c <CACHE_SZ_IN_MB+1>M </dev/urandom >shared_readonly_file.txt`
   - **TODO: LLC-Hit-Threshold**
   
4. Building the Attack:
   - To build the binaries for all the attack experiments, use `make all`
   - To only build the binaries for some of the experiments, use the following:
       - For the baseline attack (Figure-9, Table-2 in paper) : `make base`
       - For the attack with ECC enabled (Table-3 in paper) : `make ecc`
       - For the sensitivity study varying shared-array sizes (Table-4 in paper) : `make array_sz`
       - For the sensitivity study with varying synchronization-periods (Table-5 in paper) : `make sync_period`
       - **TODO** For the prior work comparison (Flush+Reload) (Figure-10 in paper) : `make prior_FR`


5. Testing the Base Attack:
   - Run the command: `numbits=1000000; sudo ./bin/receiver.o -n $numbits &; sudo ./bin/sender.o -n $numbits >>sender_out.log 2>&1` ;
   - The code requires sudo privilege to set core-affinity and scheduler-policy/priority for the program.
   - The test should run within a few seconds and print the following output:
       - First, the Bit-Period, Bit-rate, and bit-error-rates (with a breakup of 1->0 errors and 0->1 errors).
       - Then, the statistics per epoch of 200,0000 bits (the granularity at which synchronization occurs).
   - FinalCorrectSamples in the output should be close to 99% (i.e. error-rate close to 1%).
       - A high error-rate could indicate a misconfiguration of the attack parameters; subsequent experiments might fail as well.  
   
5. Running the Experiments:
   - While running the experiments, you may be prompted to enter the sudo password for each experiment run. To allow all the experiments to run uninterrupted, the password timeout can be set to 2 hours: by adding `Defaults        env_reset,timestamp_timeout=120` line to `/etc/sudoers` file as described in this [link](https://www.tecmint.com/set-sudo-password-timeout-session-longer-linux/).
   - All the experiments can be run using `./run_exp.sh` (completes in less than 1.5 hours).
   - Experiments can be run individually as follows:
       - For base attack (Figure-9, Table-2 in paper): `cd results/base; ./run_base.sh`
       - For the attack with ECC enabled (Table-3 in paper) : `cd results/ecc; ./run_ecc.sh`
       - For the sensitivity study varying shared-array sizes (Table-4 in paper) : `cd results/array_sz; ./run_array_sz.sh`
       - For the sensitivity study with varying synchronization-periods (Table-5 in paper) : `cd results/sync_period; ./run_sync_period.sh`

6. Analyzing the Results:
   - After the run-scripts complete, the results are saved in : `results/*/*_results.txt` for each of the experiments.
   - **TODO** To visualize the results, use `jupyter notebook plot_results.*`