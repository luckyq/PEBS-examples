import numpy as np 
import sys

f = open(sys.argv[1], "r")
time_slot = 0

#  1: filename ; 2: time interval (ms) ; 3: threshold of accesses, 4: base or huge(12 or 21 )
# if len(sys.argv) == 4:
time_slot = int(sys.argv[2])

time_slot = time_slot * 10**6

records = f.readlines()
L_PM=list()
R_D=list()
R_PM=list()

L_PM_Huge = list()
R_D_Huge  = list()
R_PM_Huge = list()
PM = list()

tcount = [0,0,0,0]
count=0


#  we loop all the records and collect the statistics for each interval
base_time = 0
# print(base_time)
# exit()
p_start = base_time
p_end = base_time+time_slot


for line in records:
        
    line = line.split(" ")
    if len(line) < 4:
        continue
    # print(line[3])
    t=int(line[3])
    cur = int(line[1][:-1])
    # print(cur, p_end)
    if base_time==0:
        base_time = cur
        p_start=base_time
        p_end = base_time+time_slot
    if cur > p_end:
        PM_dict = dict()
        for access in PM:
            if access in PM_dict:
                PM_dict[access] += 1
                # print(access)
            else:
                PM_dict[access] = 1               
        #  calcuate the access distribution in one huge page
        PM_keys = PM_dict.keys()
        key_list = [key for key in PM_keys]
        key_list = np.sort(np.array(key_list))
        cur_pfn =0
        tmp = list()
        for key in key_list:
            pfn = key>>9
            if cur_pfn ==0:
                cur_pfn = pfn
                tmp=list()
                tmp.append(key)
            elif pfn == cur_pfn:
                tmp.append(key)
            else:
                print(pfn,end=" ")
                print(len(tmp), end=" ")
                for key in tmp:
                    print(PM_dict[key], end=" " )
                print("")
                
                tmp =list()
                tmp.append(key)
                cur_pfn = pfn
                
                
        p_start = p_end
        p_end += time_slot
        PM.clear()
        if t==1 or t ==3:
            PM.append(int(line[0])>>int(sys.argv[4]))    
        continue
        
        
        PM_values =  PM_dict.values()
        tmp_dict  = dict()
        for v in PM_values:
            if v in tmp_dict:
                tmp_dict[v]+=1
            else:
                tmp_dict[v] =1  
        keys = [key for key in tmp_dict.keys()]
        keys = np.sort(keys)
        # print(p_end, cur)
        if len(keys)==0:
                print("NULL")
        # if key < 10: 
        res1=0
        res2=0
        for key in keys:
            if key < int(sys.argv[3]):
                res1+=tmp_dict[key]
            else:
                res2+=tmp_dict[key]
            
            # print("%d %d " % (key, tmp_dict[key]), end='')
        print("%d %d " %(res1, res2));
        PM.clear()
        p_start = p_end
        p_end += time_slot
        if t==1 or t ==3:
            PM.append(int(line[0])>>int(sys.argv[4]))         
        # print("")
    else:
        if t==1 or t ==3:
            PM.append(int(line[0])>>int(sys.argv[4]))   
         
exit()

for line in records:
    # get the count of each type
    line = line.split(" ")
    if len(line) < 4:
        continue
    # print(line[3])
    t=int(line[3])
    # print(line)
    tcount[t]+=1
    
    if base_time == -1:
        base_time = int(line[1])
    else:
        base_time = min(base_time, int(line[1]))
    # print(line[0])
    if t == 1:
        L_PM.append(int(line[0])>>12)
        L_PM_Huge.append(int(line[0])>>21)
    elif t == 2:
        R_D.append(int(line[0])>>12)
        R_D_Huge.append(int(line[0])>>21)
    elif t == 3:
        R_PM.append(int(line[0])>>12)
        R_PM_Huge.append(int(line[0])>>21)

# analysis the distribution of each type
# 1. Page access distribution
# 2. Page access count
# show the acccess patten for each memory node

