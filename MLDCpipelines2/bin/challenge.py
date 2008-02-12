#!/usr/bin/env python

__version__ = '$Id: challenge2.py 415 2007-06-14 22:47:09Z vallisneri $'

# some definitions...

import sys
import os
import shutil
import glob
import re
import time
from distutils.dep_util import newer, newer_group

import lisaxml
import numpy

def makefromtemplate(output,template,**kwargs):
    fi = open(template,'r')
    fo = open(output,'w')
        
    for line in fi:
        repline = line
        
        for kw in kwargs:
            repline = re.sub('{' + kw + '}',str(kwargs[kw]),repline)
        
        print >> fo, repline,

def timestring(lapse):
    hrs  = int(lapse/3600)
    mins = int((lapse - hrs*3600)/60)
    secs = int((lapse - hrs*3600 - mins*60))
    
    return "%sh%sm%ss" % (hrs,mins,secs)

def run(command,quiet = False):
    commandline = command % globals()
    
    try:
        if not quiet:
            print "--> %s" % commandline
            assert(os.system(commandline) == 0)
        else:
            assert(os.system(commandline + ' > /dev/null') == 0)
    except:
        print 'Script %s failed at command "%s".' % (sys.argv[0],commandline)
        sys.exit(1)

try:
    import subprocess
    
    class parallelrun(object):
        def __init__(self,np=1):
            self.nproc = np
            
            self.queue = []
            self.slots = [None] * 4
        
        def submit(self,command,quiet=True):
            self.queue.append((command % globals(),quiet))
            pass
        
        def run(self):
            jobs = len(self.queue)
            fail = 0
            
            while True:
                idle = 0
                
                for cpu in range(self.nproc):
                    # first clear up ended processes if any
                    if self.slots[cpu]:
                        proc, quiet, command = self.slots[cpu]
                        ret = proc.poll()
                        
                        if ret != None:
                            if ret == 0:
                                if not quiet:
                                    print "--:> CPU [%d] finished." % cpu
                            else:
                                print 'Script %s failed on return from parallel command "%s".' % (sys.argv[0],command)
                                fail += 1
                            
                            self.slots[cpu] = None
                    
                    # now run another process if we have a free slot        
                    if not self.slots[cpu]:
                        if self.queue:
                            command, quiet = self.queue[0]
                            
                            try:
                                # stdout=subprocess.PIPE
                                if quiet:
                                    self.slots[cpu] = (subprocess.Popen(command,stdin=None,stdout=open('/dev/null','w'),stderr=subprocess.STDOUT,shell=True),quiet,command)    
                                else:
                                    self.slots[cpu] = (subprocess.Popen(command,stdin=None,stdout=None,stderr=subprocess.STDOUT,shell=True),quiet,command)
                            except:
                                print 'Script %s failed running parallel command "%s".' % (sys.argv[0],command)
                                fail += 1
                            else:
                                print "--:> CPU [%d]: %s" % (cpu,command)
                            
                            del self.queue[0]
                        else:
                            idle += 1
                
                if idle == self.nproc:
                    if fail == 0:
                        print "--:> Parallel run completed (%d jobs)." % jobs
                        return
                    else:
                        print "--:> Parallel run reported %d/%d failures!" % (fail,jobs)
                        sys.exit(1)
                else:
                    time.sleep(1.0)
        
    
    candopar = True
except:
    candopar = False

step0time = time.time()

# ---------------------------------------
# STEP 0: parse parameters, set constants
# ---------------------------------------

oneyear = 31457280

from optparse import OptionParser

# note that correct management of the Id string requires issuing the command
# svn propset svn:keywords Id FILENAME

parser = OptionParser(usage="usage: %prog [options] CHALLENGENAME...",
                      version="$Id: challenge2.py 415 2007-06-14 22:47:09Z vallisneri $")

parser.add_option("-t", "--training",
                  action="store_true", dest="istraining", default=False,
                  help="include source information in output file [off by default]")

# note that the default value is handled below
parser.add_option("-T", "--duration",
                  type="float", dest="duration", default=None,
                  help="total time for TDI observables (s) [default 62914560 = 2^22 * 15]")

