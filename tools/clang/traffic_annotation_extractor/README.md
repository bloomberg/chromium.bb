# Traffic Annotation Extrator
This is a clang tool to extract network traffic annotations. The tool is run by
`tools/traffic_annotation/auditor/traffic_annotation_auditor.py`. Refer to it
for help on how to use.

## Build on Linux
`tools/clang/scripts/update.py --bootstrap --force-local-build
   --without-android --extra-tools traffic_annotation_extractor`

## Build on Window
1. Either open a `VS2015 x64 Native Tools Command Prompt`, or open a normal
   command prompt and run `depot_tools\win_toolchain\vs_files\
   $long_autocompleted_hash\win_sdk\bin\setenv.cmd /x64`
2. Run `python tools/clang/scripts/update.py --bootstrap --force-local-build
   --without-android --extra-tools traffic_annotation_extractor`

## Usage
Run `traffic_annotation_extractor --help` for parameters help.

Example for direct call:
  `third_party/llvm-build/Release+Asserts/bin/traffic_annotation_extractor
     -p=out/Debug components/spellcheck/browser/spelling_service_client.cc`

Example for call using run_tool.py:
  `tools/clang/scripts/run_tool.py --tool=traffic_annotation_extractor
     --generate-compdb -p=out/Debug components/spellcheck/browser`

The executable extracts network traffic annotations from given file paths based
  on build parameters in build path, and writes them to llvm::outs.
  Each output will have the following format:
  - Line 1: File path.
  - Line 2: Name of the function in which annotation is defined.
  - Line 3: Line number of annotation.
  - Line 4: Function type ("Definition", "Partial", "Completing",
            "BranchedCompleting").
  - Line 5: Unique id of annotation.
  - Line 6: Completing id or group id, when applicable, empty otherwise.
  - Line 7-: Serialized protobuf of the annotation.
Outputs are enclosed by "==== NEW ANNOTATION ====" and
  "==== ANNOTATION ENDS ===="
