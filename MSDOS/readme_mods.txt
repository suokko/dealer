From:  Paul Baxter

email: paje@globalnet.co.uk


PURPOSE

Put together a few batch files for the various versions I have.

dealer.exe    - cygwin release version  Use cygwin.bat
mdealer.exe   - MSDOS release version	  Use dostest.bat
mddealer.exe  - MSDOS debug version     Use ddostest.bat

Copy any of the Descr.* example files into this (MSDOS) directory.
The batch files will run through each Descr.* file in turn.

I thought this would be a crude initial step towards ensuring
new versions produce same results as old, or that different computer
systems produce similar results.

Maybe use this test dir with examples and appropriate results to 'validate'
new versions.

I would prefer that the tests were in a separate dir to the MSVC project files
but that's up to Henk. When I refer to the 'test' dir it is currently
dealer/MSDOS dir



USAGE
   
For the dos programs I have batch files dostest and ddostest (extra d for debug)
Similarly for cygtest
   
   Put any Descr*.* input files in the test dir
   
   cd to it
   
FROM DOS PROMPT

type:
 
   dostest
   
to run all files through mdealer (o/p to stdout)

or type:

   command.com /c dostest >file.log

to redirect to a file. (Output will not be visible)
(Have to use a new dos shell in order to redirect the output of a batch file)
   
similarly use 'ddostest' instead of 'dostest' to exercise the debug DOS version
   
   
These batch files assume executables are one dir higher than test dir
If you have put the executables on your Path or have renamed the files
edit the batch files as appropriate
   
   
Also try with cygtest (must have cygwin dll in path etc.)

	cygtest   
   
Results of the three log files appear similar in probabilities etc.
So maybe the Dos version can give your analysis a 30% boost in performance!
   

FROM CYGWIN SHELL PROMPT

Will need to explicitly add .bat and possibly begin command with ./
to indicate the file is in current dir

type:

    ./dostest.bat
or
    command.com /c dostest.bat >file.log

PROBLEMS   

See my notes in main dir regarding problem with Descr.Stayman in
debug version
   
SPEED COMPARISON

Release DOS executable seems about ~50-80% faster than Cygwin ver
Debug DOS is about 50% slower than Cygwin
Mileage may vary
   
(Descr.blue_team modified to run a bit longer)
   
Times: 12 sec on Win release
       20 sec on Cygwin
       30 sec on Win debug

CONCLUSIONS

It would be useful to collect a few simulation files to do the following:

1) Exercise all features of dealer
2) Generate some standard sims where results are known. Can check that dealer
   gets the answers right.