parser.add_option("-d", "--timeStep",
                  type="float", dest="timestep", default=15.0,
                  help="timestep for TDI observable sampling (s) [default 15]")

parser.add_option("-s", "--seed",
                  type="int", dest="seed", default=None,
                  help="seed for random number generator (int) [required]")

parser.add_option("-n", "--seedNoise",
                  type="int", dest="seednoise", default=None,
                  help="noise-specific seed (int) [will use global seed (-s/--seed) if not given]")

parser.add_option("-N","--noNoise",
                  action="store_true", dest="nonoise", default=False,
                  help="do not include noise [default False]")

parser.add_option("-m", "--make",
                  action="store_true", dest="makemode", default=False,
                  help="run in make mode (use already-generated source key files and intermediate products; needs same seeds)")

parser.add_option("-S", "--synthlisa",
                  action="store_true", dest="synthlisaonly", default=False,
                  help="run only Synthetic LISA")

parser.add_option("-L", "--lisasim",
                  action="store_true", dest="lisasimonly", default=False,
                  help="run only the LISA Simulator")

parser.add_option("-C", "--lisacode",
                  action="store_true", dest="lisacodeonly", default=False,
                  help="run only lisacode, if challenge supports it [off by default]")

parser.add_option("-f", "--keyFile",
                  type="string", dest="sourcekeyfile", default=None,
                  help="get source descriptions from key file, not CHALLENGE.py script")

parser.add_option("-r", "--rawMeasurements",
                  action="store_true", dest="rawMeasurements", default=False,
                  help="output raw phase measurements (y's and z's) in addition to TDI X, Y, Z [synthlisa and lisacode only]")

parser.add_option("-R", "--randomizeNoise",
                  type="float", dest="randomizeNoise", default=0.0,
                  help="randomize level of noises [e.g., 0.2 for 20% randomization; defaults to 0; synthlisa and lisacode only]")

parser.add_option("-l", "--laserNoise",
                  type="string", dest="laserNoise", default='None',
                  help="laser noise level: None, Standard, <numerical value> [e.g., 0.2 for 20% of pm noise at 1 mHz; defaults to None; synthlisa and lisacode only]")

parser.add_option("-c", "--combinedSNR",
                  action="store_true", dest="combinedSNR", default=False,
                  help="use combined SNR = sqrt(2)*max{SNR_x, SNR_y, SNR_z} to rescale signals [off by default]")

parser.add_option("-P", "--nProc",
                  type="int", dest="nproc", default=1,
                  help="run in parallel on nproc CPUs [default 1]")

(options, args) = parser.parse_args()

if len(args) < 1:
    parser.error("I need the challenge name!")

challengename = args[0]

if options.seed == None:
    parser.error("You must specify the seed!")

# this sets the default dataset duration if not specified with special cases:
# - one year for challenge 1 except 1.3.X EMRIs (two years)
# - 24 days for challenges 3.4 and 3.5

if options.duration == None:
    if ('challenge1' in challengename) and (not 'challenge1.3' in challengename):
        print("--> It looks like you're generating a challenge-1 dataset, so I will set the duration to 2^21 * 15 (you can override this with -T).")
        options.duration = oneyear
    elif ('challenge3.4' in challengename) or ('challenge3.5' in challengename):
        options.duration = 2**21
        options.timestep = 1.0
    else:
        options.duration = 2.0*oneyear

# decide which simulators to use

dosynthlisa = not (                         options.lisasimonly or options.lisacodeonly) or options.synthlisaonly
dolisasim   = not (options.synthlisaonly                        or options.lisacodeonly) or options.lisasimonly
dolisacode  = not (options.synthlisaonly or options.lisasimonly                        ) or options.lisacodeonly

if dolisasim:
    # see if the duration is allowed by lisasim
    import lisasimulator
    
    if options.duration == 31457280:
        lisasimdir = lisasimulator.lisasim1yr
    elif options.duration == 62914560:
        lisasimdir = lisasimulator.lisasim2yr
    else:
        parser.error("I can only run the LISA Simulator for one or two years (2^21 or 2^22 s)!")
    
    if options.randomizeNoise > 0.0:
        parser.error("Option --randomizeNoise is not supported with the LISA Simulator. Run with --synthlisa.")
    if options.laserNoise != 'None':
        parser.error("Option --laserNoise is not supported with the LISA Simulator. Run with --synthlisa.")
    if options.rawMeasurements == True:
        parser.error("Option --rawMeasurement is not supported with The LISA Simulator. Run with --synthlisa.")

