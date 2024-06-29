import pandas as pd
import matplotlib.pyplot as plt
from matplotlib import rcParams

# Set font family and size for ACM papers
rcParams['font.family'] = 'Liberation Serif'  
rcParams['font.size'] = 10

# Data
x = [1, 2, 3, 4, 5, 6, 7, 8]#, 9]  # Note: Adjusted x-value for clarity
y = [4.32, 7.53, 8.06, 8.82, 9.09, 9.44, 10.03, 10.99]#, 13.22]

fig, ax = plt.subplots(figsize=(4, 3))  # Adjust figure size for a compact look
ax.plot(x, y, linewidth=1)  # Adjust linewidth as needed

#ax.set_title('Impact of Thread Placement', fontsize=15)
ax.set_xlabel('# of Soft Numa placement by 8 threads', fontsize=15, fontweight='bold')
plt.ylabel('Coherence Overhead', fontsize=15, fontweight='bold')  # Clarify y-axis label

plt.tick_params(axis='both', which='major', labelsize=12)

ax.grid(True, linestyle='--', linewidth=0.5, alpha=0.7)  # Use dashed lines, adjust as needed
plt.tight_layout(pad=0.5)

# Display the plot
plt.savefig('softnuma.pdf',
            format='pdf',
            dpi=300,
            bbox_inches='tight')
