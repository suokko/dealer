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

Required dependencies are
* cmake (>= 3.1)
* Compiler with c++14 support

Required dependencies building from sources tarball:
* libdds
* pcg-cpp

Recommended dependencies for development:
* perl and module IPC::Run
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

5. Development

If you want to write code or debug issues then it is best to set
CMAKE_BUILD_TYPE to RelWithDebInfo or Debug. If you use RelWithDebInfo then I
would recommend removing NDEBUG from CMAKE_CXX_FLAGS_RELWITHDEBINFO. Changing
variables can be done using cmake user interface tools. Curses variant can be
executed in a build directory like `ccmake ..`.

Other helpful flags include adding -Wpedantic and -Wall to CMAKE_CXX_FLAGS.
These gcc flags or similar for your compiler makes compiler warn about common
programming errors which are likely to cause bugs.

Build system offers build targets to run test in different manners:
* `check` runs test suit once for the runtime detected optimization configuration
* `check_failed` runs only test cases which failed in the previous `check` or
  `check_failed` test run
* `check_all` runs test for all compiled optimization configurations
* `check_all_failed` runs failed tests from all configurations
* `check_coverage` builds and runs coverage instrument version and generates and
  html coverage report. This target requires optional tools for your compiler.
  (eg. gcov, lcov and genhtml for gcc)
* `check_coverage_all` is like `check_coverage` but runs test for all runtime
  configurations.

Asserts are written to indicate internal coding error. They are supposed to
be impossible to trigger by any user input. But in practice bugs like incorrect
state transitions or missing user input validation can lead to assert failure.
Asserts are early warning indicating that follow up executions would have likely
resulted to crash or garbage output. An early assert failure after a bug can
make debugging the issue mush easier.

Old asserts have only condition without explanations. Often condition is self
explanatory for someone familiar with the code. I decided to start add
explanation strings to new asserts because a common case is assert failure
happening for an user without understanding the code.

6. Packaging

The build system supports installation components `Runtime`, `Development` and
`Docs`. Each target is expected to include files related to a specific type of
distribution package.

Example use to installation to multiple packages. Commands are run in the build
directory.
```
DESTDIR=../debian/tmp cmake -DCOMPONENT=Runtime -P cmake_install.cmake
DESTDIR=../debian/dev cmake -DCOMPONENT=Development -P cmake_install.cmake
DESTDIR=../debian/docs cmake -DCOMPONENT=Docs -P cmake_install.cmake
````

Build system offers packaging support using cpack. Supported targets are:
* `package_source` Generates source tarballs (default is .tar.gz and .zip)