seed = options.seed

if options.seednoise == None:
    seednoise = seed
else:
    seednoise = options.seednoise

# setup for parallel run

if options.nproc > 1 and candopar:
    pengine = parallelrun(options.nproc)
    prun  = pengine.submit
    pwait = pengine.run
else:
    prun = run
    pwait = lambda : None

timestep = options.timestep
duration = options.duration
makemode = options.makemode
istraining = options.istraining
donoise = not options.nonoise

sourcekeyfile = options.sourcekeyfile

# -----------------------------------------------------------------------------------
# STEP 0: see where we're running from, and create directories if they're not present
# -----------------------------------------------------------------------------------

execdir = os.path.dirname(os.path.abspath(sys.argv[0]))
workdir = os.path.curdir

for folder in ('Barycentric','Dataset','Galaxy','Source','TDI','Template','Immediate','LISACode'):
    if not os.path.exists(folder):
        os.mkdir(folder)

for doc in ('Template/lisa-xml.css','Template/lisa-xml.xsl','Template/StandardLISA.xml','Template/LISACode.xml'):
    if not os.path.exists(doc):
        shutil.copyfile(execdir + '/../' + doc,doc)

# --------------------------------------------------------------------------------------
# STEP 1: create source XML parameter files, and collect all of them in Source directory
# --------------------------------------------------------------------------------------

step1time = time.time()

# first empty the Source and Galaxy directories
# makemode won't regenerate sources, so at least step 1 must be completed
# before running in make mode

if (not makemode) and (not sourcekeyfile):
    run('rm -f Source/*.xml')
    run('rm -f Galaxy/*.xml Galaxy/*.dat')
    run('rm -f Immediate/*.xml')
    run('rm -f LISACode/*.xml')
    
    # to run CHALLENGE, a file source-parameter generation script CHALLENGE.py must sit in bin/
    # it must take a single argument (the seed) and put its results in the Source subdirectory
    # if makesource-Galaxy.py is called, it must be with the UNALTERED seed, which is used
    # later to call makeTDIsignals-Galaxy.py
    
    # look for the script relative to challenge.py
    
    sourcescript = execdir + '/' + challengename + '.py'
    if not os.path.isfile(sourcescript):
        parser.error("I need the challenge script %s!" % sourcescript)
    else:
        run('python %s %s %s %s' % (sourcescript,seed,istraining,options.nproc))
elif (not makemode) and sourcekeyfile:
    # convoluted logic, to make sure that the keyfile given is not in Source/ and therefore deleted
    # TO DO: improve
    
    run('mv %s %s-backup' % (sourcekeyfile,sourcekeyfile))
    run('rm -f Source/*.xml')
    run('rm -f Galaxy/*.xml Galaxy/*.dat')
    run('mv %s-backup %s' % (sourcekeyfile,sourcekeyfile))
    
    run('cp %s Source/%s' % (sourcekeyfile,os.path.basename(sourcekeyfile)))

step2time = time.time()

# check if any of the source files requests an SN; in this case we'll need to run synthlisa!

if os.system('grep -qs RequestSN Source/*.xml') == 0 and dosynthlisa == False:
    parser.error("Ah, at least one of your Source files has a RequestSN field; then I MUST run synthlisa to adjust SNRs.")

# ---------------------------------------
# STEP 2: create barycentric strain files
# ---------------------------------------

# first empty the Barycentric directory
if (not makemode):
    run('rm -f Barycentric/*.xml Barycentric/*.bin')

# then run makebarycentric over all the files in the Source directory
for xmlfile in glob.glob('Source/*.xml'):
    baryfile = 'Barycentric/' + re.sub('\.xml$','-barycentric.xml',os.path.basename(xmlfile))
    if (not makemode) or newer(xmlfile,baryfile):
        prun('%(execdir)s/makebarycentric.py --duration=%(duration)s --timeStep=%(timestep)s %(xmlfile)s %(baryfile)s')

