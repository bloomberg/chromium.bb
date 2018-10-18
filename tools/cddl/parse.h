// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CDDL_PARSE_H_
#define TOOLS_CDDL_PARSE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

struct AstNode {
  // These types all correspond to types in the grammar, which can be found in
  // grammar.abnf.
  enum class Type {
    kRule,
    kTypename,
    kGroupname,
    kAssign,
    kAssignT,
    kAssignG,
    kType,
    kGrpent,
    kType1,
    kType2,
    kValue,
    kGroup,
    kUint,
    kDigit,
    kRangeop,
    kCtlop,
    kGrpchoice,
    kOccur,
    kMemberKey,
    kId,
    kNumber,
    kText,
    kBytes,
    kOther,
  };

  AstNode* children;
  AstNode* sibling;
  Type type;
  std::string text;
};

struct ParseResult {
  AstNode* root;
  std::vector<std::unique_ptr<AstNode>> nodes;
};

ParseResult ParseCddl(std::string& data);
void DumpAst(AstNode* node, int indent_level = 0);

#endif  // TOOLS_CDDL_PARSE_H_
