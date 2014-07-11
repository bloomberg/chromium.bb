// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/file_template.h"

#include <algorithm>
#include <iostream>

#include "tools/gn/escape.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/string_utils.h"
#include "tools/gn/target.h"

const char FileTemplate::kSource[] = "{{source}}";
const char FileTemplate::kSourceNamePart[] = "{{source_name_part}}";
const char FileTemplate::kSourceFilePart[] = "{{source_file_part}}";
const char FileTemplate::kSourceDir[] = "{{source_dir}}";
const char FileTemplate::kRootRelDir[] = "{{source_root_relative_dir}}";
const char FileTemplate::kSourceGenDir[] = "{{source_gen_dir}}";
const char FileTemplate::kSourceOutDir[] = "{{source_out_dir}}";

const char kSourceExpansion_Help[] =
    "How Source Expansion Works\n"
    "\n"
    "  Source expansion is used for the action_foreach and copy target types\n"
    "  to map source file names to output file names or arguments.\n"
    "\n"
    "  To perform source expansion in the outputs, GN maps every entry in the\n"
    "  sources to every entry in the outputs list, producing the cross\n"
    "  product of all combinations, expanding placeholders (see below).\n"
    "\n"
    "  Source expansion in the args works similarly, but performing the\n"
    "  placeholder substitution produces a different set of arguments for\n"
    "  each invocation of the script.\n"
    "\n"
    "  If no placeholders are found, the outputs or args list will be treated\n"
    "  as a static list of literal file names that do not depend on the\n"
    "  sources.\n"
    "\n"
    "  See \"gn help copy\" and \"gn help action_foreach\" for more on how\n"
    "  this is applied.\n"
    "\n"
    "Placeholders\n"
    "\n"
    "  {{source}}\n"
    "      The name of the source file including directory (*). This will\n"
    "      generally be used for specifying inputs to a script in the\n"
    "      \"args\" variable.\n"
    "        \"//foo/bar/baz.txt\" => \"../../foo/bar/baz.txt\"\n"
    "\n"
    "  {{source_file_part}}\n"
    "      The file part of the source including the extension.\n"
    "        \"//foo/bar/baz.txt\" => \"baz.txt\"\n"
    "\n"
    "  {{source_name_part}}\n"
    "      The filename part of the source file with no directory or\n"
    "      extension. This will generally be used for specifying a\n"
    "      transformation from a soruce file to a destination file with the\n"
    "      same name but different extension.\n"
    "        \"//foo/bar/baz.txt\" => \"baz\"\n"
    "\n"
    "  {{source_dir}}\n"
    "      The directory (*) containing the source file with no\n"
    "      trailing slash.\n"
    "        \"//foo/bar/baz.txt\" => \"../../foo/bar\"\n"
    "\n"
    "  {{source_root_relative_dir}}\n"
    "      The path to the source file's directory relative to the source\n"
    "      root, with no leading \"//\" or trailing slashes. If the path is\n"
    "      system-absolute, (beginning in a single slash) this will just\n"
    "      return the path with no trailing slash. This value will always\n"
    "      be the same, regardless of whether it appears in the \"outputs\"\n"
    "      or \"args\" section.\n"
    "        \"//foo/bar/baz.txt\" => \"foo/bar\"\n"
    "\n"
    "  {{source_gen_dir}}\n"
    "      The generated file directory (*) corresponding to the source\n"
    "      file's path. This will be different than the target's generated\n"
    "      file directory if the source file is in a different directory\n"
    "      than the BUILD.gn file.\n"
    "        \"//foo/bar/baz.txt\" => \"gen/foo/bar\"\n"
    "\n"
    "  {{source_out_dir}}\n"
    "      The object file directory (*) corresponding to the source file's\n"
    "      path, relative to the build directory. this us be different than\n"
    "      the target's out directory if the source file is in a different\n"
    "      directory than the build.gn file.\n"
    "        \"//foo/bar/baz.txt\" => \"obj/foo/bar\"\n"
    "\n"
    "(*) Note on directories\n"
    "\n"
    "  Paths containing directories (except the source_root_relative_dir)\n"
    "  will be different depending on what context the expansion is evaluated\n"
    "  in. Generally it should \"just work\" but it means you can't\n"
    "  concatenate strings containing these values with reasonable results.\n"
    "\n"
    "  Details: source expansions can be used in the \"outputs\" variable,\n"
    "  the \"args\" variable, and in calls to \"process_file_template\". The\n"
    "  \"args\" are passed to a script which is run from the build directory,\n"
    "  so these directories will relative to the build directory for the\n"
    "  script to find. In the other cases, the directories will be source-\n"
    "  absolute (begin with a \"//\") because the results of those expansions\n"
    "  will be handled by GN internally.\n"
    "\n"
    "Examples\n"
    "\n"
    "  Non-varying outputs:\n"
    "    action(\"hardcoded_outputs\") {\n"
    "      sources = [ \"input1.idl\", \"input2.idl\" ]\n"
    "      outputs = [ \"$target_out_dir/output1.dat\",\n"
    "                  \"$target_out_dir/output2.dat\" ]\n"
    "    }\n"
    "  The outputs in this case will be the two literal files given.\n"
    "\n"
    "  Varying outputs:\n"
    "    action_foreach(\"varying_outputs\") {\n"
    "      sources = [ \"input1.idl\", \"input2.idl\" ]\n"
    "      outputs = [ \"{{source_gen_dir}}/{{source_name_part}}.h\",\n"
    "                  \"{{source_gen_dir}}/{{source_name_part}}.cc\" ]\n"
    "    }\n"
    "  Performing source expansion will result in the following output names:\n"
    "    //out/Debug/obj/mydirectory/input1.h\n"
    "    //out/Debug/obj/mydirectory/input1.cc\n"
    "    //out/Debug/obj/mydirectory/input2.h\n"
    "    //out/Debug/obj/mydirectory/input2.cc\n";

