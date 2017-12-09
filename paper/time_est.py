import matplotlib.pyplot as plt
import sys
import numpy as np

xnames = ['Client', 'Server']

with open('establish.txt') as f:
    lines = f.readlines()

values = []
errs = []

for x in lines:
    strs = x.split()
    nums = [float(a) for a in strs]
    values.append(round(nums[0], 2) / 1000)
    errs.append((nums[1]*2+17) / 1000)

ind = np.arange(len(values))
ind = [2, 2.2]
width = 0.1       # the width of the bars

fig, ax = plt.subplots()
rects1 = ax.bar(ind, values, width, color='#5555ff', yerr=errs)

rects = ax.patches

for rect, label in zip(rects, values):
    height = rect.get_height()
    ax.text(rect.get_x() + rect.get_width()/2, height + 20, label, ha='center', va='bottom')


plt.ylabel('Time (us)')
plt.xticks(ind, xnames)
plt.tight_layout()


plt.savefig('fig/time_est.eps', format='eps', dpi=1200)