import matplotlib.pyplot as plt


LPM_dict = dict()
R_D_dict = dict()
R_PM_dict= dict()

for access in L_PM:
    if access in LPM_dict:
        LPM_dict[access] += 1
        # print(access)
    else:
        LPM_dict[access] = 1

for access in R_D:
    if access in R_D_dict:
        R_D_dict[access] += 1
    else:
        R_D_dict[access] = 1 
        
for access in R_PM:
    if access in R_PM_dict:
        R_PM_dict[access] += 1
    else:
        R_PM_dict[access] = 1
# **
# Draw figures of memory accesses
# *#

print("L_PM %{d}, R_D %{d}, R_PM %{d}", tcount[1], tcount[2], tcount[3])
LPM_values =  LPM_dict.values()
R_D_values =  R_D_dict.values()
RPMM_values =  R_PM_dict.values()


tmp_dict  = dict()
for v in LPM_values:
    if v in tmp_dict:
        tmp_dict[v]+=1
    else:
        tmp_dict[v] =1
# tmp_v = tmp_dict.values()

X = tmp_dict.keys()
X = [v for v in X]
Y = [tmp_dict[v] for v in X ]
plt.axes(yscale = "log", xscale="log")

plt.bar(X, Y )
for a,b in zip(X,Y):
    plt.text(a, b+0.05, '%d' % b, ha='center', va= 'bottom',fontsize=7)
plt.xlabel('access times')

plt.ylabel('number of page')
plt.savefig("L_PM_base", dpi=200)
plt.clf()


tmp_dict  = dict()
for v in R_D_values:
    if v in tmp_dict:
        tmp_dict[v]+=1
    else:
        tmp_dict[v] =1
# tmp_dict = tmp_dict.values()
X = tmp_dict.keys()
X = [v for v in X]
Y = [tmp_dict[v] for v in X ]
plt.axes(yscale = "log", xscale="log")
plt.bar(X, Y)
for a,b in zip(X,Y):
    plt.text(a, b+0.05, '%d' % b, ha='center', va= 'bottom',fontsize=7)

plt.xlabel('access times')


plt.ylabel('number of page')
plt.savefig("R_D_base", dpi=200)
plt.clf()


tmp_dict  = dict()
for v in RPMM_values:
    if v in tmp_dict:
        tmp_dict[v]+=1
    else:
        tmp_dict[v] =1
# tmp_v = tmp_dict.values()
X = tmp_dict.keys()
X = [v for v in X]
Y = [tmp_dict[v] for v in X ]
plt.axes(yscale = "log", xscale="log")
plt.bar(X, Y )
for a,b in zip(X,Y):
    plt.text(a, b+0.05, '%d' % b, ha='center', va= 'bottom',fontsize=7)
plt.xlabel('access times')


plt.ylabel('number of page')
plt.savefig("R_PM_base", dpi=200)
plt.clf()


# analysis page distribution 

# np.percentile(a, 50)
L_PM_Per=list()
L_R_D_Per =list()
L_R_PMM_Per = list()

site = [10,20,30,40,50,60,70,80,90,100]

for pos in site:
    L_PM_Per.append(np.percentile(L_PM_Huge,pos))
    L_R_D_Per.append(np.percentile(R_D_Huge, pos))
    L_R_PMM_Per.append(np.percentile(R_PM_Huge, pos))
    
print(L_PM_Per)
print(L_R_D_Per)
print(L_R_PMM_Per)

# calculate the numbers of 

LPM_dict_huge = dict()
R_D_dict_huge = dict()
R_PM_dict_huge= dict()

for access in L_PM_Huge:
    if access in LPM_dict_huge:
        LPM_dict_huge[access] += 1
        # print(access)
    else:
        LPM_dict_huge[access] = 1

for access in R_D_Huge:
    if access in R_D_dict_huge:
        R_D_dict_huge[access] += 1
    else:
        R_D_dict_huge[access] = 1 
               
