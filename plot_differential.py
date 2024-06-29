import pandas as pd
import matplotlib.pyplot as plt
from matplotlib import rcParams
import numpy as np
from scipy.signal import savgol_filter

# Set font family and size for ACM papers
rcParams['font.family'] = 'Liberation Serif'
rcParams['font.size'] = 10

attribute = 'diff'
result_files = 'differential.pdf'

# Read the results from the CSV files
df2 = pd.read_csv('results/coherence_atomic.csv')

# Apply Savitzky-Golay filter for smoothing
df2['smooth'] = savgol_filter(df2[attribute], window_length=11, polyorder=3)

# Calculate the derivative
df2['slope'] = np.gradient(df2['smooth'], df2['threads'])

# Apply moving average to the slope
window_size = 5
#df2['avg_slope'] = df2['slope'].rolling(window=window_size).mean()

# Plot the results
fig, ax = plt.subplots(figsize=(4, 3))  # Adjust figure size for a compact look

window_size = 5
df2['avg_slope'] = df2['slope'].rolling(window=window_size).mean()

# Plot smoothed derivative
ax.plot(df2['threads'].iloc[window_size-1:], df2['avg_slope'].iloc[window_size-1:], label='Atomic Derivative', linewidth=1, color='green')

ax.axvline(x=128, color='red', linestyle='--', linewidth=0.5)

# Improve title and labels clarity
#ax.set_title('Derivatives of Empirical Performance', fontsize=15)
ax.set_xlabel('Number of Cores', fontsize=15, fontweight='bold')
ax.set_ylabel('Derivatives', fontsize=15, fontweight='bold')  # Clarify y-axis label

# Customize grid for better readability
ax.grid(True, linestyle='--', linewidth=0.5, alpha=0.7)  # Use dashed lines, adjust as needed
ax.axvline(x=128, color='red', linestyle='--', linewidth=0.5)

plt.tick_params(axis='both', which='major', labelsize=12)

# Adjust y-axis to focus on the area of interest
ax.set_ylim(0, 2)  # Adjust these values as needed

# Ensure tight layout with appropriate padding
plt.tight_layout(pad=0.5)

# Save the figure with high resolution
plt.savefig(result_files,
            format='pdf',
            dpi=300,
            bbox_inches='tight')
