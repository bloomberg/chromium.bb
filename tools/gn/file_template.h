// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_FILE_TEMPLATE_H_
#define TOOLS_GN_FILE_TEMPLATE_H_

#include <iosfwd>

#include "base/basictypes.h"
#include "base/containers/stack_container.h"
#include "tools/gn/err.h"
#include "tools/gn/source_dir.h"
#include "tools/gn/value.h"

struct EscapeOptions;
class ParseNode;
class Settings;
class SourceFile;
class Target;

extern const char kSourceExpansion_Help[];

// A FileTemplate object implements source expansion for a given "template"
// (either outputs or args, depending on the target type).
//
// There are two ways you can use this. You can make a template and then
// apply a source to it to get a list of outputs manually. Or you can do the
// actual substitution in Ninja, writing the arguments in a rule and using
// variables in build statements to invoke the rule with the right
// substitutions.
class FileTemplate {
 public:
  enum OutputStyle {
    OUTPUT_ABSOLUTE,  // Dirs will be absolute "//foo/bar".
    OUTPUT_RELATIVE,  // Dirs will be relative to a given directory.
  };

  struct Subrange {
    // See the help in the .cc file for what these mean.
    enum Type {
      LITERAL = 0,

      SOURCE,  // {{source}}
      NAME_PART,  // {{source_name_part}}
      FILE_PART,  // {{source_file_part}}
      SOURCE_DIR,  // {{source_dir}}
      ROOT_RELATIVE_DIR,  // {{root_relative_dir}}
      SOURCE_GEN_DIR,  // {{source_gen_dir}}
      SOURCE_OUT_DIR,  // {{source_out_dir}}

      NUM_TYPES  // Must be last
    };
    Subrange(Type t, const std::string& l = std::string())
        : type(t),
          literal(l) {
    }

    Type type;

    // When type_ == LITERAL, this specifies the literal.
    std::string literal;
  };

  // Constructs a template from the given value. On error, the err will be
  // set. In this case you should not use this object.
  FileTemplate(const Settings* settings,
               const Value& t,
               OutputStyle output_style,
               const SourceDir& relative_to,
               Err* err);
  FileTemplate(const Settings* settings,
               const std::vector<std::string>& t,
               OutputStyle output_style,
               const SourceDir& relative_to);
  FileTemplate(const Settings* settings,
               const std::vector<SourceFile>& t,
               OutputStyle output_style,
               const SourceDir& relative_to);

  ~FileTemplate();

  // Returns an output template representing the given target's script
  // outputs.
  static FileTemplate GetForTargetOutputs(const Target* target);

  // Returns true if the given substitution type is used by this template.
  bool IsTypeUsed(Subrange::Type type) const;

  // Returns true if there are any substitutions.
  bool has_substitutions() const { return has_substitutions_; }

  // Applies the template to one source file. The results will be *appended* to
  // the output.
  void Apply(const SourceFile& source,
             std::vector<std::string>* output) const;

  // Writes a string representing the template with Ninja variables for the
  // substitutions, and the literals escaped for Ninja consumption.
  //
  // For example, if the input is "foo{{source_name_part}}bar" this will write
  // foo${source_name_part}bar. If there are multiple templates (we were
  // constucted with a list of more than one item) then the values will be
  // separated by spaces.
  //
  // If this template is nonempty, we will first print out a space to separate
  // it from the previous command.
  //
  // The names used for the Ninja variables will be the same ones used by
  // WriteNinjaVariablesForSubstitution. You would use this to define the Ninja
  // rule, and then define the variables to substitute for each file using
  // WriteNinjaVariablesForSubstitution.
  void WriteWithNinjaExpansions(std::ostream& out) const;

  // Writes to the given stream the variable declarations for extracting the
  // required parts of the given source file string. The results will be
  // indented two spaces.
  //
  // This is used to set up a build statement to invoke a rule where the rule
  // contains a representation of this file template to be expanded by Ninja
  // (see GetWithNinjaExpansions).
  void WriteNinjaVariablesForSubstitution(
      std::ostream& out,
      const SourceFile& source,
      const EscapeOptions& escape_options) const;

  // Returns the Ninja variable name used by the above Ninja functions to
  // substitute for the given type.
  static const char* GetNinjaVariableNameForType(Subrange::Type type);

  // Extracts the given type of substitution from the given source file.
  // If output_style is OUTPUT_RELATIVE, relative_to indicates the directory
  // that the relative directories should be relative to, otherwise it is
  // ignored.
  static std::string GetSubstitution(const Settings* settings,
                                     const SourceFile& source,
                                     Subrange::Type type,
                                     OutputStyle output_style,
                                     const SourceDir& relative_to);

  // Known template types, these include the "{{ }}".
  // IF YOU ADD NEW ONES: If the expansion expands to something inside the
  // output directory, also update EnsureStringIsInOutputDir.
  static const char kSource[];
  static const char kSourceNamePart[];
  static const char kSourceFilePart[];
  static const char kSourceDir[];
  static const char kRootRelDir[];
  static const char kSourceGenDir[];
  static const char kSourceOutDir[];

 private:
  typedef base::StackVector<Subrange, 8> Template;
  typedef base::StackVector<Template, 8> TemplateVector;

  void ParseInput(const Value& value, Err* err);

  // Parses a template string and adds it to the templates_ list.
  void ParseOneTemplateString(const std::string& str);

  const Settings* settings_;
  OutputStyle output_style_;
  SourceDir relative_to_;

  TemplateVector templates_;

  // The corresponding value is set to true if the given subrange type is
  // required. This allows us to precompute these types whem applying them
  // to a given source file.
  bool types_required_[Subrange::NUM_TYPES];

  // Set when any of the types_required_ is true. Otherwise, everythins is a
  // literal (a common case so we can optimize some code paths).
  bool has_substitutions_;
};

#endif  // TOOLS_GN_FILE_TEMPLATE_H_
