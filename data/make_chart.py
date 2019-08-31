import sys
import seaborn as sns
import pandas as pd
import matplotlib.pyplot as plt

file_name = str(sys.argv[1])

with open(file_name) as f:
    ts_goal = int(f.readline())
data = pd.read_csv(file_name, skiprows=1, sep=",")
print(ts_goal)

print(data['Service_Time'])
print(data['Time'])
print(data['Degree'])

#sns.set_style("whitegrid")
fig, ax = plt.subplots()
sns.lineplot(x='Time', y='Service_Time', data=data, markers=True, ax=ax)
#data['Service_Time'].max()
#plt.ylim(ymax = 250, ymin = 25)
ax2 = ax.twinx()
sns.lineplot(x='Time', y='Degree', data=data, markers=True, ax=ax2, color='r')
#plt.ylim(ymax = 20, ymin = 0)
ax.axhline(ts_goal, ls='--')
plt.show()
