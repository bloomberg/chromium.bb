// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/cddl/parse.h"

#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "platform/api/logging.h"
#include "third_party/abseil/src/absl/strings/ascii.h"
#include "third_party/abseil/src/absl/strings/match.h"

static_assert(sizeof(absl::string_view::size_type) == sizeof(size_t),
              "We assume string_view's size_type is the same as size_t. If "
              "not, the following file needs to be refactored");

// All of the parsing methods in this file that operate on Parser are named
// either Parse* or Skip* and are named according to the CDDL grammar in
// grammar.abnf.  Similarly, methods like ParseMemberKey1 attempt to parse the
// first choice in the memberkey rule.
struct Parser {
  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;

  const char* data;
  std::vector<std::unique_ptr<AstNode>> nodes;
};

AstNode* AddNode(Parser* p,
                 AstNode::Type type,
                 absl::string_view text,
                 AstNode* children = nullptr) {
  p->nodes.emplace_back(new AstNode);
  AstNode* node = p->nodes.back().get();
  node->children = children;
  node->sibling = nullptr;
  node->type = type;
  node->text = std::string(text);
  return node;
}

bool IsBinaryDigit(char x) {
  return '0' == x || x == '1';
}

bool IsExtendedAlpha(char x) {
  return absl::ascii_isalpha(x) || x == '@' || x == '_' || x == '$';
}

bool IsNewline(char x) {
  return x == '\r' || x == '\n';
}

absl::string_view SkipNewline(absl::string_view view) {
  size_t index = 0;
  while (IsNewline(view[index])) {
    ++index;
  }

  return view.substr(index);
}

absl::string_view SkipComment(absl::string_view view) {
  size_t index = 0;
  if (view[index] == ';') {
    ++index;
    while (!IsNewline(view[index]) && index < view.length()) {
      OSP_CHECK(absl::ascii_isprint(view[index]))
          << "Comments should only contain printable characters";
      ++index;
    }

    while (IsNewline(view[index])) {
      ++index;
    }
  }

  return view.substr(index);
}

bool IsWhitespace(char c) {
  return c == ' ' || c == ';' || c == '\r' || c == '\n';
}

void SkipWhitespaceAndComments(Parser* p) {
  absl::string_view view = p->data;
  absl::string_view new_view;

  while (true) {
    new_view = SkipComment(view);
    if (new_view.data() == view.data()) {
      new_view = absl::StripLeadingAsciiWhitespace(view);
    }

    if (new_view == view) {
      break;
    }

    view = new_view;
  }

  p->data = new_view.data();
}

enum class AssignType {
  kInvalid = -1,
  kAssign,
  kAssignT,
  kAssignG,
};

AssignType ParseAssignmentType(Parser* p) {
  const char* it = p->data;
  if (it[0] == '=') {
    p->data = it + 1;
    return AssignType::kAssign;
  } else if (it[0] == '/') {
    ++it;
    if (it[0] == '/' && it[1] == '=') {
      p->data = it + 2;
      return AssignType::kAssignG;
    } else if (it[0] == '=') {
      p->data = it + 1;
      return AssignType::kAssignT;
    }
  }
  return AssignType::kInvalid;
}

AstNode* ParseType1(Parser* p);
AstNode* ParseType(Parser* p);
AstNode* ParseId(Parser* p);

void SkipUint(Parser* p) {
  absl::string_view view = p->data;

  bool is_binary = false;
  size_t index = 0;
  if (absl::StartsWith(view, "0b")) {
    is_binary = true;
    index = 2;
  } else if (absl::StartsWith(view, "0x")) {
    index = 2;
  }

  while (index < view.length() && absl::ascii_isdigit(view[index])) {
    if (is_binary) {
      OSP_CHECK(IsBinaryDigit(view[index]))
          << "Binary numbers should be comprised of only 0 or 1";
    }

    ++index;
  }

  p->data = view.substr(index).data();
}

