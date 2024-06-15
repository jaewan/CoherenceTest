import pandas as pd
import matplotlib.pyplot as plt

# Read the results from the CSV files
df1 = pd.read_csv('results/local_write_bandwidth.csv')
df2 = pd.read_csv('results/remote_write_bandwidth.csv')
df3 = pd.read_csv('results/cxl_write_bandwidth.csv')

# Plot the results
fig, axs = plt.subplots(3, figsize=(10, 12))

# Plot first dataframe
axs[0].bar(df1['threads'], df1['bandwidth'])
axs[0].set_title('Write Bandwidth vs Number of Threads - Dataframe 1')
axs[0].set_xlabel('Number of Threads')
axs[0].set_ylabel('Write Bandwidth (GB/s)')
axs[0].grid(True)

# Plot second dataframe
axs[1].bar(df2['threads'], df2['bandwidth'])
axs[1].set_title('Write Bandwidth on different memory access types with increasing number of threads ')
axs[1].set_xlabel('Number of Threads')
axs[1].set_ylabel('Write Bandwidth (GB/s)')
axs[1].grid(True)

# Plot third dataframe
axs[2].bar(df3['threads'], df3['bandwidth'])
axs[2].set_title('Write Bandwidth vs Number of Threads - Dataframe 3')
axs[2].set_xlabel('Number of Threads')
axs[2].set_ylabel('Write Bandwidth (GB/s)')
axs[2].grid(True)

plt.tight_layout()
plt.savefig('results/write_bandwidth.png')
