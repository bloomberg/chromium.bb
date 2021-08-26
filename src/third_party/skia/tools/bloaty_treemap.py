#!/usr/bin/env python3

# Copyright 2021 Google LLC
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# This script is written to process the output from bloaty, read via stdin
# The easiest way to use the script:
#
#   bloaty <path_to_binary> -d compileunits,symbols -n 0 --tsv | bloaty_treemap.py > bloaty.html
#
# Open the resulting .html file in your browser.

# TODO: Deal with symbols vs. fullsymbols, even both?
# TODO: Support aggregation by scope, rather than file (split C++ identifiers on '::')
# TODO: Deal with duplicate symbols better. These are actually good targets for optimization.
#       They are sometimes static functions in headers (so they appear in multiple .o files),
#       There are also symbols that appear multiple times due to inlining (eg, kNoCropRect).
# TODO: Figure out why some symbols are misattributed. Eg, Swizzle::Convert and ::Make are tied
#       to the header by nm, and then to one caller (at random) by bloaty. They're not inlined,
#       though. Unless LTO is doing something wacky here? Scope-aggregation may be the answer?
#       Ultimately, this seems like an issue with bloaty and/or debug information itself.

import os
import sys

parent_map = {}

# For a given filepath "foo/bar/baz.cpp", `add_path` outputs rows to the data table
# establishing the node hierarchy, and ensures that each line is emitted exactly once:
#
#   ['foo/bar/baz.cpp', 'foo/bar', 0],
#   ['foo/bar',         'foo',     0],
#   ['foo',             'ROOT',    0],
def add_path(path):
    if not path in parent_map:
        head = os.path.split(path)[0]
        if not head:
            parent_map[path] = "ROOT"
        else:
            add_path(head)
            parent_map[path] = head
        print("['" + path + "', '" + parent_map[path] + "', 0],")

def main():
    # HTML/script header, plus the first two (fixed) rows of the data table
    print("""
    <html>
        <head>
            <script type="text/javascript" src="https://www.gstatic.com/charts/loader.js"></script>
            <script type="text/javascript">
                google.charts.load('current', {'packages':['treemap']});
                google.charts.setOnLoadCallback(drawChart);
                function drawChart() {
                    const data = google.visualization.arrayToDataTable([
                        ['Name', 'Parent', 'Size'],
                        ['ROOT', null, 0],""")

    all_symbols = {}

    # Skip header row
    # TODO: In the future, we could use this to automatically detect the source columns
    next(sys.stdin)

    for line in sys.stdin:
        vals = line.rstrip().split("\t")
        if len(vals) != 4:
            print("ERROR: Failed to match line\n" + line)
            sys.exit(1)
        (filepath, symbol, vmsize, filesize) = vals

        # Skip any entry where the filepath or symbol starts with '['
        # These tend to be section meta-data and debug information
        if filepath.startswith("[") or symbol.startswith("["):
            continue

        # Strip the leading ../../ from paths
        while filepath.startswith("../"):
            filepath = filepath.removeprefix("../")

        # Files in third_party sometimes have absolute paths. Strip those:
        if filepath.startswith("/"):
            rel_path_start = filepath.find("third_party")
            if rel_path_start >= 0:
                filepath = filepath[rel_path_start:]
            else:
                print("ERROR: Unexpected absolute path:\n" + filepath)
                sys.exit(1)

        # Symbols involving C++ lambdas can contain single quotes
        symbol = symbol.replace("'", "\\'")

        # Ensure that we've added intermediate nodes for all portions of this file path
        add_path(filepath)

        # Ensure that our final symbol name is unique
        while symbol in all_symbols:
            symbol += "_x"
        all_symbols[symbol] = True

        # Append another row for our sanitized data
        print("['" + symbol + "', '" + filepath + "', " + filesize + "],")

    # HTML/script footer
    print("""       ]);
                    tree = new google.visualization.TreeMap(document.getElementById('chart_div'));
                    tree.draw(data, {
                        generateTooltip: showTooltip
                    });

                    function showTooltip(row, size, value) {
                        const escapedLabel = data.getValue(row, 0)
                            .replace('&', '&amp;')
                            .replace('<', '&lt;')
                            .replace('>', '&gt;')
                        return `<div style="background:#fd9; padding:10px; border-style:solid">
                                <span style="font-family:Courier"> ${escapedLabel} <br>
                                Size: ${size} </div>`;
                    }
                }
            </script>
        </head>
        <body>
            <div id="chart_div" style="width: 100%; height: 100%;"></div>
        </body>
    </html>""")

if __name__ == "__main__":
    main()