FileTemplate::FileTemplate(const Settings* settings,
                           const Value& t,
                           OutputStyle output_style,
                           const SourceDir& relative_to,
                           Err* err)
    : settings_(settings),
      output_style_(output_style),
      relative_to_(relative_to),
      has_substitutions_(false) {
  std::fill(types_required_, &types_required_[Subrange::NUM_TYPES], false);
  ParseInput(t, err);
}

FileTemplate::FileTemplate(const Settings* settings,
                           const std::vector<std::string>& t,
                           OutputStyle output_style,
                           const SourceDir& relative_to)
    : settings_(settings),
      output_style_(output_style),
      relative_to_(relative_to),
      has_substitutions_(false) {
  std::fill(types_required_, &types_required_[Subrange::NUM_TYPES], false);
  for (size_t i = 0; i < t.size(); i++)
    ParseOneTemplateString(t[i]);
}

FileTemplate::FileTemplate(const Settings* settings,
                           const std::vector<SourceFile>& t,
                           OutputStyle output_style,
                           const SourceDir& relative_to)
    : settings_(settings),
      output_style_(output_style),
      relative_to_(relative_to),
      has_substitutions_(false) {
  std::fill(types_required_, &types_required_[Subrange::NUM_TYPES], false);
  for (size_t i = 0; i < t.size(); i++)
    ParseOneTemplateString(t[i].value());
}

FileTemplate::~FileTemplate() {
}

// static
FileTemplate FileTemplate::GetForTargetOutputs(const Target* target) {
  const std::vector<std::string>& outputs = target->action_values().outputs();
  std::vector<std::string> output_template_args;
  for (size_t i = 0; i < outputs.size(); i++)
    output_template_args.push_back(outputs[i]);
  return FileTemplate(target->settings(), output_template_args,
                      OUTPUT_ABSOLUTE, SourceDir());
}

bool FileTemplate::IsTypeUsed(Subrange::Type type) const {
  DCHECK(type > Subrange::LITERAL && type < Subrange::NUM_TYPES);
  return types_required_[type];
}

