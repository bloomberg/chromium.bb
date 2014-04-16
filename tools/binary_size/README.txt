================================================================================
              __________  .__
              \______   \ |__|   ____   _____    _______   ___.__.
               |    |  _/ |  |  /    \  \__  \   \_  __ \ <   |  |
               |    |   \ |  | |   |  \  / __ \_  |  | \/  \___  |
               |______  / |__| |___|  / (____  /  |__|     / ____|
                      \/            \/       \/            \/
    _________ .__                        ___________                   .__
   /   _____/ |__| ________   ____       \__    ___/   ____     ____   |  |
   \_____  \  |  | \___   / _/ __ \        |    |     /  _ \   /  _ \  |  |
   /        \ |  |  /    /  \  ___/        |    |    (  <_> ) (  <_> ) |  |__
  /_______  / |__| /_____ \  \___  >       |____|     \____/   \____/  |____/
          \/             \/      \/
================================================================================

--------------------------------------------------------------------------------
Introduction
--------------------------------------------------------------------------------
The ever-increasing size of binaries is a problem for everybody. Increased
binary size means longer download times and a bigger on-disk footprint after
installation. Mobile devices suffer the worst, as they frequently have
sub-optimal connectivity and limited storage capacity. Developers currently
have almost no visibility into how the space in the existing binaries is
divided nor how their contributions change the space within those binaries.
The first step to reducing the size of binaries is to make the size information
accessible to everyone so that developers can take action.

The Binary Size Tool does the following:
  1. Runs "nm" on a specified binary to dump the symbol table
  2. Runs a parallel pool of "addr2line" processes to map the symbols from the
     symbol table back to source code (way faster than running "nm -l")
  3. Creates (and optionally saves) an intermediate file that accurately mimcs
     (binary compatible with) the "nm" output format, with all the source code
     mappings present
  4. Parses, sorts and analyzes the results
  5. Generates an HTML-based report in the specified output directory


--------------------------------------------------------------------------------
How to Run
--------------------------------------------------------------------------------
Running the tool is fairly simply. For the sake of this example we will
pretend that you are building the Content Shell APK for Android.

  1. Build your product as you normally would, e.g.:
       ninja -C out/Release -j 100 content_shell_apk

  2. Build the "binary_size_tool" target from ../../build/all_android.gyp, e.g.:
       ninja -C out/Release binary_size_tool

  3. Run the tool specifying the library and the output report directory.
     This command will run the analysis on the Content Shell native library for
     Android using the nm/addr2line binaries from the Android NDK for ARM,
     producing an HTML report in /tmp/report:
       tools/binary_size/run_binary_size_analysis.py \
         --library out/Release/lib/libcontent_shell_content_view.so \
         --destdir /tmp/report \
         --arch android-arm

Of course, there are additional options that you can see by running the tool
with "--help".

This whole process takes about an hour on a modern (circa 2014) quad-core
machine. If you have LOTS of RAM, you can use the "--jobs" argument to
add more addr2line workers; doing so will *greatly* reduce the processing time
but will devour system memory. If you've got the horsepower, 10 workers can
thrash through the binary in about 5 minutes at a cost of around 60 GB of RAM.

--------------------------------------------------------------------------------
Analyzing the Output
--------------------------------------------------------------------------------
When the tool finishes its work you'll find an HTML report in the output
directory that you specified with "--destdir". Open the index.html file in your
*cough* browser of choice *cough* and have a look around. The index.html page
is likely to evolve over time, but will always be your starting point for
investigation. From here you'll find links to various views of the data such
as treemap visualizations, overall statistics and "top n" lists of various
kinds.

The report is completely standalone. No external resources are required, so the
report may be saved and viewed offline with no problems.

--------------------------------------------------------------------------------
Caveats
--------------------------------------------------------------------------------
The tool is not perfect and has several shortcomings:

  * Not all space in the binary is accounted for. The cause is still under
    investigation.
  * When dealing with inlining and such, the size cost is attributed to the
    resource in which the code gets inlined. Depending upon your goals for
    analysis, this may be either good or bad; fundamentally, the more trickery
    that the compiler and/or linker do, the less simple the relationship
    between the original source and the resultant binary.
  * The tool is partly written in Java, temporarily tying it to Android
    purely and solely because it needs Java build logic which is only defined
    in the Android part of the build system. The Java portions need to be
    rewritten in Python so we can decouple from Android, or we need to find
    an alternative (readelf, linker maps, etc) to running nm/addr2line.
  * The Java code assumes that the library file is within a Chromium release
    directory. This limits it to Chromium-based binaries only.
  * The Java code has a hack to accurately identify the source of ICU data
    within the Chromium source tree due to missing size information in the ICU
    ASM output in some build variants.
  * The Python script assumes that arm-based and mips-based nm/addr2line
    binaries exist in ../../third_party/android_tools/ndk/toolchains. This is
    true only when dealing with Android and again limits the tool to
    Chromium-based binaries.
  * The Python script uses build system variables to construct the classpath
    for running the Java code.
  * The Javascript code in the HTML report Assumes code lives in Chromium for
    generated hyperlinks and will not hyperlink any file that starts with the
    substring "out".

--------------------------------------------------------------------------------
Feature Requests and Bug Reports
--------------------------------------------------------------------------------
Please file bugs and feature requests here, making sure to use the label
"Tools-BinarySize":
  https://code.google.com/p/chromium/issues/entry?labels=Tools-BinarySize

View all open issues here:
  https://code.google.com/p/chromium/issues/list?can=2&q=label:Tools-BinarySize
