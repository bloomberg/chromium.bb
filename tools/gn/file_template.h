// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_FILE_TEMPLATE_H_
#define TOOLS_GN_FILE_TEMPLATE_H_

#include <iosfwd>

#include "base/basictypes.h"
#include "base/containers/stack_container.h"
#include "tools/gn/err.h"
#include "tools/gn/value.h"

struct EscapeOptions;
class ParseNode;
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
  struct Subrange {
    enum Type {
      LITERAL = 0,

      // {{source}} -> expands to be the source file name relative to the build
      // root dir.
      SOURCE,

      // {{source_name_part}} -> file name without extension or directory.
      // Maps "foo/bar.txt" to "bar".
      NAME_PART,

      // {{source_file_part}} -> file name including extension but no directory.
      // Maps "foo/bar.txt" to "bar.txt".
      FILE_PART,

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
  FileTemplate(const Value& t, Err* err);
  FileTemplate(const std::vector<std::string>& t);
  ~FileTemplate();

  // Returns an output template representing the given target's script
  // outputs.
  static FileTemplate GetForTargetOutputs(const Target* target);

  // Returns true if the given substitution type is used by this template.
  bool IsTypeUsed(Subrange::Type type) const;

  // Returns true if there are any substitutions.
  bool has_substitutions() const { return has_substitutions_; }

  // Applies this template to the given list of sources, appending all
  // results to the given dest list. The sources must be a list for the
  // one that takes a value as an input, otherwise the given error will be set.
  void Apply(const Value& sources,
             const ParseNode* origin,
             std::vector<Value>* dest,
             Err* err) const;
  void ApplyString(const std::string& input,
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
      const std::string& source,
      const EscapeOptions& escape_options) const;

  // Returns the Ninja variable name used by the above Ninja functions to
  // substitute for the given type.
  static const char* GetNinjaVariableNameForType(Subrange::Type type);

  // Extracts the given type of substitution from the given source. The source
  // should be the file name relative to the output directory.
  static std::string GetSubstitution(const std::string& source,
                                     Subrange::Type type);

  // Known template types, these include the "{{ }}"
  static const char kSource[];
  static const char kSourceNamePart[];
  static const char kSourceFilePart[];

 private:
  typedef base::StackVector<Subrange, 8> Template;
  typedef base::StackVector<Template, 8> TemplateVector;

  void ParseInput(const Value& value, Err* err);

  // Parses a template string and adds it to the templates_ list.
  void ParseOneTemplateString(const std::string& str);

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
