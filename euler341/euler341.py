#!/usr/bin/env pypy

import sys

sys.setrecursionlimit(2000000)

sys.path[-1] = "../common"

import common

golomb2 = [0, 1, 2]

def golomb2__(n):
    while n >= len(golomb2):
        golomb2.append( golomb2[-1] + (len(golomb2) - 1)*golomb_(len(golomb2) - 1) )
    return golomb2[n]

def golomb2_(n):
    # print "!!!", n
    
    while n >= golomb2[-1]:
        golomb2__(len(golomb2))
    
    # i = 0
    # while n >= golomb2__(i):
    #     i += 1
    # return i - 1

    left = 0
    right = len(golomb2) - 1
    while left < right:
        mid = (left + right) / 2
        val = golomb2__(mid)
        # print "!", n, left, right, mid, val
        if val == n:
            return mid
        elif val > n:
            right = mid
        else:
            left = mid + 1
    while n >= golomb2[left]:
        left -= 1
    return left - 1

@common.memoize
def golomb_(n):
    if 1 == n:
        return 1
    return 1 + golomb_(n - golomb2_(n - 1))

@common.memoize
def golomb(n):
    if 1 == n:
        return 1
    return 1 + golomb(n - golomb(golomb(n - 1)))

if False:
    for i in xrange(1, 100):
        print i, golomb(i), golomb(golomb(i)), golomb2_(i)

    sys.exit(0)

if False:
    for i in xrange(1, 1000):
        print i, golomb_(i), golomb(i)
        assert golomb_(i) == golomb(i)

    sys.exit(0)

assert golomb_(1000) == 86
assert golomb_(1000000) == 6137

N = 10**6
res = 0
for i in xrange(1, N):
    if i % 1000 == 0:
        print i
    res += golomb2_(i*i*i)
print res
