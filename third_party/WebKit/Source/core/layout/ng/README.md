# LayoutNG #

This directory contains the implementation of Blink's new layout engine
"LayoutNG".

This README can be viewed in formatted form [here](https://chromium.googlesource.com/chromium/src/+/master/third_party/WebKit/Source/core/layout/ng/README.md).

The original design document can be seen [here](https://docs.google.com/document/d/1uxbDh4uONFQOiGuiumlJBLGgO4KDWB8ZEkp7Rd47fw4/edit).

## High level overview ##

CSS has many different types of layout modes, controlled by the `display`
property. (In addition to this specific HTML elements have custom layout modes
as well). For each different type of layout, we have a
[NGLayoutAlgorithm](ng_layout_algorithm.h).

The input to an [NGLayoutAlgorithm](ng_layout_algorithm.h) is the same tuple
for every kind of layout:

 - The [NGBlockNode](ng_block_node.h) which we are currently performing layout for. The
   following information is accessed:

   - The [ComputedStyle](../../style/ComputedStyle.h) for the node which we are
     currently performing laying for.

   - The list of children [NGBlockNode](ng_block_node.h)es to perform layout upon, and their
     respective style objects.

 - The [NGConstraintSpace](ng_constraint_space.h) which represents the "space"
   in which the current layout should produce a
   [NGPhysicalFragment](ng_physical_fragment.h).

 - TODO(layout-dev): BreakTokens should go here once implemented.

The current layout should not access any information outside this set, this
will break invariants in the system. (As a concrete example we intend to cache
[NGPhysicalFragment](ng_physical_fragment.h)s based on this set, accessing
additional information outside this set will break caching behaviour).

### Box Tree ###

TODO(layout-dev): Document with lots of pretty pictures.

### Inline Layout ###

Please refer to the [inline layout README](inline/README.md).

### Fragment Tree ###

TODO(layout-dev): Document with lots of pretty pictures.

### Constraint Spaces ###

TODO(layout-dev): Document with lots of pretty pictures.

## Block Flow Algorithm ##

This section contains details specific to the
[NGBlockLayoutAlgorithm](ng_block_layout_algorithm.h).

### Collapsing Margins ###

TODO(layout-dev): Document with lots of pretty pictures.

### Float Positioning ###

TODO(layout-dev): Document with lots of pretty pictures.

### Code coverage ###

The latest code coverage (from Feb 14 2017) can be found [here](https://glebl.users.x20web.corp.google.com/www/layout_ng_code_coverage/index.html).
Here is the instruction how to generate a new result.

#### Environment setup ####
 1. Set up Chromium development environment for Windows
 2. Download DynamoRIO from [www.dynamorio.org](www.dynamorio.org)
 3. Extract downloaded DynamoRIO package into your chromium/src folder.
 4. Get dynamorio.git and extract it into your chromium/src folder `git clone https://github.com/DynamoRIO/dynamorio.git`
 5. Install Node js from https://nodejs.org/en/download/
 6. Install lcov-merger dependencies:  `npm install vinyl, npm install vinyl-fs`
 7. Install Perl from https://www.perl.org/get.html#win32
 8. Get lcov-result-merger and extract into your chromium/src folder `git clone https://github.com/mweibel/lcov-result-merger`

#### Generating code coverage ####
* Build the unit tets target with debug information
`chromium\src> ninja -C out\Debug webkit_unit_tests`
* Run DynamoRIO with drcov tool
`chromium\src>DynamoRIO\bin64\drrun.exe -t drcov -- .\out\Debug\webkit_unit_tests.exe --gtest_filter=NG*`
* Convert the output information to lcov format
`chromium\src>for %file in (*.log) do DynamoRIO\tools\bin64\drcov2lcov.exe -input %file -output %file.info -src_filter layout/ng -src_skip_filter _test`
* Merge all lcov files into one file
`chromium\src>node lcov-result-merger\bin\lcov-result-merger.js *.info output.info`
* Generate the coverage html from the master lcov file
`chromium\src>C:\Perl64\bin\perl.exe dynamorio.git\third_party\lcov\genhtml output.info -o output`
