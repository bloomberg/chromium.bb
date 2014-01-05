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

const char kSourceExpansion_Help[] =
    "How Source Expansion Works\n"
    "\n"
    "  Source expansion is used for the custom script and copy target types\n"
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
    "  See \"gn help copy\" and \"gn help custom\" for more on how this is\n"
    "  applied.\n"
    "\n"
    "Placeholders\n"
    "\n"
    "  {{source}}\n"
    "      The name of the source file relative to the root build output\n"
    "      directory (which is the current directory when running compilers\n"
    "      and scripts). This will generally be used for specifying inputs\n"
    "      to a script in the \"args\" variable.\n"
    "\n"
    "  {{source_file_part}}\n"
    "      The file part of the source including the extension. For the\n"
    "      source \"foo/bar.txt\" the source file part will be \"bar.txt\".\n"
    "\n"
    "  {{source_name_part}}\n"
    "      The filename part of the source file with no directory or\n"
    "      extension. This will generally be used for specifying a\n"
    "      transformation from a soruce file to a destination file with the\n"
    "      same name but different extension. For the source \"foo/bar.txt\"\n"
    "      the source name part will be \"bar\".\n"
    "\n"
    "Examples\n"
    "\n"
    "  Non-varying outputs:\n"
    "    script(\"hardcoded_outputs\") {\n"
    "      sources = [ \"input1.idl\", \"input2.idl\" ]\n"
    "      outputs = [ \"$target_out_dir/output1.dat\",\n"
    "                  \"$target_out_dir/output2.dat\" ]\n"
    "    }\n"
    "  The outputs in this case will be the two literal files given.\n"
    "\n"
    "  Varying outputs:\n"
    "    script(\"varying_outputs\") {\n"
    "      sources = [ \"input1.idl\", \"input2.idl\" ]\n"
    "      outputs = [ \"$target_out_dir/{{source_name_part}}.h\",\n"
    "                  \"$target_out_dir/{{source_name_part}}.cc\" ]\n"
    "    }\n"
    "  Performing source expansion will result in the following output names:\n"
    "    //out/Debug/obj/mydirectory/input1.h\n"
    "    //out/Debug/obj/mydirectory/input1.cc\n"
    "    //out/Debug/obj/mydirectory/input2.h\n"
    "    //out/Debug/obj/mydirectory/input2.cc\n";

FileTemplate::FileTemplate(const Value& t, Err* err)
    : has_substitutions_(false) {
  std::fill(types_required_, &types_required_[Subrange::NUM_TYPES], false);
  ParseInput(t, err);
}

FileTemplate::FileTemplate(const std::vector<std::string>& t)
    : has_substitutions_(false) {
  std::fill(types_required_, &types_required_[Subrange::NUM_TYPES], false);
  for (size_t i = 0; i < t.size(); i++)
    ParseOneTemplateString(t[i]);
}

FileTemplate::~FileTemplate() {
}

// static
FileTemplate FileTemplate::GetForTargetOutputs(const Target* target) {
  const Target::FileList& outputs = target->script_values().outputs();
  std::vector<std::string> output_template_args;
  for (size_t i = 0; i < outputs.size(); i++)
    output_template_args.push_back(outputs[i].value());
  return FileTemplate(output_template_args);
}

bool FileTemplate::IsTypeUsed(Subrange::Type type) const {
  DCHECK(type > Subrange::LITERAL && type < Subrange::NUM_TYPES);
  return types_required_[type];
}

void FileTemplate::Apply(const Value& sources,
                         const ParseNode* origin,
                         std::vector<Value>* dest,
                         Err* err) const {
  if (!sources.VerifyTypeIs(Value::LIST, err))
    return;
  dest->reserve(sources.list_value().size() * templates_.container().size());

  // Temporary holding place, allocate outside to re-use- buffer.
  std::vector<std::string> string_output;

  const std::vector<Value>& sources_list = sources.list_value();
  for (size_t i = 0; i < sources_list.size(); i++) {
    if (!sources_list[i].VerifyTypeIs(Value::STRING, err))
      return;

    ApplyString(sources_list[i].string_value(), &string_output);
    for (size_t out_i = 0; out_i < string_output.size(); out_i++)
      dest->push_back(Value(origin, string_output[out_i]));
  }
}

