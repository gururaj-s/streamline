# Streamline Covert Channel Attack (ASPLOS'21)

### Citation  
Gururaj Saileshwar, Christopher Fletcher and Moinuddin Qureshi. **Streamline: A Fast, Flushless Cache Covert-Channel Attack by Enabling Asynchronous Collusion**. In _Proceedings of the 26th International Conference on Architectural Support for Programming Languages and Operating Systems, ASPLOS 2021_.

### Hardware Requirements
* Intel CPU from Haswell or newer generation
    * You can check your CPU model with `cat /proc/cpuinfo | grep "model name"` and learn which generation it is by looking up the model on [Wikichip](http://wikichip.org) or [Intel's Repository](https://ark.intel.com).
* Native execution
* Sudo priviliges (to enable THP and set the CPU frequency to a constant value)  
  
### Software Requirements
* Command-line tool *cpupower* : On Fedora, this can be installed with `sudo dnf install kernel-tools`  

### Steps to Run

1. Enable Transparent Huge Pages (THP):
   - On Fedora: Checking if this is enabled : `cat /sys/kernel/mm/transparent_hugepage/enabled`  
   - On Fedora: Enabling THP : `su ;` to enter super-user mode. Then, `echo "always" > /sys/kernel/mm/transparent_hugepage/enabled`  

2. Set the CPU Frequency to fixed value (this ensures stable bit-rate measurement):
   - On Fedora: Setting constant frequency - `sudo cpupower frequency-set -g performance`
   - On Fedora: Reading the frequency - `cpupower frequency-info | grep "current CPU frequency"`

3. Setting the System-Specific Parameters in `src/params.hh`:
   - **TODO**

4. Building the Attack:
   - To build the binaries for all the attack experiments, use `make all`
   - To only build the binaries for some of the experiments, use the following:
       - For the baseline attack (Figure-9, Table-2 in paper) : `make base`
       - For the attack with ECC enabled (Table-3 in paper) : `make ecc`
       - For the sensitivity study varying shared-array sizes (Table-4 in paper) : `make array_sz`
       - For the sensitivity study with varying synchronization-periods (Table-5 in paper) : `make sync_period`
       - **TODO** For the prior work comparison (Flush+Reload) (Figure-10 in paper) : `make prior_FR`


5. Testing the Base Attack:
   - Run the command: `numbits=1000000; sudo ./bin/receiver.o -n $numbits &; sudo ./bin/sender.o -n $numbits >>sender_out.log 2>&1 ;
   - The code requires sudo privilege to set core-affinity and scheduler-policy/priority for the program. (You may be prompted to enter the sudo password)
   - First, the output prints the Bit-Period, Bit-rate in Bits/second, and bit-error-rates (with a breakup of 1->0 errors and 0->1 errors).
   - Then, the output prints statistics per epoch of 200,0000 bits (the granularity at which synchronization occurs).
   - Ideally, the results should show the rate of CorrectSamples >99% (i.e. error-rate <1%). If not something is wrong.
   
5. Running the Experiments:
   - While running the experiments, you may be prompted to enter the sudo password for each experiment run. To allow all the experiments to run uninterrupted, the password timeout can be set to 2 hours: by adding `Defaults        env_reset,timestamp_timeout=120` line to `/etc/sudoers` file as described in this [link](https://www.tecmint.com/set-sudo-password-timeout-session-longer-linux/).
   - **TODO** All the experiments can be run using `./run_exp.sh` (completes in less than 1.5 hours).
   - Experiments can be run individually as follows:
       - For base attack (Figure-9, Table-2 in paper): `cd results/base; ./run_base.sh`
       - **TODO** For the attack with ECC enabled (Table-3 in paper) : `cd results/ecc; ./run_ecc.sh`
       - **TODO** For the sensitivity study varying shared-array sizes (Table-4 in paper) : `cd results/array_sz; ./run_array_sz.sh`
       - **TODO** For the sensitivity study with varying synchronization-periods (Table-5 in paper) : `cd results/sync_period; ./run_sync_period.sh`
