#!/usr/bin/env python

N = 1000000

ways = [0]*(N + 1)
nums = []

def gen(now, index, cur):
    if index < 10:
        print index, now
    if now <= N:
        if ways[now] == 0:
            ways[now] = 1
            for i in xrange(index, len(nums)):
                j = 0
                while j < len(cur) and 0 != cur[j] % nums[i]:
                    j += 1
                if j == len(cur):
                    cur.append(nums[i])
                    gen(now + nums[i], i + 1, cur)
                    cur.pop()
        else:
            ways[now] = 2

for i in xrange(1, N + 1):
    j = i
    while j % 2 == 0:
        j /= 2
    while j % 3 == 0:
        j /= 3
    if j == 1:
        nums.append(i)

gen(0, 0, [])

sum = 0
erato = [True]*N
for i in xrange(2, N):
    if erato[i]:
        if ways[i] == 1:
            sum += i
        j = i + i
        while j < N:
            erato[j] = False
            j += i
print sum
