# Copyright 2017 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================

"""def_file_filter.py - tool to filter a windows def file.

The def file can be used to export symbols from the tensorflow dll to enable
tf.load_library().

Because the linker allows only 64K symbols to be exported per dll
we filter the symbols down to the essentials. The regular expressions
we use for this are specific to tensorflow.

TODO: this works fine but there is an issue with exporting
'const char * const' and importing it from a user_ops. The problem is
on the importing end and using __declspec(dllimport) works around it.
"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import io
import os
import re
import subprocess
import sys
import tempfile

# External tools we use that come with visual studio sdk
UNDNAME = "%{undname_bin_path}"
DUMPBIN_CMD = "\"{}\" /SYMBOLS".format("%{dumpbin_bin_path}")

# Exclude if matched
EXCLUDE_RE = re.compile(r"RTTI|deleting destructor|::internal::")

# Include if matched before exclude
INCLUDEPRE_RE = re.compile(r"google::protobuf::internal::ExplicitlyConstructed|"
                           r"google::protobuf::internal::ArenaImpl::AllocateAligned|" # for contrib/data/_prefetching_ops
                           r"google::protobuf::internal::ArenaImpl::AddCleanup|" # for contrib/data/_prefetching_ops
                           r"google::protobuf::internal::LogMessage|" # for contrib/data/_prefetching_ops
                           r"google::protobuf::Arena::OnArenaAllocation|" # for contrib/data/_prefetching_ops
                           r"absl::Mutex::ReaderLock|" # for //tensorflow/contrib/rnn:python/ops/_gru_ops.so and more ops
                           r"absl::Mutex::ReaderUnlock|" # for //tensorflow/contrib/rnn:python/ops/_gru_ops.so and more ops
                           r"tensorflow::internal::LogMessage|"
                           r"tensorflow::internal::LogString|"
                           r"tensorflow::internal::CheckOpMessageBuilder|"
                           r"tensorflow::internal::MakeCheckOpValueString|"
                           r"tensorflow::internal::PickUnusedPortOrDie|"
                           r"tensorflow::internal::ValidateDevice|"
                           r"tensorflow::ops::internal::Enter|"
                           r"tensorflow::strings::internal::AppendPieces|"
                           r"tensorflow::strings::internal::CatPieces|"
                           r"tensorflow::io::internal::JoinPathImpl")

# Include if matched after exclude
INCLUDE_RE = re.compile(r"^(TF_\w*)$|"
                        r"^(TFE_\w*)$|"
                        r"nsync::|"
                        r"tensorflow::|"
                        r"toco::|"
                        r"functor::|"
                        r"tf_git_version|"
                        r"tf_compiler_version|"
                        r"tf_cxx11_abi_flag|"
                        r"tf_monolithic_build|"
                        r"perftools::gputools")

# We want to identify data members explicitly in the DEF file, so that no one
# can implicitly link against the DLL if they use one of the variables exported
# from the DLL and the header they use does not decorate the symbol with
# __declspec(dllimport). It is easier to detect what a data symbol does
# NOT look like, so doing it with the below regex.
DATA_EXCLUDE_RE = re.compile(r"[)(]|"
                             r"vftable|"
                             r"vbtable|"
                             r"vcall|"
                             r"RTTI|"
                             r"protobuf::internal::ExplicitlyConstructed")

def get_args():
  """Parse command line.

  Examples:
  (usecases in //tensorflow/python:pywrap_tensorflow_filtered_def_file)
    --symbols $(location //tensorflow/tools/def_file_filter:symbols_pybind)
    --lib_paths_file $(location :pybind_symbol_target_libs_file)
  """
  filename_list = lambda x: x.split(";")
  parser = argparse.ArgumentParser()
  parser.add_argument("--input", type=filename_list,
                      help="paths to input def file",
                      required=True)
  parser.add_argument("--output", help="output deffile", required=True)
  parser.add_argument("--target", help="name of the target")
  parser.add_argument("--symbols", help="file that lists symbols to be exported.")
  parser.add_argument("--lib_paths_file", help="file that lists cc_library targets for pybind")
  args = parser.parse_args()
  return args

def get_symbols(path_to_lib, re_filter):
  """Get a list of symbols to be exported.

  Args:
    path_to_lib: String that is path (execpath) to target .lib file.
    re_filter: String that is regex filter for filtering symbols from .lib.
  """
  try:
    full_output = subprocess.check_output(
        "{} {}".format(DUMPBIN_CMD, path_to_lib),
        stderr=subprocess.STDOUT, shell=True)
  except subprocess.CalledProcessError as e:
    print("Getting symbol list using dumpbin failed with code %d:" % (
              e.returncode))
    print("\t\tFailing command: %s" % (e.cmd))
    print("\t\tOutput: %s" % (e.output))
    print("\t\tError details: %s" % (e))
    raise e

  # Convert to normal string from bytes type.
  full_output = full_output.decode()

  # Split and filter the list
  sym_split = [x for x in full_output.split("\r\n")
               if "External" in x]

  # Example symbol line:
  # 954 00000000 SECT2BD notype ()    External    | ?IsSequence@swig@tensorflow@@YA_NPEAU_object@@@Z (bool __cdecl tensorflow::swig::IsSequence(struct _object *))
  # Anomaly symbol line:
  # 00B 00000000 SECT4  notype       External     | _tensorflow_numpy_api.
  sym_filtered = []
  re_filter_comp = re.compile(r"{}".format(re_filter))

  # Filter out symbol from the split line (`sym_split` in the for loop below).
  sym_line_filter = r".*\s+\| (.*) \(.*"
  sym_line_filter_anomaly = r".*\s+\| (.*)"

  for sym_line in sym_split:
    if re_filter_comp.search(sym_line):
      try:
        sym = re.match(sym_line_filter, sym_line).groups()[0]
      except AttributeError:
        try:
          sym = re.match(sym_line_filter_anomaly, sym_line).groups()[0]
        except:
          raise RuntimeError("Unable to find the following symbol:[%s]" % sym_line)

      sym_filtered.append(sym)

  return sym_filtered

def get_pybind_export_symbols(symbols_file, lib_paths_file):
  """Returns a list of symbols to be exported from the target libs.

  Args:
    symbols_file: String that is the path to symbols_pybind.txt.
    lib_paths_file: String that is the path to txt file that lists
                    cc_library target execpaths for exporting symbols.
  """
  # cc_library target name is always in [target_name] format in
  # `symbols_pybind.txt`.
  section_header_filter = r"\[(\S+)\]"  # e.g. `[cpp_python_util]`

  # Create a dict of target libs and their symbols to be exported and populate
  # it. (key = cc_library target, value = list of symbols) that we need to
  # export.
  symbols = {}
  with open(symbols_file, "r") as f:
    curr_lib = ""
    for line in f:
      line = line.strip()
      section_header = re.match(section_header_filter, line)
      if section_header:
        curr_lib = section_header.groups()[0]
        symbols[curr_lib] = []
      elif not line:
        pass
      else:
        # If not a section header and not an empty line, then it's a symbol
        # line. e.g. `tensorflow::swig::IsSequence`
        symbols[curr_lib].append(line)

  lib_paths = []
  with open(lib_paths_file, "r") as f:
    lib_paths = [line.strip() for line in f]

  # All symbols to be exported.
  symbols_all = []
  for lib in lib_paths:
    if lib:
      for cc_lib in symbols:  # keys in symbols = cc_library target name
        path_to_lib = cc_lib.split("/")
        cc_target = path_to_lib[-1]
        # if `len(path_to_lib)` is larger than 1, that means, we are given one
        # or more parent directory of the target. e.g. `[foo/bar]` instead of
        # just the target name `[bar]`.
        if len(path_to_lib) > 1:
          parent_dir = path_to_lib[0]
        else:
          parent_dir = ""
        if cc_target in lib and parent_dir in lib:
          symbols_all.extend(
            get_symbols(lib, "|".join(symbols[cc_lib])))

  return symbols_all

def main():
  """main."""
  args = get_args()

  # Get symbols that need to be exported from specific libraries for pybind.
  symbols_pybind = []
  if args.symbols and args.lib_paths_file:
    symbols_pybind = get_pybind_export_symbols(args.symbols, args.lib_paths_file)

  # Pipe dumpbin to extract all linkable symbols from libs.
  # Good symbols are collected in candidates and also written to
  # a temp file.
  candidates = []
  tmpfile = tempfile.NamedTemporaryFile(mode="w", delete=False)
  for def_file_path in args.input:
    def_file = open(def_file_path, 'r')
    for line in def_file:
      cols = line.split()
      sym = cols[0]
      tmpfile.file.write(sym + "\n")
      candidates.append(sym)
  tmpfile.file.close()

  # Run the symbols through undname to get their undecorated name
  # so we can filter on something readable.
  with open(args.output, "w") as def_fp:
    # track dupes
    taken = set()

    # Header for the def file.
    if args.target:
      def_fp.write("LIBRARY " + args.target + "\n")
    def_fp.write("EXPORTS\n")
    def_fp.write("\t ??1OpDef@tensorflow@@UEAA@XZ\n")

    # Each symbols returned by undname matches the same position in candidates.
    # We compare on undname but use the decorated name from candidates.
    dupes = 0
    proc = subprocess.Popen([UNDNAME, tmpfile.name], stdout=subprocess.PIPE)
    for idx, line in enumerate(io.TextIOWrapper(proc.stdout, encoding="utf-8")):
      decorated = candidates[idx]
      if decorated in taken:
        # Symbol is already in output, done.
        dupes += 1
        continue

      if not INCLUDEPRE_RE.search(line):
        if EXCLUDE_RE.search(line):
          continue
        if not INCLUDE_RE.search(line):
          continue

      if "deleting destructor" in line:
        # Some of the symbols convered by INCLUDEPRE_RE export deleting
        # destructor symbols, which is a bad idea.
        # So we filter out such symbols here.
        continue

      if DATA_EXCLUDE_RE.search(line):
        def_fp.write("\t" + decorated + "\n")
      else:
        def_fp.write("\t" + decorated + " DATA\n")
      taken.add(decorated)

    for sym in symbols_pybind:
      def_fp.write("\t{}\n".format(sym))
      taken.add(sym)
    def_fp.close()

  exit_code = proc.wait()
  if exit_code != 0:
    print("{} failed, exit={}".format(UNDNAME, exit_code))
    return exit_code

  os.unlink(tmpfile.name)

  print("symbols={}, taken={}, dupes={}"
        .format(len(candidates), len(taken), dupes))
  return 0


if __name__ == "__main__":
  sys.exit(main())
