#! /usr/bin/python
# -*- coding: utf-8 -*-
'''
#=============================================================================
#     FileName: gdbtovcd.py
#         Desc: convert gdb output to vcd file
#       Author: KuoE0
#        Email: kuoe0.tw@gmail.com
#     HomePage: http://kuoe0.ch/
#      Version: 0.0.1
#   LastChange: 2012-10-15 16:24:15
#      History:
#=============================================================================
'''

import re

# read all file lines
gdbfile = open( 'gdb.txt' )
lines = gdbfile.readlines()
gdbfile.close()
# regex pattern declaration
btm_regex = re.compile('^#\d+\s+0x00000000.*')
task_regex = re.compile( '^#\d+\s+(0x\w+\s+in\s)?(?P<task>\w+).*' )
time_regex = re.compile( '.*?tick_count\s=\s(?P<ms>\d+)')

# store context switch
events = list()
# store task type
task = dict()

# record start time
cmp_obj = time_regex.search( lines[ 0 ] )
lines.remove( lines[ 0 ] )


last = ""
cur = ""
found = False
e = dict()

# read all data
for i in range(len(lines)):
    if lines[ i ] == "\n":
        continue

    last = cur
    cur = lines[ i ]

    if time_regex.match( cur ):
        e = dict()
        cmp_obj = time_regex.search( cur )
        e[ 'ms' ] = int( cmp_obj.group( 'ms' ) )

    # found task
    if lines[ i ].find("init_user_tasks") == -1 and task_regex.match( lines[ i ] ):
        # extract task name
        cmp_obj = task_regex.search( lines[ i ]  )
        task_name = cmp_obj.group( 'task' )
        e[ 'task' ] = task_name
        # add task
        if task_name not in task:
            task[ task_name ] = len( task )
        events.append( e )

start_time = events[ 0 ][ 'ms' ]

# output vcd file
vcdfile = open( 'sched.vcd', 'w' )
vcdfile.write( '$version\n' )
vcdfile.write( '$end\n' )
vcdfile.write( '$timescale 1us\n' )
vcdfile.write( '$end\n' )
for t in task:
    vcdfile.write( '$var wire 1 ' + t + ' ' + t + ' $end\n' )
vcdfile.write( '$dumpvars\n' )

last_e = -1
d = 0
for e in events:
    diff_time = e[ 'ms' ] - start_time

    if diff_time - last_e > 0:
        d = 0;
    else:
        d = d + 1

    vcdfile.write( '#' + str(diff_time * 1000 + d * 100) + '\n' )
    last_e = diff_time

    for t in task:
        if t != e[ 'task' ]:
            vcdfile.write( 'b0 ' )
        else:
            vcdfile.write( 'b1 ' )
        vcdfile.write( t + ' \n' );

vcdfile.write( '$end' )


