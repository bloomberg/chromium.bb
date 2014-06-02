// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_VISIBILITY_H_
#define TOOLS_GN_VISIBILITY_H_

#include <vector>

#include "base/basictypes.h"
#include "base/strings/string_piece.h"
#include "tools/gn/source_dir.h"

class Err;
class Item;
class Label;
class Scope;
class Value;

class Visibility {
 public:
  class VisPattern {
   public:
    enum Type {
      MATCH = 1,  // Exact match for a given target.
      DIRECTORY,  // Only targets in the file in the given directory.
      RECURSIVE_DIRECTORY  // The given directory and any subdir.
                           // (also indicates "public" when dir is empty).
    };

    VisPattern();
    VisPattern(Type type, const SourceDir& dir, const base::StringPiece& name);
    ~VisPattern();

    bool Matches(const Label& label) const;

    Type type() const { return type_; }
    const SourceDir& dir() const { return dir_; }
    const std::string& name() const { return name_; }

   private:
    Type type_;

    // Used when type_ == PRIVATE and PRIVATE_RECURSIVE. This specifies the
    // directory that to which the pattern is private to.
    SourceDir dir_;

    // Empty name means match everything. Otherwise the name must match
    // exactly.
    std::string name_;
  };

  // Defaults to private visibility (only the current file).
  Visibility();
  ~Visibility();

  // Set the visibility to the thing specified by the given value. On failure,
  // returns false and sets the error.
  bool Set(const SourceDir& current_dir, const Value& value, Err* err);

  // Sets the visibility to be public.
  void SetPublic();

  // Sets the visibility to be private to the given directory.
  void SetPrivate(const SourceDir& current_dir);

  // Returns true if the target with the given label can depend on one with the
  // current visibility.
  bool CanSeeMe(const Label& label) const;

  // Returns a string listing the visibility. |indent| number of spaces will
  // be added on the left side of the output. If |include_brackets| is set, the
  // result will be wrapped in "[ ]" and the contents further indented. The
  // result will end in a newline.
  std::string Describe(int indent, bool include_brackets) const;

  // Converts the given input string to a pattern. This does special stuff
  // to treat the pattern as a label. Sets the error on failure.
  static VisPattern GetPattern(const SourceDir& current_dir,
                               const Value& value,
                               Err* err);

  // Helper function to check visibility between the given two items. If
  // to is invisible to from, returns false and sets the error.
  static bool CheckItemVisibility(const Item* from, const Item* to, Err* err);

  // Helper function to fill an item's visibility from the "visibility" value
  // in the current scope.
  static bool FillItemVisibility(Item* item, Scope* scope, Err* err);

 private:
  std::vector<VisPattern> patterns_;

  DISALLOW_COPY_AND_ASSIGN(Visibility);
};

#endif  // TOOLS_GN_VISIBILITY_H_
