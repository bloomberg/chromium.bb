# Network Traffic Annotation Auditor
This executable runs the clang tool for extraction of Network Traffic
Annotations from chromium source code and collects and summarizes its outputs.
Please see `docs/network_traffic_annotations.md` for an introduction to network
traffic annotations.

## Usage
`traffic_annotation_auditor [OPTION]... [path_filter]...`

Extracts network traffic annotations from source files. If path filter(s) are
specified, only those directories of the source will be analyzed.
Run `traffic_annotation_auditor --help` for options.

Example:
  `traffic_annotation_auditor --build-dir=out/Debug`

The binaries of this file and the clang tool are checked out into
`tools/traffic_annotation/bin/[platform]`. This is only done for Linux platform
now and will be extended to other platforms later.

## Running on Linux
Before running, you need to build the COMPLETE chromium.

## Running on Windows
Before running, you need to build the COMPLETE chromium using clang with keeprsp
switch as follows:
1. `gn args [build_dir, e.g. out\Debug]`
2. add `is_clang=true` to the opened text file and save and close it.
3. `ninja -C [build_dir] -d keeprsp`
