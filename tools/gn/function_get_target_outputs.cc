// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/build_settings.h"
#include "tools/gn/file_template.h"
#include "tools/gn/functions.h"
#include "tools/gn/ninja_helper.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/settings.h"
#include "tools/gn/target.h"
#include "tools/gn/value.h"

namespace functions {

namespace {

void GetOutputsForTarget(const Settings* settings,
                         const Target* target,
                         std::vector<std::string>* ret) {
  switch (target->output_type()) {
    case Target::ACTION: {
      // Actions: return the outputs specified.
      const std::vector<std::string>& outs = target->action_values().outputs();
      ret->reserve(outs.size());
      for (size_t i = 0; i < outs.size(); i++)
        ret->push_back(outs[i]);
      break;
    }

    case Target::COPY_FILES:
    case Target::ACTION_FOREACH: {
      // Copy/action_foreach: return the result of the template in the outputs.
      FileTemplate file_template(settings, target->action_values().outputs(),
                                 FileTemplate::OUTPUT_ABSOLUTE, SourceDir());
      const std::vector<SourceFile>& sources = target->sources();
      for (size_t i = 0; i < sources.size(); i++)
        file_template.Apply(sources[i], ret);
      break;
    }

    case Target::EXECUTABLE:
    case Target::SHARED_LIBRARY:
    case Target::STATIC_LIBRARY:
      // Return the resulting binary file. Currently, fall through to the
      // Ninja helper below which will compute the main output name.
      //
      // TODO(brettw) some targets have secondary files which should go into
      // the list after the main (like shared libraries on Windows have an
      // import library).
    case Target::GROUP:
    case Target::SOURCE_SET: {
      // These return the stamp file, which is computed by the NinjaHelper.
      NinjaHelper helper(settings->build_settings());
      OutputFile output_file = helper.GetTargetOutputFile(target);

      // The output file is relative to the build dir.
      std::string absolute_output_file =
          settings->build_settings()->build_dir().value();
      absolute_output_file.append(output_file.value());

      ret->push_back(absolute_output_file);
      break;
    }

    default:
      NOTREACHED();
  }
}

}  // namespace

const char kGetTargetOutputs[] = "get_target_outputs";
const char kGetTargetOutputs_HelpShort[] =
    "get_target_outputs: [file list] Get the list of outputs from a target.";
const char kGetTargetOutputs_Help[] =
    "get_target_outputs: [file list] Get the list of outputs from a target.\n"
    "\n"
    "  get_target_outputs(target_label)\n"
    "\n"
    "  Returns a list of output files for the named target. The named target\n"
    "  must have been previously defined in the current file before this\n"
    "  function is called (it can't reference targets in other files because\n"
    "  there isn't a defined execution order, and it obviously can't\n"
    "  reference targets that are defined after the function call).\n"
    "\n"
    "Return value\n"
    "\n"
    "  The names in the resulting list will be absolute file paths (normally\n"
    "  like \"//out/Debug/bar.exe\", depending on the build directory).\n"
    "\n"
    "  action targets: this will just return the files specified in the\n"
    "  \"outputs\" variable of the target.\n"
    "\n"
    "  action_foreach targets: this will return the result of applying\n"
    "  the output template to the sources (see \"gn help source_expansion\").\n"
    "  This will be the same result (though with guaranteed absolute file\n"
    "  paths), as process_file_template will return for those inputs\n"
    "  (see \"gn help process_file_template\").\n"
    "\n"
    "  binary targets (executables, libraries): this will return a list\n"
    "  of the resulting binary file(s). The \"main output\" (the actual\n"
    "  binary or library) will always be the 0th element in the result.\n"
    "  Depending on the platform and output type, there may be other output\n"
    "  files as well (like import libraries) which will follow.\n"
    "\n"
    "  source sets and groups: this will return a list containing the path of\n"
    "  the \"stamp\" file that Ninja will produce once all outputs are\n"
    "  generated. This probably isn't very useful.\n"
    "\n"
    "Example\n"
    "\n"
    "  # Say this action generates a bunch of C source files.\n"
    "  action_foreach(\"my_action\") {\n"
    "    sources = [ ... ]\n"
    "    outputs = [ ... ]\n"
    "  }\n"
    "\n"
    "  # Compile the resulting source files into a source set.\n"
    "  source_set(\"my_lib\") {\n"
    "    sources = get_target_outputs(\":my_action\")\n"
    "  }\n";

Value RunGetTargetOutputs(Scope* scope,
                          const FunctionCallNode* function,
                          const std::vector<Value>& args,
                          Err* err) {
  if (args.size() != 1) {
    *err = Err(function, "Expected one argument.");
    return Value();
  }

  // Resolve the requested label.
  Label label = Label::Resolve(scope->GetSourceDir(),
                               ToolchainLabelForScope(scope), args[0], err);
  if (label.is_null())
    return Value();

  // Find the referenced target. The targets previously encountered in this
  // scope will have been stashed in the item collector (they'll be dispatched
  // when this file is done running) so we can look through them.
  const Target* target = NULL;
  Scope::ItemVector* collector = scope->GetItemCollector();
  if (!collector) {
    *err = Err(function, "No targets defined in this context.");
    return Value();
  }
  for (size_t i = 0; i < collector->size(); i++) {
    const Item* item = (*collector)[i]->get();
    if (item->label() != label)
      continue;

    const Target* as_target = item->AsTarget();
    if (!as_target) {
      *err = Err(function, "Label does not refer to a target.",
          label.GetUserVisibleName(false) +
          "\nrefers to a " + item->GetItemTypeName());
      return Value();
    }
    target = as_target;
    break;
  }

  if (!target) {
    *err = Err(function, "Target not found in this context.",
        label.GetUserVisibleName(false) +
        "\nwas not found. get_target_outputs() can only be used for targets\n"
        "previously defined in the current file.");
    return Value();
  }

  std::vector<std::string> files;
  GetOutputsForTarget(scope->settings(), target, &files);

  Value ret(function, Value::LIST);
  ret.list_value().reserve(files.size());
  for (size_t i = 0; i < files.size(); i++)
    ret.list_value().push_back(Value(function, files[i]));

  return ret;
}

}  // namespace functions
