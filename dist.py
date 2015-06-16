import numpy as np

import matplotlib.pyplot as plt

data = np.loadtxt('time')
fig=plt.figure(facecolor='white')
fig.suptitle('Candidate\'s crash test:\n distribution of client request times', fontsize=20, fontweight='bold')
plt.xlabel('Microseconds', fontsize=18)
plt.ylabel('Frequency', fontsize=16)
hist=plt.hist(list(data), bins=500,color='green')
plt.xlim([0, 12000]);
plt.ylim([0, 100]);
plt.grid()
mn=np.mean(data)
ax=fig.add_subplot(111)
ax.text(10000, 70, "Mean = "+str(mn), style='italic',
        bbox={'facecolor':'white', 'alpha':0.5, 'pad':10})
plt.locator_params(nbins=14)
fig = plt.plot()
plt.show()

