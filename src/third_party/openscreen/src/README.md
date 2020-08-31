# Open Screen Library

The openscreen library implements the Open Screen Protocol and the Chromecast
protocols (both control and streaming).

Information about the protocol and its specification can be found [on
GitHub](https://github.com/webscreens/openscreenprotocol).

# Getting the code

## Installing depot_tools

openscreen library dependencies are managed using `gclient`, from the
[depot_tools](https://www.chromium.org/developers/how-tos/depottools) repo.

To get gclient, run the following command in your terminal:
```bash
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```

Then add the `depot_tools` folder to your `PATH` environment variable.

Note that openscreen does not use other features of `depot_tools` like `repo` or
`drover`.  However, some `git-cl` functions *do* work, like `git cl try`, `git cl
lint` and `git cl upload.`

## Checking out code

From the parent directory of where you want the openscreen checkout (e.g.,
`~/my_project_dir`), configure `gclient` and check out openscreen with the
following commands:

```bash
    cd ~/my_project_dir
    gclient config https://chromium.googlesource.com/openscreen
    gclient sync
```

The first `gclient` command will create a default .gclient file in
`~/my_project_dir` that describes how to pull down the `openscreen` repository.
The second command creates an `openscreen/` subdirectory, downloads the source
code, all third-party dependencies, and the toolchain needed to build things;
and at their appropriate revisions.

## Syncing your local checkout

To update your local checkout from the openscreen master repository, just run

```bash
   cd ~/my_project_dir/openscreen
   git pull
   gclient sync
```

This will rebase any local commits on the remote top-of-tree, and update any
dependencies that have changed.

# Build setup

The following are the main tools are required for development/builds:

 - Build file generator: `gn`
 - Code formatter: `clang-format`
 - Builder: `ninja` ([GitHub releases](https://github.com/ninja-build/ninja/releases))
 - Compiler/Linker: `clang` (installed by default) or `gcc` (installed by you)

All of these--except `gcc` as noted above--are automatically downloaded/updated
for the Linux and Mac environments via `gclient sync` as described above. The
first two are installed into `buildtools/<platform>/`.

Mac only: XCode must be installed on the system, to link against its frameworks.

`clang-format` is used for maintaining consistent coding style, but it is not a
complete replacement for adhering to Chromium/Google C++ style (that's on you!).
The presubmit script will sanity-check that it has been run on all new/changed
code.

## Linux clang

On Linux, the build will automatically download the Clang compiler from the
Google storage cache, the same way that Chromium does it.

Ensure that libstdc++ 8 is installed, as clang depends on the system
instance of it. On Debian flavors, you can run:

```bash
   sudo apt-get install libstdc++-8-dev
```

## Linux gcc

Setting the `gn` argument "is_gcc=true" on Linux enables building using gcc
instead.

```bash
  gn gen out/Default --args="is_gcc=true"
```

Note that g++ version 7 or newer must be installed.  On Debian flavors you can
run:

```bash
  sudo apt-get install gcc-7
```

## Mac clang

On Mac OS X, the build will use the clang provided by
[XCode](https://apps.apple.com/us/app/xcode/id497799835?mt=12), which must be
installed.

## Debug build

Setting the `gn` argument "is_debug=true" enables debug build.

```bash
  gn gen out/Default --args="is_debug=true"
```

To install debug information for libstdc++ 8 on Debian flavors, you can run:

```bash
   sudo apt-get install libstdc++6-8-dbg
```

## gn configuration

Running `gn args` opens an editor that allows to create a list of arguments
passed to every invocation of `gn gen`.

```bash
  gn args out/Default
```

# Building targets

## Building the OSP demo

The following commands will build a sample executable and run it.

``` bash
  mkdir out/Default
  gn gen out/Default          # Creates the build directory and necessary ninja files
  ninja -C out/Default demo   # Builds the executable with ninja
  ./out/Default/demo          # Runs the executable
```

The `-C` argument to `ninja` works just like it does for GNU Make: it specifies
the working directory for the build.  So the same could be done as follows:

``` bash
  ./gn gen out/Default
  cd out/Default
  ninja
  ./demo
```

After editing a file, only `ninja` needs to be rerun, not `gn`.  If you have
edited a `BUILD.gn` file, `ninja` will re-run `gn` for you.

Unless you like to wait longer than necessary for builds to complete, run
`autoninja` instead of `ninja`, which takes the same command-line arguments.
This will automatically parallelize the build for your system, depending on
number of processor cores, RAM, etc.

For details on running `demo`, see its [README.md](demo/README.md).

## Building other targets

Running `ninja -C out/Default gn_all` will build all non-test targets in the
repository.

`gn ls --type=executable out/Default/` will list all of the executable targets
that can be built.

If you want to customize the build further, you can run `gn args out/Default` to
pull up an editor for build flags. `gn args --list out/Default` prints all of
the build flags available.

## Building and running unit tests

```bash
  ninja -C out/Default unittests
  ./out/Default/unittests
```

## Building and running fuzzers

In order to build fuzzers, you need the GN arg `use_libfuzzer=true`.  It's also
recommended to build with `is_asan=true` to catch additional problems.  Building
and running then might look like:
```bash
  gn gen out/libfuzzer --args="use_libfuzzer=true is_asan=true is_debug=false"
  ninja -C out/libfuzzer some_fuzz_target
  out/libfuzzer/some_fuzz_target <args> <corpus_dir> [additional corpus dirs]
```

The arguments to the fuzzer binary should be whatever is listed in the GN target
description (e.g. `-max_len=1500`).  These arguments may be automatically
scraped by Chromium's ClusterFuzz tool when it runs fuzzers, but they are not
built into the target.  You can also look at the file
`out/libfuzzer/some_fuzz_target.options` for what arguments should be used.  The
`corpus_dir` is listed as `seed_corpus` in the GN definition of the fuzzer
target.

# Continuous build and try jobs

openscreen uses [LUCI builders](https://ci.chromium.org/p/openscreen/builders)
to monitor the build and test health of the library.  Current builders include:

| Name                   | Arch   | OS                 | Toolchain | Build   | Notes        |
|------------------------|--------|--------------------|-----------|---------|--------------|
| linux64_debug          | x86-64 | Ubuntu Linux 16.04 | clang     | debug   | ASAN enabled |
| linux64_gcc_debug      | x86-64 | Ubuntu Linux 18.04 | gcc-7     | debug   | |
| linux64_tsan           | x86-64 | Ubuntu Linux 16.04 | clang     | release | TSAN enabled |
| mac_debug              | x86-64 | Mac OS X/Xcode     | clang     | debug   | |
| chromium_linux64_debug | x86-64 | Ubuntu Linux 16.04 | clang     | debug   | built within chromium |
| chromium_mac_debug     | x86-64 | Mac OS X/Xcode     | clang     | debug   | built within chromium |

You can run a patch through the try job queue (which tests it on all
non-chromium builders) using `git cl try`, or through Gerrit (details below).

The chromium builders compile openscreen HEAD vs. chromium HEAD.  They run as
experimental trybots and continuous-integration FYI bots.

# Submitting changes

openscreen library code should follow the [Open Screen Library Style
Guide](docs/style_guide.md).

openscreen uses [Chromium Gerrit](https://chromium-review.googlesource.com/) for
patch management and code review (for better or worse).

The following sections contain some tips about dealing with Gerrit for code
reviews, specifically when pushing patches for review, getting patches reviewed,
and committing patches.

## Uploading a patch for review

The `git cl` tool handles details of interacting with Gerrit (the Chromium code
review tool) and is recommended for pushing patches for review.  Once you have
committed changes locally, simply run:

```bash
  git cl format
  git cl upload
```

The first command will will auto-format the code changes. Then, the second
command runs the `PRESUBMIT.sh` script to check style and, if it passes, a
newcode review will be posted on `chromium-review.googlesource.com`.

If you make additional commits to your local branch, then running `git cl
upload` again in the same branch will merge those commits into the ongoing
review as a new patchset.

It's simplest to create a local git branch for each patch you want reviewed
separately.  `git cl` keeps track of review status separately for each local
branch.

## Addressing merge conflicts

If conflicting commits have been landed in the repository for a patch in review,
Gerrit will flag the patch as having a merge conflict.  In that case, use the
instructions above to rebase your commits on top-of-tree and upload a new
patchset with the merge conflicts resolved.

## Tryjobs

Clicking the `CQ DRY RUN` button (also, confusingly, labeled `COMMIT QUEUE +1`)
will run the current patchset through all LUCI builders and report the results.
It is always a good idea get a green tryjob on a patch before sending it for
review to avoid extra back-and-forth.

You can also run `git cl try` from the commandline to submit a tryjob.

## Code reviews

Send your patch to one or more committers in the
[COMMITTERS](https://chromium.googlesource.com/openscreen/+/refs/heads/master/COMMITTERS)
file for code review.  All patches must receive at least one LGTM by a committer
before it can be submitted.

## Submission

After your patch has received one or more LGTM commit it by clicking the
`SUBMIT` button (or, confusingly, `COMMIT QUEUE +2`) in Gerrit.  This will run
your patch through the builders again before committing to the main openscreen
repository.

<!-- TODO(mfoltz): split up README.md into more manageable files. -->
## Working with ARM/ARM64/the Raspberry PI

openscreen supports cross compilation for both arm32 and arm64 platforms, by
using the `gn args` parameter `target_cpu="arm"` or `target_cpu="arm64"`
respectively. Note that quotes are required around the target arch value.

Setting an arm(64) target_cpu causes GN to pull down a sysroot from openscreen's
public cloud storage bucket. Google employees may update the sysroots stored
by requesting access to the Open Screen pantheon project and uploading a new
tar.xz to the openscreen-sysroots bucket.

NOTE: The "arm" image is taken from Chromium's debian arm image, however it has
been manually patched to include support for libavcodec and libsdl2. To update
this image, the new image must be manually patched to include the necessary
header and library dependencies. Note that if the versions of libavcodec and
libsdl2 are too out of sync from the copies in the sysroot, compilation will
succeed, but you may experience issues decoding content.

To install the last known good version of the libavcodec and libsdl packages
on a Raspberry Pi, you can run the following command:

```bash
sudo ./cast/standalone_receiver/install_demo_deps_raspian.sh
```

NOTE: until [Issue 106](http://crbug.com/openscreen/106) is resolved, you may
experience issues streaming to a Raspberry Pi if multiple network interfaces
(e.g. WiFi + Ethernet) are enabled. The workaround is to disable either the WiFi
or ethernet connection.

## Code Coverage

Code coverage can be checked using clang's source-based coverage tools.  You
must use the GN argument `use_coverage=true`.  It's recommended to do this in a
separate output directory since the added instrumentation will affect
performance and generate an output file every time a binary is run.  You can
read more about this in [clang's
documentation](http://clang.llvm.org/docs/SourceBasedCodeCoverage.html) but the
bare minimum steps are also outlined below.  You will also need to download the
pre-built clang coverage tools, which are not downloaded by default.  The
easiest way to do this is to set a custom variable in your `.gclient` file.
Under the "openscreen" solution, add:
```python
  "custom_vars": {
    "checkout_clang_coverage_tools": True,
  },
```
then run `gclient runhooks`.  You can also run the python command from the
`clang_coverage_tools` hook in `//DEPS` yourself or even download the tools
manually
([link](https://storage.googleapis.com/chromium-browser-clang-staging/)).

Once you have your GN directory (we'll call it `out/coverage`) and have
downloaded the tools, do the following to generate an HTML coverage report:
```bash
out/coverage/openscreen_unittests
third_party/llvm-build/Release+Asserts/bin/llvm-profdata merge -sparse default.profraw -o foo.profdata
third_party/llvm-build/Release+Asserts/bin/llvm-cov show out/coverage/openscreen_unittests -instr-profile=foo.profdata -format=html -output-dir=<out dir> [filter paths]
```
There are a few things to note here:
 - `default.profraw` is generated by running the instrumented code, but
 `foo.profdata` can be any path you want.
 - `<out dir>` should be an empty directory for placing the generated HTML
 files.  You can view the report at `<out dir>/index.html`.
 - `[filter paths]` is a list of paths to which you want to limit the coverage
 report.  For example, you may want to limit it to cast/ or even
 cast/streaming/.  If this list is empty, all data will be in the report.

The same process can be used to check the coverage of a fuzzer's corpus.  Just
add `-runs=0` to the fuzzer arguments to make sure it only runs the existing
corpus then exits.