void FileTemplate::Apply(const SourceFile& source,
                         std::vector<std::string>* output) const {
  // Compute all substitutions needed so we can just do substitutions below.
  // We skip the LITERAL one since that varies each time.
  std::string subst[Subrange::NUM_TYPES];
  for (int i = 1; i < Subrange::NUM_TYPES; i++) {
    if (types_required_[i]) {
      subst[i] = GetSubstitution(settings_, source,
                                 static_cast<Subrange::Type>(i),
                                 output_style_, relative_to_);
    }
  }

  size_t first_output_index = output->size();
  output->resize(output->size() + templates_.container().size());
  for (size_t template_i = 0;
       template_i < templates_.container().size(); template_i++) {
    const Template& t = templates_[template_i];
    std::string& cur_output = (*output)[first_output_index + template_i];
    for (size_t subrange_i = 0; subrange_i < t.container().size();
         subrange_i++) {
      if (t[subrange_i].type == Subrange::LITERAL)
        cur_output.append(t[subrange_i].literal);
      else
        cur_output.append(subst[t[subrange_i].type]);
    }
  }
}

void FileTemplate::WriteWithNinjaExpansions(std::ostream& out) const {
  EscapeOptions escape_options;
  escape_options.mode = ESCAPE_NINJA_COMMAND;
  escape_options.inhibit_quoting = true;

  for (size_t template_i = 0;
       template_i < templates_.container().size(); template_i++) {
    out << " ";  // Separate args with spaces.

    const Template& t = templates_[template_i];

    // Escape each subrange into a string. Since we're writing out Ninja
    // variables, we can't quote the whole thing, so we write in pieces, only
    // escaping the literals, and then quoting the whole thing at the end if
    // necessary.
    bool needs_quoting = false;
    std::string item_str;
    for (size_t subrange_i = 0; subrange_i < t.container().size();
         subrange_i++) {
      if (t[subrange_i].type == Subrange::LITERAL) {
        bool cur_needs_quoting = false;
        item_str.append(EscapeString(t[subrange_i].literal, escape_options,
                                     &cur_needs_quoting));
        needs_quoting |= cur_needs_quoting;
      } else {
        // Don't escape this since we need to preserve the $.
        item_str.append("${");
        item_str.append(GetNinjaVariableNameForType(t[subrange_i].type));
        item_str.append("}");
      }
    }

    if (needs_quoting || item_str.empty()) {
      // Need to shell quote the whole string. We also need to quote empty
      // strings or it would be impossible to pass "" as a command-line
      // argument.
      out << '"' << item_str << '"';
    } else {
      out << item_str;
    }
  }
}

void FileTemplate::WriteNinjaVariablesForSubstitution(
    std::ostream& out,
    const SourceFile& source,
    const EscapeOptions& escape_options) const {
  for (int i = 1; i < Subrange::NUM_TYPES; i++) {
    if (types_required_[i]) {
      Subrange::Type type = static_cast<Subrange::Type>(i);
      out << "  " << GetNinjaVariableNameForType(type) << " = ";
      EscapeStringToStream(
          out,
          GetSubstitution(settings_, source, type, output_style_, relative_to_),
          escape_options);
      out << std::endl;
    }
  }
}

// static
const char* FileTemplate::GetNinjaVariableNameForType(Subrange::Type type) {
  switch (type) {
    case Subrange::SOURCE:
      return "source";
    case Subrange::NAME_PART:
      return "source_name_part";
    case Subrange::FILE_PART:
      return "source_file_part";
    case Subrange::SOURCE_DIR:
      return "source_dir";
    case Subrange::ROOT_RELATIVE_DIR:
      return "source_root_rel_dir";
    case Subrange::SOURCE_GEN_DIR:
      return "source_gen_dir";
    case Subrange::SOURCE_OUT_DIR:
      return "source_out_dir";

    default:
      NOTREACHED();
  }
  return "";
}