for access in R_PM_Huge:
    if access in R_PM_dict_huge:
        R_PM_dict_huge[access] += 1
    else:
        R_PM_dict_huge[access] = 1


LPM_values_huge =  LPM_dict_huge.values()
R_D_values_huge =  R_D_dict_huge.values()
RPMM_values_huge =  R_PM_dict_huge.values()

#
# Making figures:
#   1. Number of pages which have the same access time
# 
# #

tmp_dict  = dict()
for v in LPM_values_huge:
    if v in tmp_dict:
        tmp_dict[v]+=1
    else:
        tmp_dict[v] =1
# tmp_v = tmp_dict.values()
X = tmp_dict.keys()
X = [v for v in X]
Y = [tmp_dict[v] for v in X ]
plt.axes(yscale = "log", xscale="log")
plt.bar(X, Y )
for a,b in zip(X,Y):
    plt.text(a, b+0.05, '%d' % b, ha='center', va= 'bottom',fontsize=7)
plt.xlabel('access times')

plt.ylabel('number of page')
plt.savefig("L_PM_Huge", dpi=200)
plt.clf()


tmp_dict  = dict()
for v in R_D_values_huge:
    if v in tmp_dict:
        tmp_dict[v]+=1
    else:
        tmp_dict[v] =1
# tmp_v = tmp_dict.values()
X = tmp_dict.keys()
X = [v for v in X]
Y = [tmp_dict[v] for v in X ]
plt.axes(yscale = "log")
plt.bar(X, Y )
for a,b in zip(X,Y):
    plt.text(a, b+0.05, '%d' % b, ha='center', va= 'bottom',fontsize=7)

plt.xlabel('access times')
plt.ylabel('number of page')
plt.savefig("R_D_Huge", dpi=200)
plt.clf()


tmp_dict  = dict()
for v in RPMM_values_huge:
    if v in tmp_dict:
        tmp_dict[v]+=1
    else:
        tmp_dict[v] =1
# tmp_v = tmp_dict.values()
X = tmp_dict.keys()
X = [v for v in X]
Y = [tmp_dict[v] for v in X ]
plt.axes(yscale = "log", xscale="log")
plt.bar(X, Y )
for a,b in zip(X,Y):
    plt.text(a, b+0.05, '%d' % b, ha='center', va= 'bottom',fontsize=7)
plt.xlabel('access times')
plt.ylabel('number of page')
plt.savefig("R_PMM_Huge", dpi=200)
plt.clf()
 


# tmp_list =list()
# for v in LPM_values:
#     tmp_list.append(v)
# LPM_values = np.sort(np.array(tmp_list))
# X1 = np.array(range(0, len(tmp_list)))
# tmp_list =list()
# for v in R_D_values:
#     tmp_list.append(v)
# R_D_values = np.sort(np.array(tmp_list))
# # X2 = np.array(range(0, len(tmp_list)))

# tmp_list =list()
# for v in RPMM_values:
#     tmp_list.append(v)
# RPMM_values = np.sort(np.array(tmp_list))
# X3 = np.array(range(0, len(tmp_list)))

# X = np.array(range(0, len(LPM_values)))
# plt.plot(X1, LPM_values, 'ro',markersize=5)
# plt.xlabel('page')
# plt.ylabel('access times')
# plt.savefig("L_PM_access", dpi=200)
# plt.clf()


# X = np.array(range(0, len(R_D_values)))
# print(X.shape)
# print(R_D_values.shape)
# plt.plot(X, R_D_values, 'ro',markersize=5)
# plt.xlabel('page')
# plt.ylabel('access times')
# plt.savefig("R_D_access", dpi=200)
# plt.clf()


# X = np.array(range(0, len(RPMM_values)))
# plt.plot(X3, RPMM_values, 'ro',markersize=5)
# plt.xlabel('page')
# plt.ylabel('access times')
# plt.savefig("R_PMM_access", dpi=200)
# plt.clf()





# Here we analysis  percenage of the address.





    
    
    
    
    