pwait()

step3time = time.time()

# --------------------------
# STEP 3: run Synthetic LISA
# --------------------------

# first empty the TDI directory
if (not makemode):
    run('rm -f TDI/*.xml TDI/*.bin TDI/Binary TDI/*.txt')

# make noise options (will be used also by LISACode later)

noiseoptions = ''
if options.randomizeNoise > 0.0:
    noiseoptions += ('--randomizeNoise=%s ' % options.randomizeNoise)
if options.laserNoise != 'None':
    noiseoptions += ('--laserNoise=%s ' % options.laserNoise)
if options.rawMeasurements == True:
    noiseoptions += '--rawMeasurements '

if dosynthlisa:
    # then run makeTDI-synthlisa over all the barycentric files in the Barycentric directory
    # the results are files of TDI time series that include the source info
    # these calls have also the result of rescaling the barycentric files to satisfy any RequestSN that
    # they may carry
    
    # for safety: use new makeTDIsignal-synthlisa.py only if we have immediate sources
    if glob.glob('Immediate/*.xml') or (options.rawMeasurements == True):
        runfile = 'makeTDIsignal-synthlisa2.py'
    else:
        runfile = 'makeTDIsignal-synthlisa.py'

    runoptions = ''    
    if options.rawMeasurements == True:
        runoptions += '--rawMeasurements '
    if options.combinedSNR == True:
        runoptions += '--combinedSNR '
    if not glob.glob('Galaxy/*.xml'):
        # if we are not generating a Galaxy, don't include its confusion noise in the evaluation of SNR
        runoptions += '--noiseOnly '
    
    for xmlfile in glob.glob('Barycentric/*-barycentric.xml') + glob.glob('Immediate/*.xml'):
        if 'barycentric.xml' in xmlfile:
            tdifile = 'TDI/' + re.sub('barycentric\.xml$','tdi-frequency.xml',os.path.basename(xmlfile))
        else:
            tdifile = 'TDI/' + re.sub('\.xml$','-tdi-frequency.xml',os.path.basename(xmlfile))

        if (not makemode) or newer(xmlfile,tdifile):
            prun('%(execdir)s/%(runfile)s %(runoptions)s --duration=%(duration)s --timeStep=%(timestep)s %(xmlfile)s %(tdifile)s')

    if noiseoptions or (timestep != 15.0):
        runfile = 'makeTDInoise-synthlisa2.py'
    else:
        runfile = 'makeTDInoise-synthlisa.py'

    # now run noise generation... note that we're not checking whether the seed is the correct one
    noisefile = 'TDI/tdi-frequency-noise.xml'
        
    if donoise and ((not makemode) or (not os.path.isfile(noisefile))):        
        prun('%(execdir)s/%(runfile)s --seed=%(seednoise)s --duration=%(duration)s --timeStep=%(timestep)s %(noiseoptions)s %(noisefile)s')

    pwait()

step4time = time.time()

# --------------------------
# STEP 4: run LISA Simulator
# --------------------------

if dolisasim:
    for xmlfile in glob.glob('Immediate/*.xml'):
        print "--> !!! Ignoring immediate synthlisa file %s" % xmlfile

    for xmlfile in glob.glob('Barycentric/*-barycentric.xml'):
        tdifile = 'TDI/' + re.sub('\-barycentric.xml','-tdi-strain.xml',os.path.basename(xmlfile))

        if (not makemode) or newer(xmlfile,tdifile):
            # remember that we're not doing any SNR adjusting here...
            prun('%(execdir)s/makeTDIsignal-lisasim.py --lisasimDir=%(lisasimdir)s %(xmlfile)s %(tdifile)s')
    
    slnoisefile = 'TDI/tdi-strain-noise.xml'

    if donoise and ((not makemode) or (not os.path.isfile(slnoisefile))):
        prun('%(execdir)s/makeTDInoise-lisasim.py --lisasimDir=%(lisasimdir)s --seedNoise=%(seednoise)s %(slnoisefile)s')
    
    pwait()