// static
std::string FileTemplate::GetSubstitution(const Settings* settings,
                                          const SourceFile& source,
                                          Subrange::Type type,
                                          OutputStyle output_style,
                                          const SourceDir& relative_to) {
  std::string to_rebase;
  switch (type) {
    case Subrange::SOURCE:
      if (source.is_system_absolute())
        return source.value();
      to_rebase = source.value();
      break;

    case Subrange::NAME_PART:
      return FindFilenameNoExtension(&source.value()).as_string();

    case Subrange::FILE_PART:
      return source.GetName();

    case Subrange::SOURCE_DIR:
      if (source.is_system_absolute())
        return DirectoryWithNoLastSlash(source.GetDir());
      to_rebase = DirectoryWithNoLastSlash(source.GetDir());
      break;

    case Subrange::ROOT_RELATIVE_DIR:
      if (source.is_system_absolute())
        return DirectoryWithNoLastSlash(source.GetDir());
      return RebaseSourceAbsolutePath(
          DirectoryWithNoLastSlash(source.GetDir()), SourceDir("//"));

    case Subrange::SOURCE_GEN_DIR:
      to_rebase = DirectoryWithNoLastSlash(
          GetGenDirForSourceDir(settings, source.GetDir()));
      break;

    case Subrange::SOURCE_OUT_DIR:
      to_rebase = DirectoryWithNoLastSlash(
          GetOutputDirForSourceDir(settings, source.GetDir()));
      break;

    default:
      NOTREACHED();
      return std::string();
  }

  // If we get here, the result is a path that should be made relative or
  // absolute according to the output_style. Other cases (just file name or
  // extension extraction) will have been handled via early return above.
  if (output_style == OUTPUT_ABSOLUTE)
    return to_rebase;
  return RebaseSourceAbsolutePath(to_rebase, relative_to);
}

void FileTemplate::ParseInput(const Value& value, Err* err) {
  switch (value.type()) {
    case Value::STRING:
      ParseOneTemplateString(value.string_value());
      break;
    case Value::LIST:
      for (size_t i = 0; i < value.list_value().size(); i++) {
        if (!value.list_value()[i].VerifyTypeIs(Value::STRING, err))
          return;
        ParseOneTemplateString(value.list_value()[i].string_value());
      }
      break;
    default:
      *err = Err(value, "File template must be a string or list.",
                 "A sarcastic comment about your skills goes here.");
  }
}

void FileTemplate::ParseOneTemplateString(const std::string& str) {
  templates_.container().resize(templates_.container().size() + 1);
  Template& t = templates_[templates_.container().size() - 1];

  size_t cur = 0;
  while (true) {
    size_t next = str.find("{{", cur);

    // Pick up everything from the previous spot to here as a literal.
    if (next == std::string::npos) {
      if (cur != str.size())
        t.container().push_back(Subrange(Subrange::LITERAL, str.substr(cur)));
      break;
    } else if (next > cur) {
      t.container().push_back(
          Subrange(Subrange::LITERAL, str.substr(cur, next - cur)));
    }

    // Given the name of the string constant and enum for a template parameter,
    // checks for it and stores it. Writing this as a function requires passing
    // the entire state of this function as arguments, so this actually ends
    // up being more clear.
    #define IF_MATCH_THEN_STORE(const_name, enum_name) \
        if (str.compare(next, arraysize(const_name) - 1, const_name) == 0) { \
          t.container().push_back(Subrange(Subrange::enum_name)); \
          types_required_[Subrange::enum_name] = true; \
          has_substitutions_ = true; \
          cur = next + arraysize(const_name) - 1; \
        }

    // Decode the template param.
    IF_MATCH_THEN_STORE(kSource, SOURCE)
    else IF_MATCH_THEN_STORE(kSourceNamePart, NAME_PART)
    else IF_MATCH_THEN_STORE(kSourceFilePart, FILE_PART)
    else IF_MATCH_THEN_STORE(kSourceDir, SOURCE_DIR)
    else IF_MATCH_THEN_STORE(kRootRelDir, ROOT_RELATIVE_DIR)
    else IF_MATCH_THEN_STORE(kSourceGenDir, SOURCE_GEN_DIR)
    else IF_MATCH_THEN_STORE(kSourceOutDir, SOURCE_OUT_DIR)
    else {
      // If it's not a match, treat it like a one-char literal (this will be
      // rare, so it's not worth the bother to add to the previous literal) so
      // we can keep going.
      t.container().push_back(Subrange(Subrange::LITERAL, "{"));
      cur = next + 1;
    }

    #undef IF_MATCH_THEN_STORE
  }
}
