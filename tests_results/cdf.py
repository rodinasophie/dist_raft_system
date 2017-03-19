import numpy as np

import matplotlib.pyplot as plt
import sys
import glob

files=glob.glob("election_times/*.txt")

fig=plt.figure(facecolor='white')
fig.suptitle('Election Test', fontsize=20, fontweight='bold')
plt.xlabel('Microseconds', fontsize=18)
plt.ylabel('Probability', fontsize=16)
plt.locator_params(nbins=14)
plt.xlim([0, 800000])
plt.ylim([0, 1])
plt.grid()
#stri="Mean = " + str(int(np.mean(data)))
#plt.figtext(0.6, 0.2,stri, fontweight='bold', fontsize='20',color='red')
colors=['red','green', 'blue', 'yellow', 'orange', 'pink', 'grey','black', 'c']
i=0
for arg in files:
    data = np.loadtxt(arg)
    sorted_data = np.sort(data)
    yvals=np.arange(len(sorted_data))/float(len(sorted_data))
    plt.plot(sorted_data,yvals,'r--', color=colors[i], linewidth=2.0)
    #plt.scatter(sorted_data,yvals,color=colors[i], s=15)
    parts=arg.split('.')
    parts=parts[0].split('/')
    plt.plot(1,1,colors[i], label=parts[1])
    i=i+1
plt.legend( loc='upper left', numpoints = 1,prop={'size':12} )
plt.show()
plt.savefig("LeaderElection.png")
