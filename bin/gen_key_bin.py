#!/usr/bin/env python

f = open('master_device_key.txt','r')
key = f.readline().strip()

g = open('key.bin','w')
g.write('{:\xFF<128}'.format(key))
