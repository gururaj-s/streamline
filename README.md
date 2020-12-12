# Streamline Covert Channel Attack (ASPLOS'21)

### Citation:  
Gururaj Saileshwar, Christopher Fletcher and Moinuddin Qureshi. **Streamline: A Fast, Flushless Cache Covert-Channel Attack by Enabling Asynchronous Collusion**. In _Proceedings of the 26th International Conference on Architectural Support for Programming Languages and Operating Systems, ASPLOS 2021_.

### Packages Required
* Command-line tool *cpupower* : On Fedora `dnf install kernel-tools`   

### Steps to Run

1. Enable Transparent Huge Pages (THP):
- On Fedora: Checking if this is enabled : `cat /sys/kernel/mm/transparent_hugepage/enabled`  
- On Fedora: Enabling THP : `su ;` to enter super-user mode. Then, `echo "always" > /sys/kernel/mm/transparent_hugepage/enabled`  

2. Setting the CPU Frequency to fixed value (this ensures stable bit-rate measurement)
- On Fedora: Setting constant frequency - `sudo cpupower frequency-set -g performance`
- On Fedora: Reading the frequency - `cpupower frequency-info | grep "current CPU frequency"`

