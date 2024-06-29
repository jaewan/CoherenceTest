import pandas as pd
import matplotlib.pyplot as plt
from matplotlib import rcParams
import numpy as np

def print_intra_numa_cost(df):
    df['d'] = df['diff'].diff()
    idx = 1
    soft_numa_avg = 0
    sni = 0;
    for i in df['d']:
        if i < 1.9 :
            soft_numa_avg = soft_numa_avg + i
            sni = sni + 1
        idx = idx + 1
    print('inter soft numa avg', soft_numa_avg/sni)
    sni = 0
    idx = 0
    soft_numa_avg =0
    for i in df['d']:
        if i > 1.9 and i < 4:
            soft_numa_avg = soft_numa_avg + i
            sni = sni + 1
        idx = idx + 1
    print('intra soft numa avg', soft_numa_avg/sni)

def get_slope(df, x1, x2):
    return ((df.loc[x2]['diff'] - df.loc[x1]['diff']) / (x2 - x1) )

# Set font family and size for ACM papers
rcParams['font.family'] = 'Liberation Serif'
rcParams['font.size'] = 10

attribute = 'diff'
result_files = 'extrapolate.pdf'

# Read the results from the CSV files
df = pd.read_csv('results/coherence_atomic.csv')

interSN = 25.8
intraSN = 106.6
interHN = 184.9
interCXL = [200,400,800]
base_slope = get_slope(df, 0, 127)

df0 = pd.DataFrame(columns=['threads', attribute])
df1 = pd.DataFrame(columns=['threads', attribute])
df2 = pd.DataFrame(columns=['threads', attribute])

avg_inter_soft_numa = 0.6620601898148154
avg_intra_soft_numa = 2.8215290322580624

x0 = df['diff'].iloc[-1] + (avg_inter_soft_numa * (interCXL[0]/interSN))
x1 = df['diff'].iloc[-1] + (avg_inter_soft_numa * (interCXL[1]/interSN))
x2 = df['diff'].iloc[-1] + (avg_inter_soft_numa * (interCXL[2]/interSN))

thread_num = 257
df0.loc[len(df0)] = [thread_num, x0]
df1.loc[len(df1)] = [thread_num, x1]
df2.loc[len(df2)] = [thread_num, x2]

thread_num = thread_num + 127
df0.loc[len(df0)] = [thread_num, (base_slope*(interCXL[0]/intraSN) * 128 + x0)]
df1.loc[len(df1)] = [thread_num, (base_slope*(interCXL[1]/intraSN) * 128 + x1)]
df2.loc[len(df2)] = [thread_num, (base_slope*(interCXL[2]/intraSN) * 128 + x2)]

# Plot the results
plt.figure(figsize=(4, 3))  # Optional: Set figure size

plt.plot(df['threads'], df[attribute], label='Empirical Single Node', linewidth=1) 

plt.plot(df0['threads'], df0[attribute], label='Est Multi Node 200ns', color='#88e488', linewidth=1, linestyle='--')
plt.plot(df1['threads'], df1[attribute], label='Est Multi Node 400ns', color='#5ac25a', linewidth=1, linestyle='--')
plt.plot(df2['threads'], df2[attribute], label='Est Multi Node 800ns', color='#2ca02c', linewidth=1, linestyle='--')


plt.axvline(x=256, color='red', linestyle='--', linewidth=0.5)
plt.axvline(x=128, color='red', linestyle='--', linewidth=0.5)


# Improve title and labels clarity
#plt.title('Cache Coherence performance', fontsize=15)
plt.xlabel('Number of Cores', fontsize=15, fontweight='bold')
plt.ylabel('Coherence Overhead', fontsize=15, fontweight='bold')  # Clarify y-axis label

# Customize grid for better readability
plt.grid(True, linestyle='--', linewidth=0.5, alpha=0.7)  # Use dashed lines, adjust as needed

plt.tick_params(axis='both', which='major', labelsize=12)

# Add legend with smaller font size
plt.legend(fontsize=12)

# Ensure tight layout with appropriate padding
plt.tight_layout(pad=0.5)

# Save the figure with high resolution
plt.savefig('/home/domin/' + result_files,
            format='pdf',
            dpi=300,
            bbox_inches='tight')
