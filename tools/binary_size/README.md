# Tools for analyzing Chrome's binary size

# Super Size

Collect, archive, and analyze Chrome's binary size.

## "archive"

Collect size information and dump it into a `.size` file. Mainly consists of
symbol information parsed from a linker .map file.

### Example Usage:

    # Android:
    # Googlers:
    gn gen out/Release --args='is_official_build=true symbol_level=1 is_chrome_branded=true target_os="android"'
    # Non-Googlers:
    gn gen out/Release --args='is_official_build=true symbol_level=1 exclude_unwind_tables=true ffmpeg_branding="Chrome" proprietary_codecs=true target_os="android"'
    ninja -C out/Release -j 1000 libchrome.so
    tools/binary_size/supersize archive chrome.size --elf-file out/Release/lib.unstripped/libchrome.so -v

    # Linux:
    LLVM_DOWNLOAD_GOLD_PLUGIN=1 gclient runhooks  # One-time download.
    # Googlers:
    gn gen out/Release --args='is_official_build=true symbol_level=1 is_chrome_branded=true'
    # Non-Googlers:
    gn gen out/Release --args='is_official_build=true symbol_level=1 exclude_unwind_tables=true ffmpeg_branding="Chrome" proprietary_codecs=true'
    ninja -C out/Release -j 1000 chrome
    tools/binary_size/supersize archive chrome.size --elf-file out/Release/chrome -v

## "html_report"

Creates an interactive size breakdown (by source path) as a stand-alone html
report.

## Example Usage:

    tools/binary_size/supersize html_report chrome.size --report-dir size-report -v
    xdg-open size-report/index.html

## "console"

Starts a Python interpreter where you can run custom queries.

## Example Usage:

    # Runs a single diff and exits (does not enter interactive mode).
    tools/binary_size/supersize console without_patch.size with_patch.size --query='Diff(size_info2, size_info1)'

    # Enters a Python REPL (it will print more guidance).
    tools/binary_size/supersize console chrome.size

## diagnose_apk_bloat.py

Determine the cause of binary size bloat for a patch.

### Example Usage:

    # Sync, build, and diff for HEAD and HEAD^.
    tools/binary_size/diagnose_apk_bloat.py

    # Display detailed usage info (there are many options).
    tools/binary_size/diagnose_apk_bloat.py -h

# Roadmap for Super Size:

Tracked in https://crbug.com/681694

1. More `archive` features:

  * Find out more about 0xffffffffffffffff addresses, and why such large
    gaps exist after them.
  * Use nm to get the full list of symbols that share the same address.
  * Collect java symbol information
  * Collect .pak file information (using .o.whitelist files)
  * Collect .apk entry information

1. More `console` features:

  * Template Symbols - shows when templates lead to code bloat.
  * Duplicate Symbols - shows when statics in headers are an issue.
  * Overloaded Symbols - shows when overloads are excessive.
  * Per-class / namespace size (no way to distinguish class vs namespace).
  * Per-Chrome package (Chrome-specific grouping. e.g. name prefixes).
  * CSV output (for pasting into a spreadsheet).

1. More `html_report` features:

  * Break down by other groupings (e.g. create from nested `SymbolGroups`)

1. Integrate with `resource_sizes.py` so that it tracks size of major
   components separately: chrome vs blink vs skia vs v8.
1. Speed up some steps (like normalizing names) via multiprocessing.
1. Add dependency graph info, perhaps just on a per-file basis.

# Roadmap for diagnose_apk_bloat.py:
1. More `diagnose_apk_bloat.py` features:

  * Add diffing functionality to see diff stats for two commits.
  * Add --cloud option for using artifacts from perf builders.
