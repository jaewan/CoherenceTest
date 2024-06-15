import pandas as pd
import matplotlib.pyplot as plt

# Read the results from the CSV files
df1 = pd.read_csv('results/local_write_bandwidth.csv')
df2 = pd.read_csv('results/remote_write_bandwidth.csv')
df3 = pd.read_csv('results/cxl_write_bandwidth.csv')

# Plot the results
plt.figure(figsize=(10, 6))

# Plot first dataframe
plt.plot(df1['threads'], df1['bandwidth'], label='Local Node')

# Plot second dataframe
plt.plot(df2['threads'], df2['bandwidth'], label='Remote Node')

# Plot third dataframe
plt.plot(df3['threads'], df3['bandwidth'], label='CXL')

plt.title('Write Bandwidth on different memory types with increasing number of threads')
plt.xlabel('Number of Threads')
plt.ylabel('Write Bandwidth (GB/s)')
plt.legend()
plt.grid(True)

plt.tight_layout()
plt.savefig('results/write_bandwidth.png')
