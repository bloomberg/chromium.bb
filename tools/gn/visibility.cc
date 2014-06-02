// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/visibility.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/item.h"
#include "tools/gn/label.h"
#include "tools/gn/scope.h"
#include "tools/gn/value.h"
#include "tools/gn/variables.h"

Visibility::VisPattern::VisPattern() : type_(MATCH) {
}

Visibility::VisPattern::VisPattern(Type type,
                                   const SourceDir& dir,
                                   const base::StringPiece& name)
    : type_(type),
      dir_(dir) {
  name.CopyToString(&name_);
}

Visibility::VisPattern::~VisPattern() {
}

bool Visibility::VisPattern::Matches(const Label& label) const {
  switch (type_) {
    case MATCH:
      return label.name() == name_ && label.dir() == dir_;
    case DIRECTORY:
      // The directories must match exactly for private visibility.
      return label.dir() == dir_;
    case RECURSIVE_DIRECTORY:
      // Our directory must be a prefix of the input label for recursive
      // private visibility.
      return label.dir().value().compare(0, dir_.value().size(), dir_.value())
          == 0;
    default:
      NOTREACHED();
      return false;
  }
}

Visibility::Visibility() {
}

Visibility::~Visibility() {
}

bool Visibility::Set(const SourceDir& current_dir,
                     const Value& value,
                     Err* err) {
  patterns_.clear();

  // Allow a single string to be passed in to make the common case (just one
  // pattern) easier to specify.
  if (value.type() == Value::STRING) {
    patterns_.push_back(GetPattern(current_dir, value, err));
    return !err->has_error();
  }

  // If it's not a string, it should be a list of strings.
  if (!value.VerifyTypeIs(Value::LIST, err))
    return false;

  const std::vector<Value>& list = value.list_value();
  for (size_t i = 0; i < list.size(); i++) {
    patterns_.push_back(GetPattern(current_dir, list[i], err));
    if (err->has_error())
      return false;
  }
  return true;
}

void Visibility::SetPublic() {
  patterns_.clear();
  patterns_.push_back(
      VisPattern(VisPattern::RECURSIVE_DIRECTORY, SourceDir(), std::string()));
}

void Visibility::SetPrivate(const SourceDir& current_dir) {
  patterns_.clear();
  patterns_.push_back(
      VisPattern(VisPattern::DIRECTORY, current_dir, std::string()));
}

bool Visibility::CanSeeMe(const Label& label) const {
  for (size_t i = 0; i < patterns_.size(); i++) {
    if (patterns_[i].Matches(label))
      return true;
  }
  return false;
}

std::string Visibility::Describe(int indent, bool include_brackets) const {
  std::string outer_indent_string(indent, ' ');

  if (patterns_.empty())
    return outer_indent_string + "[] (no visibility)\n";

  std::string result;

  std::string inner_indent_string = outer_indent_string;
  if (include_brackets) {
    result += outer_indent_string + "[\n";
    // Indent the insides more if brackets are requested.
    inner_indent_string += "  ";
  }

  for (size_t i = 0; i < patterns_.size(); i++) {
    switch (patterns_[i].type()) {
      case VisPattern::MATCH:
        result += inner_indent_string +
            DirectoryWithNoLastSlash(patterns_[i].dir()) + ":" +
            patterns_[i].name() + "\n";
        break;
      case VisPattern::DIRECTORY:
        result += inner_indent_string +
            DirectoryWithNoLastSlash(patterns_[i].dir()) + ":*\n";
        break;
      case VisPattern::RECURSIVE_DIRECTORY:
        result += inner_indent_string + patterns_[i].dir().value() + "*\n";
        break;
    }
  }

  if (include_brackets)
    result += outer_indent_string + "]\n";
  return result;
}

