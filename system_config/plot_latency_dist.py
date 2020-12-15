# Load the Pandas libraries with alias 'pd' 
import pandas as pd
#Load Seaborn for colors and format.
import seaborn as sns #; sns.set()
#sns.set_style("whitegrid")

import matplotlib.pyplot as plt

# Read Misses data file 'measure_rdtscp.txt' 
csv_data_1 = pd.read_csv("results.txt",skiprows=5,skipfooter=3,engine='python',header=None)
#print csv_data_1
# Drop last col and transpose
data1 = csv_data_1.iloc[:, :-1].T
#print data1
data1.columns = ['misses']

# Read Misses data file 'measure_rdtscp.txt' 
csv_data_2 = pd.read_csv("results.txt",skiprows=2,skipfooter=6,engine='python',header=None) 
# Drop last col and transpose
data2 = csv_data_2.iloc[:, :-1].T
#print data2
data2.columns = ['hits']

data = pd.concat([data1,data2],axis=1)
#print data

data['misses'].hist(bins=range(0,300,2),alpha = 0.5, label='misses',color=sns.xkcd_rgb["blue"])
data['hits'].hist(bins=range(0,300,2),alpha = 0.5, label='hits',color=sns.xkcd_rgb["bright red"])


plt.xlabel('Latencies (cycles)')
plt.ylabel('Samples (out of 2000)')
ax = plt.gca()
ax.legend()
plt.show()
plt.savefig('latency_dist.png') 