step4btime = time.time()

# ---------------------
# STEP 4b: run LISAcode
# ---------------------

if dolisacode and glob.glob('LISACode/source-*.xml'):
    # make the standard lisacode instruction set
    cname = istraining and (challengename + '-training') or challengename
    endian = (sys.byteorder == 'little') and 'LittleEndian' or 'BigEndian'
    
    # (use same seed as synthlisa if we're training)
    lcseednoise = istraining and seednoise or (seednoise + 1)
    
    makefromtemplate('LISACode/%s-lisacode-input.xml' % cname,'%s/../Template/LISACode.xml' % execdir,
                     challengename=cname,
                     cadence=timestep,
                     duration=duration,
                     randomseed=lcseednoise,
                     endianness=endian)
    
    # make lisacode noise (note that the random seed is really set above in the standard instruction set)
    run('%(execdir)s/makeTDInoise-synthlisa2.py --keyOnly --seed=%(lcseednoise)s --duration=%(duration)s --timeStep=%(timestep)s %(noiseoptions)s LISACode/noise.xml')
    
    # merge with source data into a single lisacode input file
    run('%(execdir)s/mergeXML.py LISACode/%(cname)s-lisacode-input.xml LISACode/noise.xml LISACode/source-*.xml')
    run('rm LISACode/noise.xml LISACode/source-*.xml')
    
    os.chdir('LISACode')
    
    import lisacode    
    run('%s %s-lisacode-input.xml' % (lisacode.lisacode,challengename))
    
    os.chdir('..')

step5time = time.time()

# -----------------------
# STEP 5: run Fast Galaxy
# -----------------------

# should avoid running both SL and LS if only one is requested...

if glob.glob('Galaxy/*.xml'):
    for xmlfile in glob.glob('Galaxy/*.xml'):
        sltdifile = 'TDI/' + re.sub('.xml','-tdi-frequency.xml',os.path.basename(xmlfile))
        lstdifile = 'TDI/' + re.sub('.xml','-tdi-strain.xml',os.path.basename(xmlfile))

        if (not makemode) or (newer(xmlfile,sltdifile) or newer(xmlfile,lstdifile)):
            prun('%s/makeTDIsignals-Galaxy3.py %s %s %s' % (execdir,xmlfile,sltdifile,lstdifile))

    pwait()

step6time = time.time()

# -------------------------
# STEP 6: assemble datasets
# -------------------------

# run('rm -f Dataset/*.xml Dataset/*.bin Dataset/*.txt')

# improve dataset metadata here

# do synthlisa first

secretseed = ', source seed = %s, noise seed = %s' % (seed,seednoise)

if istraining:
    globalseed = secretseed
else:
    globalseed = ''

# add SVN revision number

import lisatoolsrevision
lisatoolsrev = ', LISAtools SVN revision %s' % lisatoolsrevision.lisatoolsrevision

secretseed += lisatoolsrev
globalseed += lisatoolsrev

run('cp Template/lisa-xml.xsl Dataset/.')
run('cp Template/lisa-xml.css Dataset/.')

