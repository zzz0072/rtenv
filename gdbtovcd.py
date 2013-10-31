#!/usr/bin/env python
# -*- coding: utf-8 -*-

import re
def getAllFuncs():
    """docstring for getAllFuncs"""
    with open(source, 'r') as input:
        data = input.readlines()
    functionDict = {}
    for datum in data:
        match = funcPattern.search(datum)
        if  match:
            functionDict[match.group(1)] = True
    return functionDict

def valuesParsing():
    """parse values to precised tick"""
    with open(source, 'r') as input:
        data = input.readlines()
    status = 0;
    timeList = []
    preCur = 999999999
    preTick = 0
    for datum in data:
        if  status is 0:
            match = funcPattern.search(datum)
            if match:
                status = 1
                name = match.group(1)
        elif status is 1:
            match = valuesPattern.search(datum)
            if  match:
                # temp[0] -> systick intrrupt trigger times
                # temp[1] -> systick current register
                # temp[0] -> systick reload register
                status = 0
                temp = match.groups()
                dur = (float(temp[2]) - float(temp[1]))
                tick = float(temp[0])+( dur /float(temp[2]))
                if  float(temp[1]) > preCur and preTick == float(temp[0]):
                    tick = tick + 1
                preCur = float(temp[1])
                preTick = float(temp[0])
                print name, temp, tick
                tick = int(tick*10)
                timeList.append((name, tick))
    return timeList

if __name__ == '__main__':
    #config
    source = 'gdb.txt'
    target = 'sched.vcd'
    funcPattern = re.compile(r'^Breakpoint \d, (\w*)')
    valuesPattern = re.compile(r'^tick:(\d*)current:(\d*)countDown:(\d*)')
    
    functionDict = getAllFuncs()
    timeList = valuesParsing()
    #for a in timeList:
        #print a
    with open(target, 'w') as output:
        output.write( '$version\n' )
        output.write( '$end\n' )
        output.write( '$timescale 1us\n' )
        output.write( '$end\n' )
        for name in functionDict:
            output.write( '$var wire 1 {0} {1} $end\n'.format(name, name) )
        output.write( '$dumpvars\n' )
        for time in timeList:
            output.write('#{0}\n'.format(time[1]))
            for name in functionDict:
                if  name == time[0]:
                    output.write('b1 {0}\n'.format(name))
                else:
                    output.write('b0 {0}\n'.format(name))
        output.write( '$end' )
