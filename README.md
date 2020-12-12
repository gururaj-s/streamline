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