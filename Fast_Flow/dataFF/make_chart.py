import sys
import seaborn as sns
import pandas as pd
import matplotlib.pyplot as plt

file_name = str(sys.argv[1])
with open(file_name) as f:
    ts_goal = int(f.readline())
    ts_upper_bound = int(f.readline())
    ts_lower_bound = int(f.readline())

print(ts_goal)
print(ts_upper_bound)
print(ts_lower_bound)

data = pd.read_csv(file_name, skiprows=3, sep=",")

print(data['Service_Time'])
print(data['Time'])
print(data['Degree'])

fig, ax = plt.subplots()
ax.grid(linestyle='dashed') 
ax.plot(data['Time'].tolist(),data['Service_Time'].tolist(),  '-.', color='black', label='Farm Service_Time') 
plt.legend(loc='upper left')
plt.xlabel('Time')
ax.set_ylabel('Service_Time')
ax2 = ax.twinx()
ax2.plot(data['Time'].tolist(),data['Degree'].tolist(), color='black', label='Degree') 
plt.legend(loc='upper right')
ax2.set_ylabel('Degree')
plt.show()