AstNode* ParseNumber(Parser* p) {
  Parser p_speculative{p->data};
  if (!absl::ascii_isdigit(p_speculative.data[0]) &&
      p_speculative.data[0] != '-') {
    // TODO(btolsch): hexfloat, fraction, exponent.
    return nullptr;
  }
  if (p_speculative.data[0] == '-') {
    ++p_speculative.data;
  }

  SkipUint(&p_speculative);

  AstNode* node =
      AddNode(p, AstNode::Type::kNumber,
              absl::string_view(p->data, p_speculative.data - p->data));
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

AstNode* ParseText(Parser* p) {
  return nullptr;
}

AstNode* ParseBytes(Parser* p) {
  return nullptr;
}

// Returns whether |c| could be the first character in a valid "value" string.
// This is not a guarantee however, since 'h' and 'b' could also indicate the
// start of an ID, but value needs to be tried first.
bool IsValue(char c) {
  return (c == '-' || absl::ascii_isdigit(c) ||  // FIRST(number)
          c == '"' ||                            // FIRST(text)
          c == '\'' || c == 'h' || c == 'b');    // FIRST(bytes)
}

AstNode* ParseValue(Parser* p) {
  AstNode* node = ParseNumber(p);
  if (!node) {
    node = ParseText(p);
  }
  if (!node) {
    ParseBytes(p);
  }
  return node;
}

AstNode* ParseOccur(Parser* p) {
  if (p->data[0] != '*' && p->data[0] != '?') {
    return nullptr;
  }
  AstNode* node =
      AddNode(p, AstNode::Type::kOccur, absl::string_view(p->data, 1));
  ++p->data;
  return node;
}

AstNode* ParseMemberKey1(Parser* p) {
  Parser p_speculative{p->data};
  if (!ParseType1(&p_speculative)) {
    return nullptr;
  }

  SkipWhitespaceAndComments(&p_speculative);

  if (*p_speculative.data++ != '=' || *p_speculative.data++ != '>') {
    return nullptr;
  }
  AstNode* node =
      AddNode(p, AstNode::Type::kMemberKey,
              absl::string_view(p->data, p_speculative.data - p->data));
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

AstNode* ParseMemberKey2(Parser* p) {
  Parser p_speculative{p->data};
  AstNode* id = ParseId(&p_speculative);
  if (!id) {
    return nullptr;
  }

  SkipWhitespaceAndComments(&p_speculative);

  if (*p_speculative.data++ != ':') {
    return nullptr;
  }

  AstNode* node =
      AddNode(p, AstNode::Type::kMemberKey,
              absl::string_view(p->data, p_speculative.data - p->data), id);
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

AstNode* ParseMemberKey3(Parser* p) {
  Parser p_speculative{p->data};
  AstNode* value = ParseValue(&p_speculative);
  if (!value) {
    return nullptr;
  }

  SkipWhitespaceAndComments(&p_speculative);

  if (*p_speculative.data++ != ':') {
    return nullptr;
  }
  AstNode* node =
      AddNode(p, AstNode::Type::kMemberKey,
              absl::string_view(p->data, p_speculative.data - p->data), value);
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

AstNode* ParseMemberKey(Parser* p) {
  AstNode* node = ParseMemberKey1(p);
  if (!node) {
    node = ParseMemberKey2(p);
  }
  if (!node) {
    node = ParseMemberKey3(p);
  }
  return node;
}

AstNode* ParseGroupEntry(Parser* p);

bool SkipOptionalComma(Parser* p) {
  SkipWhitespaceAndComments(p);
  if (p->data[0] == ',') {
    ++p->data;
    SkipWhitespaceAndComments(p);
  }
  return true;
}

AstNode* ParseGroupChoice(Parser* p) {
  Parser p_speculative{p->data};
  AstNode* tail = nullptr;
  AstNode* group_node =
      AddNode(&p_speculative, AstNode::Type::kGrpchoice, absl::string_view());
  const char* group_node_text = p_speculative.data;
  while (true) {
    const char* orig = p_speculative.data;
    AstNode* group_entry = ParseGroupEntry(&p_speculative);
    if (!group_entry) {
      p_speculative.data = orig;
      if (!group_node->children) {
        return nullptr;
      }
      group_node->text =
          std::string(group_node_text, p_speculative.data - group_node_text);
      p->data = p_speculative.data;
      std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
                std::back_inserter(p->nodes));
      return group_node;
    }
    if (!group_node->children) {
      group_node->children = group_entry;
    }
    if (tail) {
      tail->sibling = group_entry;
    }
    tail = group_entry;
    if (!SkipOptionalComma(&p_speculative)) {
      return nullptr;
    }
  }
  return nullptr;
}

AstNode* ParseGroup(Parser* p) {
  const char* orig = p->data;
  AstNode* group_choice = ParseGroupChoice(p);
  if (!group_choice) {
    return nullptr;
  }
  return AddNode(p, AstNode::Type::kGroup,
                 absl::string_view(orig, p->data - orig), group_choice);
}

AstNode* ParseType2(Parser* p) {
  const char* orig = p->data;
  const char* it = p->data;
  AstNode* node = AddNode(p, AstNode::Type::kType2, absl::string_view());
  if (IsValue(it[0])) {
    AstNode* value = ParseValue(p);
    if (!value) {
      if (it[0] == 'h' || it[0] == 'b') {
        AstNode* id = ParseId(p);
        if (!id) {
          return nullptr;
        }
        id->type = AstNode::Type::kTypename;
        node->children = id;
      } else {
        return nullptr;
      }
    } else {
      node->children = value;
    }
  } else if (IsExtendedAlpha(it[0])) {
    AstNode* id = ParseId(p);
    if (!id) {
      return nullptr;
    }
    if (p->data[0] == '<') {
      std::cerr << "It looks like you're trying to use a generic argument, "
                   "which we don't support"
                << std::endl;
      return nullptr;
    }
    id->type = AstNode::Type::kTypename;
    node->children = id;
  } else if (it[0] == '(') {
    p->data = it + 1;
    SkipWhitespaceAndComments(p);
    AstNode* type = ParseType(p);
    if (!type) {
      return nullptr;
    }
    SkipWhitespaceAndComments(p);
    if (p->data[0] != ')') {
      return nullptr;
    }
    ++p->data;
    node->children = type;
  } else if (it[0] == '{') {
    p->data = it + 1;
    SkipWhitespaceAndComments(p);
    AstNode* group = ParseGroup(p);
    if (!group) {
      return nullptr;
    }
    SkipWhitespaceAndComments(p);
    if (p->data[0] != '}') {
      return nullptr;
    }
    ++p->data;
    node->children = group;
  } else if (it[0] == '[') {
    p->data = it + 1;
    SkipWhitespaceAndComments(p);
    AstNode* group = ParseGroup(p);
    if (!group) {
      return nullptr;
    }
    SkipWhitespaceAndComments(p);
    if (p->data[0] != ']') {
      return nullptr;
    }
    ++p->data;
    node->children = group;
  } else if (it[0] == '~') {
    p->data = it + 1;
    SkipWhitespaceAndComments(p);
    if (!ParseId(p)) {
      return nullptr;
    }
    if (p->data[0] == '<') {
      std::cerr << "It looks like you're trying to use a generic argument, "
                   "which we don't support"
                << std::endl;
      return nullptr;
    }
  } else if (it[0] == '&') {
    p->data = it + 1;
    SkipWhitespaceAndComments(p);
    if (p->data[0] == '(') {
      ++p->data;
      SkipWhitespaceAndComments(p);
      AstNode* group = ParseGroup(p);
      if (!group) {
        return nullptr;
      }
      SkipWhitespaceAndComments(p);
      if (p->data[0] != ')') {
        return nullptr;
      }
      ++p->data;
      node->children = group;
    } else {
      AstNode* id = ParseId(p);
      if (id) {
        if (p->data[0] == '<') {
          std::cerr << "It looks like you're trying to use a generic argument, "
                       "which we don't support"
                    << std::endl;
          return nullptr;
        }
        node->children = id;
      } else {
        return nullptr;
      }
    }
  } else if (it[0] == '#') {
    ++it;
    if (it[0] == '6') {
      ++it;
      if (it[0] == '.') {
        p->data = it + 1;
        SkipUint(p);
        it = p->data;
      }
      if (it[0] != '(') {
        return nullptr;
      }
      p->data = ++it;
      SkipWhitespaceAndComments(p);
      AstNode* type = ParseType(p);
      if (!type) {
        return nullptr;
      }
      SkipWhitespaceAndComments(p);
      if (p->data[0] != ')') {
        return nullptr;
      }
      ++p->data;
      node->children = type;
    } else if (absl::ascii_isdigit(it[0])) {
      std::cerr << "# MAJOR unimplemented" << std::endl;
      return nullptr;
    } else {
      p->data = it;
    }
  } else {
    return nullptr;
  }
  node->text = std::string(orig, p->data - orig);
  return node;
}

AstNode* ParseType1(Parser* p) {
  const char* orig = p->data;
  AstNode* type2 = ParseType2(p);
  if (!type2) {
    return nullptr;
  }
  // TODO(btolsch): rangeop + ctlop
  // if (!HandleSpace(p)) {
  //   return false;
  // }
  return AddNode(p, AstNode::Type::kType1,
                 absl::string_view(orig, p->data - orig), type2);
}

AstNode* ParseType(Parser* p) {
  Parser p_speculative{p->data};
  AstNode* type1 = ParseType1(&p_speculative);
  if (!type1) {
    return nullptr;
  }

  SkipWhitespaceAndComments(&p_speculative);

  AstNode* tail = type1;
  while (*p_speculative.data == '/') {
    ++p_speculative.data;
    SkipWhitespaceAndComments(&p_speculative);

    AstNode* next_type1 = ParseType1(&p_speculative);
    if (!next_type1) {
      return nullptr;
    }
    tail->sibling = next_type1;
    tail = next_type1;
    SkipWhitespaceAndComments(&p_speculative);
  }
  AstNode* node =
      AddNode(p, AstNode::Type::kType,
              absl::string_view(p->data, p_speculative.data - p->data), type1);
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

AstNode* ParseId(Parser* p) {
  const char* id = p->data;
  if (!IsExtendedAlpha(id[0])) {
    return nullptr;
  }
  const char* it = id + 1;
  while (true) {
    if (it[0] == '-' || it[0] == '.') {
      ++it;
      if (!IsExtendedAlpha(it[0]) && !absl::ascii_isdigit(it[0])) {
        return nullptr;
      }
      ++it;
    } else if (IsExtendedAlpha(it[0]) || absl::ascii_isdigit(it[0])) {
      ++it;
    } else {
      break;
    }
  }
  AstNode* node =
      AddNode(p, AstNode::Type::kId, absl::string_view(p->data, it - p->data));
  p->data = it;
  return node;
}

AstNode* ParseGroupEntry1(Parser* p) {
  Parser p_speculative{p->data};
  AstNode* occur = ParseOccur(&p_speculative);
  if (occur) {
    SkipWhitespaceAndComments(&p_speculative);
  }
  AstNode* member_key = ParseMemberKey(&p_speculative);
  if (member_key) {
    SkipWhitespaceAndComments(&p_speculative);
  }
  AstNode* type = ParseType(&p_speculative);
  if (!type) {
    return nullptr;
  }
  AstNode* node = AddNode(p, AstNode::Type::kGrpent, absl::string_view());
  if (occur) {
    node->children = occur;
    if (member_key) {
      occur->sibling = member_key;
      member_key->sibling = type;
    } else {
      occur->sibling = type;
    }
  } else if (member_key) {
    node->children = member_key;
    member_key->sibling = type;
  } else {
    node->children = type;
  }
  node->text = std::string(p->data, p_speculative.data - p->data);
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

// NOTE: This should probably never be hit, why is it in the grammar?
AstNode* ParseGroupEntry2(Parser* p) {
  Parser p_speculative{p->data};
  AstNode* occur = ParseOccur(&p_speculative);
  if (occur) {
    SkipWhitespaceAndComments(&p_speculative);
  }
  AstNode* id = ParseId(&p_speculative);
  if (!id) {
    return nullptr;
  }
  id->type = AstNode::Type::kGroupname;
  if (*p_speculative.data == '<') {
    std::cerr << "It looks like you're trying to use a generic argument, "
                 "which we don't support"
              << std::endl;
    return nullptr;
  }
  AstNode* node = AddNode(p, AstNode::Type::kGrpent, absl::string_view());
  if (occur) {
    occur->sibling = id;
    node->children = occur;
  } else {
    node->children = id;
  }
  node->text = std::string(p->data, p_speculative.data - p->data);
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

AstNode* ParseGroupEntry3(Parser* p) {
  Parser p_speculative{p->data};
  AstNode* occur = ParseOccur(&p_speculative);
  if (occur) {
    SkipWhitespaceAndComments(&p_speculative);
  }
  if (*p_speculative.data != '(') {
    return nullptr;
  }
  ++p_speculative.data;
  SkipWhitespaceAndComments(&p_speculative);
  AstNode* group = ParseGroup(&p_speculative);
  if (!group) {
    return nullptr;
  }

  SkipWhitespaceAndComments(&p_speculative);
  if (*p_speculative.data != ')') {
    return nullptr;
  }
  ++p_speculative.data;
  AstNode* node = AddNode(p, AstNode::Type::kGrpent, absl::string_view());
  if (occur) {
    node->children = occur;
    occur->sibling = group;
  } else {
    node->children = group;
  }
  node->text = std::string(p->data, p_speculative.data - p->data);
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

AstNode* ParseGroupEntry(Parser* p) {
  AstNode* node = ParseGroupEntry1(p);
  if (!node) {
    node = ParseGroupEntry2(p);
  }
  if (!node) {
    node = ParseGroupEntry3(p);
  }
  return node;
}

AstNode* ParseRule(Parser* p) {
  const char* start = p->data;
  AstNode* id = ParseId(p);
  if (!id) {
    return nullptr;
  }
  if (p->data[0] == '<') {
    std::cerr << "It looks like you're trying to use a generic parameter, "
                 "which we don't support"
              << std::endl;
    return nullptr;
  }
  SkipWhitespaceAndComments(p);
  const char* assign_start = p->data;
  AssignType assign_type = ParseAssignmentType(p);
  if (assign_type != AssignType::kAssign) {
    return nullptr;
  }
  AstNode* assign_node = AddNode(
      p,
      (assign_type == AssignType::kAssign)
          ? AstNode::Type::kAssign
          : (assign_type == AssignType::kAssignT) ? AstNode::Type::kAssignT
                                                  : AstNode::Type::kAssignG,
      absl::string_view(assign_start, p->data - assign_start));
  id->sibling = assign_node;

  SkipWhitespaceAndComments(p);
  AstNode* type = ParseType(p);
  id->type = AstNode::Type::kTypename;
  if (!type) {
    type = ParseGroupEntry(p);
    id->type = AstNode::Type::kGroupname;
    if (!type) {
      return nullptr;
    }
  }
  assign_node->sibling = type;
  SkipWhitespaceAndComments(p);
  return AddNode(p, AstNode::Type::kRule,
                 absl::string_view(start, p->data - start), id);
}

ParseResult ParseCddl(absl::string_view data) {
  if (data[0] == 0) {
    return {nullptr, {}};
  }
  Parser p{(char*)data.data()};

  SkipWhitespaceAndComments(&p);
  AstNode* root = nullptr;
  AstNode* tail = nullptr;
  do {
    AstNode* next = ParseRule(&p);
    if (!next) {
      return {nullptr, {}};
    }
    if (!root) {
      root = next;
    }
    if (tail) {
      tail->sibling = next;
    }
    tail = next;

    SkipWhitespaceAndComments(&p);
  } while (p.data[0]);
  return {root, std::move(p.nodes)};
}

void PrintCollapsed(int size, absl::string_view text) {
  for (int i = 0; i < size; ++i) {
    if (text[i] == ' ' || text[i] == '\n') {
      printf(" ");
      while (i < size && (text[i] == ' ' || text[i] == '\n')) {
        ++i;
      }
    }
    if (i < size) {
      printf("%c", text[i]);
    }
  }
  printf("\n");
}

void DumpAst(AstNode* node, int indent_level) {
  while (node) {
    for (int i = 0; i <= indent_level; ++i) {
      printf("--");
    }
    switch (node->type) {
      case AstNode::Type::kRule:
        printf("kRule: ");
        break;
      case AstNode::Type::kTypename:
        printf("kTypename: ");
        break;
      case AstNode::Type::kGroupname:
        printf("kGroupname: ");
        break;
      case AstNode::Type::kAssign:
        printf("kAssign: ");
        break;
      case AstNode::Type::kAssignT:
        printf("kAssignT: ");
        break;
      case AstNode::Type::kAssignG:
        printf("kAssignG: ");
        break;
      case AstNode::Type::kType:
        printf("kType: ");
        break;
      case AstNode::Type::kGrpent:
        printf("kGrpent: ");
        break;
      case AstNode::Type::kType1:
        printf("kType1: ");
        break;
      case AstNode::Type::kType2:
        printf("kType2: ");
        break;
      case AstNode::Type::kValue:
        printf("kValue: ");
        break;
      case AstNode::Type::kGroup:
        printf("kGroup: ");
        break;
      case AstNode::Type::kUint:
        printf("kUint: ");
        break;
      case AstNode::Type::kDigit:
        printf("kDigit: ");
        break;
      case AstNode::Type::kRangeop:
        printf("kRangeop: ");
        break;
      case AstNode::Type::kCtlop:
        printf("kCtlop: ");
        break;
      case AstNode::Type::kGrpchoice:
        printf("kGrpchoice: ");
        break;
      case AstNode::Type::kOccur:
        printf("kOccur: ");
        break;
      case AstNode::Type::kMemberKey:
        printf("kMemberKey: ");
        break;
      case AstNode::Type::kId:
        printf("kId: ");
        break;
      case AstNode::Type::kNumber:
        printf("kNumber: ");
        break;
      case AstNode::Type::kText:
        printf("kText: ");
        break;
      case AstNode::Type::kBytes:
        printf("kBytes: ");
        break;
      case AstNode::Type::kOther:
        printf("kOther: ");
        break;
    }
    fflush(stdout);
    PrintCollapsed(static_cast<int>(node->text.size()), node->text.data());
    DumpAst(node->children, indent_level + 1);
    node = node->sibling;
  }
}
