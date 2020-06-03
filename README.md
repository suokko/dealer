This program is hereby put in the public domain. Do with it whatever
you want, but I would like you not to redistribute it in modified form
without mentioning the fact of modification. I will accept bug reports
and modification requests, without any obligation of course, but fixing
bugs someone else put in is beyond me.

When you report bugs please mention the version number in the source
files, and preferably send context diffs if you changed anything.
I might put in your fixes, and distribute a new version someday.

I would prefer if you did *not* use this program for generating hands
for tournaments. I have not investigated the random number generation
closely enough for me to be comfortable with that thought.

	Hans van Staveren
	Amsterdam
	Holland
	<sater@xs4all.nl>

------------------------------------------------------------------------------
This is a version of Hans' program with several new functions.   Please
look in the Manual directory for a description.

        Henk Uijterwaal
        Amsterdam
        Holland
        henk@ripe.net

------------------------------------------------------------------------------
This is modification based on Henk's version with several improvements. The
prefered bug report method is github bug tracking but email bugs will beyond
checked too.

	https://github.com/suokko/dealer/issues

	Pauli Nieminen <suokkos@gmail.com>
------------------------------------------------------------------------------

1. Dependencies

Required dependecies are
* cmake (>= 3.12)
* Compiler with c and c++11 support

Recommended dependencies for development:
* perl
* lcov if compiling with gcc
* genhtml if compiling with gcc

Recommended run time dependencies are
* Bo's dds [http://privat.bahnhof.se/wb758135/](http://privat.bahnhof.se/wb758135/)
* dds-driver for dealergenlib

libdds.so, libdds.dylib or dds.dll needs to be in runtime library search path to
use double dummy solver in dealer scripts.

2.  Building from sources

```
mkdir -p build
cd build
cmake -G<Your favorite generator> ..
```

Then you can compile sources using your selected build system.

Project settings can be modified using cmake GUI. (eg. `ccmake ..`)

3. Testing

Build system adds two testing targets. `check` target runs test using binary
compiled with flags for configuration. `check_coverage` runs same set of tests
using specially instrumented coverage binary. After running test build system
runs tools to create html report. The report is automatically opened in browser.

```
mkdir -p build
cd build
cmake ..
make check_coverage
```

4. Using dealer

Dealer uses a simple scripting language to allow user place conditions to
generate hands, print hands and do statistical analyze. Example use would be to
analyze number of control points in balanced hand with 20-22 HCP.

```
./dealer ../Examples/Descr.controls
```

The command use a source tree provide example script with limits and histogram
printing action. Example output from the dealer command looks like

```
Frequency controls:
    4	      16
    5	     312
    6	    1616
    7	    3518
    8	    3476
    9	     988
   10	      74
Generated 1553663 hands
Produced 10000 hands
Initial random seed 1591209597
Time needed    0.426 sec
```

More details about scripting language can be found from
Manual/index.html.