if dosynthlisa:
    # set filenames

    if istraining:
        nonoisefile   = 'Dataset/' + challengename + '-training-nonoise-frequency.xml'
        withnoisefile = 'Dataset/' + challengename + '-training-frequency.xml'

        nonoisetar    = challengename + '-training-nonoise-frequency.tar.gz'
        withnoisetar  = challengename + '-training-frequency.tar.gz'

        keyfile       = 'Dataset/' + challengename + '-training-key.xml'
    else:
        nonoisefile   = 'Dataset/' + challengename + '-nonoise-frequency.xml'
        withnoisefile = 'Dataset/' + challengename + '-frequency.xml'

        nonoisetar    = challengename + '-nonoise-frequency.tar.gz'
        withnoisetar  = challengename + '-frequency.tar.gz'

        keyfile       = 'Dataset/' + challengename + '-key.xml'

    # create empty files

    lisaxml.lisaXML(nonoisefile,
                    author="MLDC Task Force",
                    comments='No-noise dataset for %s (synthlisa version)%s' % (challengename,globalseed)).close()

    lisaxml.lisaXML(withnoisefile,
                    author="MLDC Task Force",
                    comments='Full dataset for %s (synthlisa version)%s' % (challengename,globalseed)).close()

    lisaxml.lisaXML(keyfile,
                    author="MLDC Task Force",
                    comments='XML key for %s%s' % (challengename,secretseed)).close()

    # add signals and noise to the no-noise and with-noise files
    # omit keys if we're not doing training

    if istraining:
        if glob.glob('TDI/*-tdi-frequency.xml'):
            run('%(execdir)s/mergeXML.py --tdiName=%(challengename)s %(nonoisefile)s TDI/*-tdi-frequency.xml')

        if donoise:
            run('%(execdir)s/mergeXML.py --tdiName=%(challengename)s %(withnoisefile)s %(nonoisefile)s %(noisefile)s')
    else:
        if glob.glob('TDI/*-tdi-frequency.xml'):
            run('%(execdir)s/mergeXML.py --noKey --tdiName=%(challengename)s %(nonoisefile)s TDI/*-tdi-frequency.xml')

        if donoise:
            run('%(execdir)s/mergeXML.py --noKey --tdiName=%(challengename)s %(withnoisefile)s %(nonoisefile)s %(noisefile)s')

    # create the key with all source info

    if glob.glob('TDI/*-tdi-frequency.xml'):
        run('%(execdir)s/mergeXML.py --keyOnly %(keyfile)s TDI/*-tdi-frequency.xml')

    # now do some tarring up, including XSL and CSS files from Template

    os.chdir('Dataset')

    nonoisefile   = os.path.basename(nonoisefile)
    withnoisefile = os.path.basename(withnoisefile)

    run('tar zcf %s %s %s lisa-xml.xsl lisa-xml.css' % (nonoisetar,  nonoisefile,  re.sub('\.xml','-[0-9].bin',nonoisefile  )))

    if donoise:
        run('tar zcf %s %s %s lisa-xml.xsl lisa-xml.css' % (withnoisetar,withnoisefile,re.sub('\.xml','-[0-9].bin',withnoisefile)))    

    os.chdir('..')

# next do LISACode

if dolisacode:
    for xmlfile in glob.glob('LISACode/*-lisacode.xml'):
        basefile = re.sub('LISACode/','',re.sub('\.xml','',xmlfile))
        
        withnoisefile = 'Dataset/' + basefile + '.xml'
        inputfile = 'LISACode/' + basefile + '-input.xml'
        
        # prepare empty file
        lisaxml.lisaXML(withnoisefile,
                        author="MLDC Task Force",
                        comments='Full dataset for %s (lisacode version)%s' % (challengename,globalseed)).close()
        
        # merge dataset with key (if training), otherwise copy dataset, renaming binaries
        if istraining:
            run('%(execdir)s/mergeXML.py %(withnoisefile)s %(xmlfile)s %(inputfile)s')
        else:
            run('%(execdir)s/mergeXML.py %(withnoisefile)s %(xmlfile)s')
        
        # also, get key file
        run('cp %s %s' % (inputfile,'Dataset/' + basefile + '-key.xml'))
        
        # make tar
        os.chdir('Dataset')
        
        withnoisefile = os.path.basename(withnoisefile)
        withnoisetar = re.sub('.xml','.tar.gz',withnoisefile)
        
        run('tar zcf %s %s %s lisa-xml.xsl lisa-xml.css' % (withnoisetar,withnoisefile,re.sub('\.xml','-[0-9].bin',withnoisefile)))
        
        os.chdir('..')

# next do LISA Simulator

