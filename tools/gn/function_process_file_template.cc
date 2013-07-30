// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/file_template.h"
#include "tools/gn/functions.h"
#include "tools/gn/parse_tree.h"

/*
process_file_template: Do template expansion over a list of files.

  process_file_template(source_list, template)

  process_file_template applies a template list to a source file list,
  returning the result of applying each template to each source. This is
  typically used for computing output file names from input files.

Arguments:

  The source_list is a list of file names.

  The template can be a string or a list. If it is a list, multiple output
  strings are generated for each input.

  The following template substrings are used in the template arguments
  and are replaced with the corresponding part of the input file name:

    "{{source}}": The entire source name.

    "{{source_name_part}}": The source name with no path or extension.

Example:

  sources = [
    "foo.idl",
    "bar.idl",
  ]
  myoutputs = process_file_template(
      sources,
      [ "$target_gen_dir/{{source_name_part}}.cc",
        "$target_gen_dir/{{source_name_part}}.h" ])

 The result in this case will be:
    [ "/out/Debug/foo.cc"
      "/out/Debug/foo.h"
      "/out/Debug/bar.cc"
      "/out/Debug/bar.h" ]
*/
Value ExecuteProcessFileTemplate(Scope* scope,
                                 const FunctionCallNode* function,
                                 const std::vector<Value>& args,
                                 Err* err) {
  if (args.size() != 2) {
    *err = Err(function->function(), "Expected two arguments");
    return Value();
  }

  FileTemplate file_template(args[1], err);
  if (err->has_error())
    return Value();

  Value ret(function, Value::LIST);
  file_template.Apply(args[0], function, &ret.list_value(), err);
  return ret;
}