// static
Visibility::VisPattern Visibility::GetPattern(const SourceDir& current_dir,
                                              const Value& value,
                                              Err* err) {
  if (!value.VerifyTypeIs(Value::STRING, err))
    return VisPattern();

  const std::string& str = value.string_value();
  if (str.empty()) {
    *err = Err(value, "Visibility pattern must not be empty.");
    return VisPattern();
  }

  // If there's no wildcard, this is specifying an exact label, use the
  // label resolution code to get all the implicit name stuff.
  size_t star = str.find('*');
  if (star == std::string::npos) {
    Label label = Label::Resolve(current_dir, Label(), value, err);
    if (err->has_error())
      return VisPattern();

    // There should be no toolchain specified.
    if (!label.toolchain_dir().is_null() || !label.toolchain_name().empty()) {
      *err = Err(value, "Visibility label specified a toolchain.",
          "Visibility names and patterns don't use toolchains, erase the\n"
          "stuff in the ().");
      return VisPattern();
    }

    return VisPattern(VisPattern::MATCH, label.dir(), label.name());
  }

  // Wildcard case, need to split apart the label to see what it specifies.
  base::StringPiece path;
  base::StringPiece name;
  size_t colon = str.find(':');
  if (colon == std::string::npos) {
    path = base::StringPiece(str);
  } else {
    path = base::StringPiece(&str[0], colon);
    name = base::StringPiece(&str[colon + 1], str.size() - colon - 1);
  }

  // The path can have these forms:
  //   1. <empty>  (use current dir)
  //   2. <non wildcard stuff>  (send through directory resolution)
  //   3. <non wildcard stuff>*  (send stuff through dir resolution, note star)
  //   4. *  (matches anything)
  SourceDir dir;
  bool has_path_star = false;
  if (path.empty()) {
    // Looks like ":foo".
    dir = current_dir;
  } else if (path[path.size() - 1] == '*') {
    // Case 3 or 4 above.
    has_path_star = true;

    // Adjust path to contain everything but the star.
    path = path.substr(0, path.size() - 1);

    if (!path.empty() && path[path.size() - 1] != '/') {
      // The input was "foo*" which is invalid.
      *err = Err(value, "'*' must match full directories in visibility.",
          "You did \"foo*\" but visibility doesn't do general pattern\n"
          "matching. Instead, you have to add a slash: \"foo/*\".");
      return VisPattern();
    }
  }

  // Resolve the part of the path that's not the wildcard.
  if (!path.empty()) {
    // The non-wildcard stuff better not have a wildcard.
    if (path.find('*') != base::StringPiece::npos) {
      *err = Err(value, "Visibility patterns only support wildcard suffixes.",
          "The visibility pattern contained a '*' that wasn't at tne end.");
      return VisPattern();
    }

    // Resolve the non-wildcard stuff.
    dir = current_dir.ResolveRelativeDir(path);
    if (dir.is_null()) {
      *err = Err(value, "Visibility pattern didn't resolve to a dir.",
          "The directory name \"" + path.as_string() + "\" didn't\n"
          "resolve to a directory.");
      return VisPattern();
    }
  }

  // Resolve the name. At this point, we're doing wildcard matches so the
  // name should either be empty ("foo/*") or a wildcard ("foo:*");
  if (colon != std::string::npos && name != "*") {
    *err = Err(value, "Invalid visibility pattern.",
        "You seem to be using the wildcard more generally that is supported.\n"
        "Did you mean \"foo:*\" to match everything in the current file, or\n"
        "\"./*\" to recursively match everything in the currend subtree.");
    return VisPattern();
  }

  VisPattern::Type type;
  if (has_path_star) {
    // We know there's a wildcard, so if the name is empty it looks like
    // "foo/*".
    type = VisPattern::RECURSIVE_DIRECTORY;
  } else {
    // Everything else should be of the form "foo:*".
    type = VisPattern::DIRECTORY;
  }

  // When we're doing wildcard matching, the name is always empty.
  return VisPattern(type, dir, base::StringPiece());
}

// static
bool Visibility::CheckItemVisibility(const Item* from,
                                     const Item* to,
                                     Err* err) {
  if (!to->visibility().CanSeeMe(from->label())) {
    std::string to_label = to->label().GetUserVisibleName(false);
    *err = Err(from->defined_from(), "Dependency not allowed.",
        "The item " + from->label().GetUserVisibleName(false) + "\n"
        "can not depend on " + to_label + "\n"
        "because it is not in " + to_label + "'s visibility list: " +
        to->visibility().Describe(0, true));
    return false;
  }
  return true;
}

// static
bool Visibility::FillItemVisibility(Item* item, Scope* scope, Err* err) {
  const Value* vis_value = scope->GetValue(variables::kVisibility, true);
  if (vis_value)
    item->visibility().Set(scope->GetSourceDir(), *vis_value, err);
  else  // Default to public.
    item->visibility().SetPublic();
  return !err->has_error();
}
