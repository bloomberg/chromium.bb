# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fileinput
import glob
import optparse
import os
import shlex
import textwrap

def AggregateVectorIcons(working_directory, file_list, output_cc, output_h):
  """Compiles all .icon files in a directory into two C++ files.

  Args:
      working_directory: The path to the directory that holds the .icon files
          and C++ templates.
      file_list: A file containing the list of vector icon files to process.
          Used for GN only (this argument defaults to None for GYP).
      output_cc: The path that should be used to write the .cc file.
      output_h: The path that should be used to write the .h file.
  """

  icon_list = []
  if file_list is None:
    # TODO(GYP_GONE): |file_list| is only None for GYP builds (where response
    # files are not supported), in which case we process all .icon files
    # contained within |working_directory| and all of its descendant
    # directories. This logic can be removed when GN is used everywhere.
    # See crbug.com/535386.
    for dirpath, dirnames, filenames in os.walk(working_directory):
      icon_list.extend(glob.glob(os.path.join(dirpath, "*.icon")))
  else:
    with open(file_list, 'r') as f:
      file_list_contents = f.read()
    icon_list = shlex.split(file_list_contents)

  input_header_template = open(os.path.join(working_directory,
                                            "vector_icons.h.template"))
  header_template_contents = input_header_template.readlines()
  input_header_template.close()
  output_header = open(output_h, "w")
  for line in header_template_contents:
    if not "TEMPLATE_PLACEHOLDER" in line:
      output_header.write(line)
      continue

    for icon_path in icon_list:
      # The icon name should be of the format "foo.icon" or "foo.1x.icon".
      (icon_name, extension) = os.path.splitext(os.path.basename(icon_path))
      (icon_name, scale_factor) = os.path.splitext(icon_name)
      if not scale_factor:
        output_header.write("  {},\n".format(icon_name.upper()))
  output_header.close()

  input_cc_template = open(
      os.path.join(working_directory, "vector_icons.cc.template"))
  cc_template_contents = input_cc_template.readlines()
  input_cc_template.close()
  output_cc = open(output_cc, "w")
  for line in cc_template_contents:
    if not "TEMPLATE_PLACEHOLDER" in line:
      output_cc.write(line)
      continue;

    for icon_path in icon_list:
      (icon_name, extension) = os.path.splitext(os.path.basename(icon_path))
      (icon_name, scale_factor) = os.path.splitext(icon_name)
      assert not scale_factor or scale_factor == ".1x"
      if (("1X" in line and scale_factor != ".1x") or
          (not "1X" in line and scale_factor == ".1x")):
        continue

      icon_file = open(icon_path)
      vector_commands = "".join(icon_file.readlines())
      icon_file.close()
      output_cc.write("ICON_TEMPLATE({}, {})\n".format(icon_name.upper(),
                                                       vector_commands))
  output_cc.close()


def main():
  parser = optparse.OptionParser()
  parser.add_option("--working_directory",
                    help="The directory to look for template C++ as well as "
                         "icon files.")
  parser.add_option("--file_list",
                    help="A response file containing the list of icon files "
                         "to be processed (GN only). Defaults to None.",
                    default=None)
  parser.add_option("--output_cc",
                    help="The path to output the CC file to.")
  parser.add_option("--output_h",
                    help="The path to output the header file to.")

  (options, args) = parser.parse_args()

  AggregateVectorIcons(options.working_directory,
                       options.file_list,
                       options.output_cc,
                       options.output_h)


if __name__ == "__main__":
  main()