if dolisasim:
    # set filenames

    if istraining:
        nonoisefile   = 'Dataset/' + challengename + '-training-nonoise-strain.xml'
        withnoisefile = 'Dataset/' + challengename + '-training-strain.xml'

        nonoisetar    = challengename + '-training-nonoise-strain.tar.gz'
        withnoisetar  = challengename + '-training-strain.tar.gz'

        keyfile       = 'Dataset/' + challengename + '-training-key.xml'
    else:
        nonoisefile   = 'Dataset/' + challengename + '-nonoise-strain.xml'
        withnoisefile = 'Dataset/' + challengename + '-strain.xml'

        nonoisetar    = challengename + '-nonoise-strain.tar.gz'
        withnoisetar  = challengename + '-strain.tar.gz'

        keyfile       = 'Dataset/' + challengename + '-key.xml'
    
    # create empty files
    
    lisaxml.lisaXML(nonoisefile,
                    author = 'MLDC task force',
                    comments='No-noise dataset for %s (lisasim version)%s' % (challengename,globalseed)).close()

    lisaxml.lisaXML(withnoisefile,
                    author = 'MLDC task force',
                    comments='Full dataset for %s (lisasim version)%s' % (challengename,globalseed)).close()

    # add signals and noise

    if istraining:
        if glob.glob('TDI/*-tdi-strain.xml'):
            run('%(execdir)s/mergeXML.py --tdiName=%(challengename)s %(nonoisefile)s TDI/*-tdi-strain.xml')

        if donoise:
            run('%(execdir)s/mergeXML.py --tdiName=%(challengename)s %(withnoisefile)s %(nonoisefile)s %(slnoisefile)s')
    else:
        if glob.glob('TDI/*-tdi-strain.xml'):
            run('%(execdir)s/mergeXML.py --noKey --tdiName=%(challengename)s %(nonoisefile)s TDI/*-tdi-strain.xml')
        
        if donoise:
            run('%(execdir)s/mergeXML.py --noKey --tdiName=%(challengename)s %(withnoisefile)s %(nonoisefile)s %(slnoisefile)s')

    # do key file, but only if synthlisa has not run, otherwise it will be erased

    if not dosynthlisa:
        lisaxml.lisaXML(keyfile,comments='XML key for %s%s' % (challengename,secretseed)).close()

        if glob.glob('TDI/*-tdi-strain.xml'):
            run('%(execdir)s/mergeXML.py --keyOnly %(keyfile)s TDI/*-tdi-strain.xml')

    # now do some tarring up, including XSL and CSS files from Template

    os.chdir('Dataset')

    nonoisefile   = os.path.basename(nonoisefile)
    withnoisefile = os.path.basename(withnoisefile)

    run('tar zcf %s %s %s lisa-xml.xsl lisa-xml.css' % (nonoisetar,  nonoisefile,  re.sub('\.xml','-[0-9].bin',nonoisefile  )))

    if donoise:
        run('tar zcf %s %s %s lisa-xml.xsl lisa-xml.css' % (withnoisetar,withnoisefile,re.sub('\.xml','-[0-9].bin',withnoisefile)))    

    os.chdir('..')

# make sure the keys point to real Table txt files, not symlinks
# build symlinks from training dataset tables to their key 

os.chdir('Dataset')

for txtfile in glob.glob('*.txt'):
    if os.path.islink(txtfile):
        if 'key' in txtfile:
            run('cp %s %s-tmp' % (txtfile,txtfile))
            run('mv %s-tmp %s' % (txtfile,txtfile))
        else:
            for filetype in ['training-frequency','training-strain','training-nonoise-frequency','training-nonoise-strain']:
                if filetype in txtfile:
                    run('rm %s' % txtfile)
                    run('ln -s %s %s' % (re.sub(filetype,'training-key',txtfile),txtfile))

os.chdir('..')

# run('rm Dataset/lisa-xml.xsl')
# run('rm Dataset/lisa-xml.css')

step7time = time.time()

# ------------------------
# STEP 7: package datasets
# ------------------------

endtime = time.time()

print
print "--> Completed generating %s" % challengename 
print "    Total time         : %s" % timestring(endtime    - step0time)
print "    Sources time       : %s" % timestring(step3time  - step1time)
print "    Synthetic LISA time: %s" % timestring(step4time  - step3time)
print "    LISA Simulator time: %s" % timestring(step4btime - step4time)
print "    LISACode time      : %s" % timestring(step5time  - step4btime)
print "    Fast Galaxy time   : %s" % timestring(step6time  - step5time)

# exit with success code
sys.exit(0)

# TO DO:
# - must add noise and geometry specification (do it with files in Template)
# - must do some instructions
