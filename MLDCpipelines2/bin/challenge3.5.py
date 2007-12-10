#!/usr/bin/env python

import os
import sys
import random

import numpy

def run(command):
    commandline = command % globals()
    print "----> %s" % commandline
    
    try:
        assert(os.system(commandline) == 0)
    except:
        print 'Script %s failed at command "%s".' % (sys.argv[0],commandline)
        sys.exit(1)

mydir = os.path.dirname(os.path.abspath(sys.argv[0]))

# two arguments, the seed...
seed = int(sys.argv[1])
nproc = int(sys.argv[3])

random.seed(seed)

# to be run for one month, at one second sampling...

run('%(mydir)s/makesource-StochasticBackground.py --seed=%(seed)s --PSD=1.0e-48 --randomizePSD=0.3 --pixelRefinement=2 --nProc=%(nproc)s Immediate/Background.xml' % globals())

# for LISACode

# run('%(mydir)s/makesource-StochasticBackground.py --seed=%(seed)s --PSD=1.0e-48 --randomizePSD=0.3 --pixelRefinement=2 LISACode/Background.xml' % globals())
# run('%(mydir)s/makeTDInoise-synthlisa2.py --keyOnly --seed=%(seed)s --randomizeNoise=0.2 --laserNoise=10 LISACode/Background-noise.xml' % globals())
# run('%(mydir)s/mergeXML.py LISACode/Background-noise.xml LISACode/Background.xml' % globals())
# run('mv LISACode/Background-noise.xml LISACode/Background.xml')
