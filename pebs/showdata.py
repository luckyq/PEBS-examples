import sys 
import numpy as np
import matplotlib.pyplot as plt


f = open(sys.argv[1])

lines = f.readlines()
x1 = list()
x2 = list()
for line in lines:
    line = line.split(" ")
    if len(line)>=2:
        x1.append(int(line[0]))
        x2.append(int(line[1]))
    print(line)
        
X = np.array(range(0, len(x1)))

start =0
for i in range(len(X)):
    if x1[i]==1 or x1[i]==0:
        continue
    else :
        start = i
        break

x2 = x2[start:]
x1 = x1[start:]
X = X[start:]
X = [x-start for x in X]


# sum =  np.array( x1) + np.array(x2)
# x1 = np.array(x1)/np.array(sum)
# x2 = np.array(x2)/np.array(sum)
# plt.axes(yscale = "log", xscale="log")
# plt.bar(X, x1, fc="r", label="<10")
plt.bar(X, x1,fc="g",label="<"+sys.argv[3])
plt.bar(X, x2, bottom=x1, fc="r", label=">="+sys.argv[3])
# print(x1)
# print(x2)
plt.ylim(0,2500)

# for a,b in zip(X,Y):
#     plt.text(a, b+0.05, '%d' % b, ha='center', va= 'bottom',fontsize=7)
plt.xlabel('profiling iteration')
plt.ylabel('page number')
plt.legend()
plt.savefig(sys.argv[2], dpi=200)
