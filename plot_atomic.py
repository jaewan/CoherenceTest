import pandas as pd
import matplotlib.pyplot as plt

# Read the results from the CSV files
df1 = pd.read_csv('results/local_atomic.csv')
df2 = pd.read_csv('results/remote_atomic.csv')
df3 = pd.read_csv('results/cxl_atomic.csv')
df4 = pd.read_csv('results/cxl_remote_atomic.csv')

# Plot the results
plt.figure(figsize=(10, 6))

# Plot first dataframe
plt.plot(df1['threads'], df1['num_ops'], label='Local Node')

# Plot second dataframe
plt.plot(df2['threads'], df2['num_ops'], label='Remote Node')

# Plot third dataframe
plt.plot(df3['threads'], df3['num_ops'], label='CXL')

# Plot fourth dataframe
plt.plot(df4['threads'], df4['num_ops'], label='CXL Remote')

plt.title('#of Atomic operations for 3 seconds on different memory types with increasing number of threads')
plt.xlabel('Number of Threads')
plt.ylabel('Number of operations')
plt.legend()
plt.grid(True)

plt.tight_layout()
plt.savefig('results/atomic_operation.png')