void FileTemplate::ApplyString(const std::string& str,
                               std::vector<std::string>* output) const {
  // Compute all substitutions needed so we can just do substitutions below.
  // We skip the LITERAL one since that varies each time.
  std::string subst[Subrange::NUM_TYPES];
  for (int i = 1; i < Subrange::NUM_TYPES; i++) {
    if (types_required_[i])
      subst[i] = GetSubstitution(str, static_cast<Subrange::Type>(i));
  }

  output->resize(templates_.container().size());
  for (size_t template_i = 0;
       template_i < templates_.container().size(); template_i++) {
    const Template& t = templates_[template_i];
    (*output)[template_i].clear();
    for (size_t subrange_i = 0; subrange_i < t.container().size();
         subrange_i++) {
      if (t[subrange_i].type == Subrange::LITERAL)
        (*output)[template_i].append(t[subrange_i].literal);
      else
        (*output)[template_i].append(subst[t[subrange_i].type]);
    }
  }
}

void FileTemplate::WriteWithNinjaExpansions(std::ostream& out) const {
  EscapeOptions escape_options;
  escape_options.mode = ESCAPE_NINJA_SHELL;
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
        item_str.append(EscapeString(t[subrange_i].literal, escape_options,
                                     &needs_quoting));
      } else {
        // Don't escape this since we need to preserve the $.
        item_str.append("${");
        item_str.append(GetNinjaVariableNameForType(t[subrange_i].type));
        item_str.append("}");
      }
    }

    if (needs_quoting) {
      // Need to shell quote the whole string.
      out << '"' << item_str << '"';
    } else {
      out << item_str;
    }
  }
}

void FileTemplate::WriteNinjaVariablesForSubstitution(
    std::ostream& out,
    const std::string& source,
    const EscapeOptions& escape_options) const {
  for (int i = 1; i < Subrange::NUM_TYPES; i++) {
    if (types_required_[i]) {
      Subrange::Type type = static_cast<Subrange::Type>(i);
      out << "  " << GetNinjaVariableNameForType(type) << " = ";
      EscapeStringToStream(out, GetSubstitution(source, type), escape_options);
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
    default:
      NOTREACHED();
  }
  return "";
}

// static
std::string FileTemplate::GetSubstitution(const std::string& source,
                                          Subrange::Type type) {
  switch (type) {
    case Subrange::SOURCE:
      return source;
    case Subrange::NAME_PART:
      return FindFilenameNoExtension(&source).as_string();
    case Subrange::FILE_PART:
      return FindFilename(&source).as_string();
    default:
      NOTREACHED();
  }
  return std::string();
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

    // Decode the template param.
    if (str.compare(next, arraysize(kSource) - 1, kSource) == 0) {
      t.container().push_back(Subrange(Subrange::SOURCE));
      types_required_[Subrange::SOURCE] = true;
      has_substitutions_ = true;
      cur = next + arraysize(kSource) - 1;
    } else if (str.compare(next, arraysize(kSourceNamePart) - 1,
                           kSourceNamePart) == 0) {
      t.container().push_back(Subrange(Subrange::NAME_PART));
      types_required_[Subrange::NAME_PART] = true;
      has_substitutions_ = true;
      cur = next + arraysize(kSourceNamePart) - 1;
    } else if (str.compare(next, arraysize(kSourceFilePart) - 1,
                           kSourceFilePart) == 0) {
      t.container().push_back(Subrange(Subrange::FILE_PART));
      types_required_[Subrange::FILE_PART] = true;
      has_substitutions_ = true;
      cur = next + arraysize(kSourceFilePart) - 1;
    } else {
      // If it's not a match, treat it like a one-char literal (this will be
      // rare, so it's not worth the bother to add to the previous literal) so
      // we can keep going.
      t.container().push_back(Subrange(Subrange::LITERAL, "{"));
      cur = next + 1;
    }
  }
}
