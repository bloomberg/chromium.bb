// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/cddl/parse.h"

#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

// All of the parsing methods in this file that operate on Parser are named
// either Parse* or Skip* and are named according to the CDDL grammar in
// grammar.abnf.  Similarly, methods like ParseMemberKey1 attempt to parse the
// first choice in the memberkey rule.
struct Parser {
  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;

  char* data;
  std::vector<std::unique_ptr<AstNode>> nodes;
};

AstNode* AddNode(Parser* p,
                 AstNode::Type type,
                 std::string&& text,
                 AstNode* children = nullptr) {
  p->nodes.emplace_back(new AstNode);
  AstNode* node = p->nodes.back().get();
  node->children = children;
  node->sibling = nullptr;
  node->type = type;
  node->text = std::move(text);
  return node;
}

bool IsBinaryDigit(char x) {
  return '0' == x || x == '1';
}

bool IsDigit1(char x) {
  return '1' <= x && x <= '9';
}

bool IsDigit(char x) {
  return '0' <= x && x <= '9';
}

bool IsExtendedAlpha(char x) {
  return (0x41 <= x && x <= 0x5a) || (0x61 <= x && x <= 0x7a) || x == '@' ||
         x == '_' || x == '$';
}

// TODO(btolsch): UTF-8
bool IsPchar(char x) {
  return x >= 0x20;
}

char* SkipNewline(char* x) {
  if (x[0] == '\n') {
    return x + 1;
  } else if (x[0] == '\r' && x[1] == '\n') {
    return x + 2;
  }
  return x;
}

bool SkipComment(Parser* p) {
  char* it = p->data;
  if (it[0] != ';') {
    return false;
  }
  ++it;
  while (true) {
    char* crlf_skip = nullptr;
    if (IsPchar(it[0])) {
      ++it;
      continue;
    } else if ((crlf_skip = SkipNewline(it)) != it) {
      it = crlf_skip;
      break;
    }
    return false;
  }
  p->data = it;
  return true;
}

bool IsWhitespace(char c) {
  return c == ' ' || c == ';' || c == '\r' || c == '\n';
}

bool SkipWhitespace(Parser* p) {
  char* it = p->data;
  while (IsWhitespace(it[0])) {
    char* crlf_skip = SkipNewline(it);
    if (it[0] == ' ') {
      ++it;
      continue;
    } else if (crlf_skip != it) {
      it = crlf_skip;
      continue;
    }
    p->data = it;
    if (!SkipComment(p)) {
      return false;
    }
    it = p->data;
  }
  p->data = it;
  return true;
}

enum class AssignType {
  kInvalid = -1,
  kAssign,
  kAssignT,
  kAssignG,
};

AssignType ParseAssignmentType(Parser* p) {
  char* it = p->data;
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

bool SkipUint(Parser* p) {
  char* it = p->data;
  if (it[0] == '0') {
    if (it[1] == 'x') {
    } else if (it[1] == 'b') {
      it = it + 2;
      if (!IsBinaryDigit(it[0])) {
        return false;
      }
    } else {
      p->data = it + 1;
    }
  } else if (IsDigit1(it[0])) {
    ++it;
    while (IsDigit(it[0])) {
      ++it;
    }
    p->data = it;
  }
  return true;
}

AstNode* ParseNumber(Parser* p) {
  Parser p_speculative{p->data};
  if (!IsDigit(p_speculative.data[0]) && p_speculative.data[0] != '-') {
    // TODO(btolsch): hexfloat, fraction, exponent.
    return nullptr;
  }
  if (p_speculative.data[0] == '-') {
    ++p_speculative.data;
  }
  if (!SkipUint(&p_speculative)) {
    return nullptr;
  }
  AstNode* node = AddNode(p, AstNode::Type::kNumber,
                          std::string(p->data, p_speculative.data - p->data));
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
  return (c == '-' || IsDigit(c) ||            // FIRST(number)
          c == '"' ||                          // FIRST(text)
          c == '\'' || c == 'h' || c == 'b');  // FIRST(bytes)
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
  AstNode* node = AddNode(p, AstNode::Type::kOccur, std::string(p->data, 1));
  ++p->data;
  return node;
}

AstNode* ParseMemberKey1(Parser* p) {
  Parser p_speculative{p->data};
  if (!ParseType1(&p_speculative)) {
    return nullptr;
  }
  if (!SkipWhitespace(&p_speculative)) {
    return nullptr;
  }
  if (*p_speculative.data++ != '=' || *p_speculative.data++ != '>') {
    return nullptr;
  }
  AstNode* node = AddNode(p, AstNode::Type::kMemberKey,
                          std::string(p->data, p_speculative.data - p->data));
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
  if (!SkipWhitespace(&p_speculative)) {
    return nullptr;
  }
  if (*p_speculative.data++ != ':') {
    return nullptr;
  }
  AstNode* node =
      AddNode(p, AstNode::Type::kMemberKey,
              std::string(p->data, p_speculative.data - p->data), id);
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
  if (!SkipWhitespace(&p_speculative)) {
    return nullptr;
  }
  if (*p_speculative.data++ != ':') {
    return nullptr;
  }
  AstNode* node =
      AddNode(p, AstNode::Type::kMemberKey,
              std::string(p->data, p_speculative.data - p->data), value);
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
  if (!SkipWhitespace(p)) {
    return false;
  }
  if (p->data[0] == ',') {
    ++p->data;
    if (!SkipWhitespace(p)) {
      return false;
    }
  }
  return true;
}

AstNode* ParseGroupChoice(Parser* p) {
  Parser p_speculative{p->data};
  AstNode* tail = nullptr;
  AstNode* group_node =
      AddNode(&p_speculative, AstNode::Type::kGrpchoice, std::string());
  char* group_node_text = p_speculative.data;
  while (true) {
    char* orig = p_speculative.data;
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
  char* orig = p->data;
  AstNode* group_choice = ParseGroupChoice(p);
  if (!group_choice) {
    return nullptr;
  }
  return AddNode(p, AstNode::Type::kGroup, std::string(orig, p->data - orig),
                 group_choice);
}

AstNode* ParseType2(Parser* p) {
  char* orig = p->data;
  char* it = p->data;
  AstNode* node = AddNode(p, AstNode::Type::kType2, std::string());
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
    if (!SkipWhitespace(p)) {
      return nullptr;
    }
    AstNode* type = ParseType(p);
    if (!type) {
      return nullptr;
    }
    if (!SkipWhitespace(p)) {
      return nullptr;
    }
    if (p->data[0] != ')') {
      return nullptr;
    }
    ++p->data;
    node->children = type;
  } else if (it[0] == '{') {
    p->data = it + 1;
    if (!SkipWhitespace(p)) {
      return nullptr;
    }
    AstNode* group = ParseGroup(p);
    if (!group) {
      return nullptr;
    }
    if (!SkipWhitespace(p)) {
      return nullptr;
    }
    if (p->data[0] != '}') {
      return nullptr;
    }
    ++p->data;
    node->children = group;
  } else if (it[0] == '[') {
    p->data = it + 1;
    if (!SkipWhitespace(p)) {
      return nullptr;
    }
    AstNode* group = ParseGroup(p);
    if (!group) {
      return nullptr;
    }
    if (!SkipWhitespace(p)) {
      return nullptr;
    }
    if (p->data[0] != ']') {
      return nullptr;
    }
    ++p->data;
    node->children = group;
  } else if (it[0] == '~') {
    p->data = it + 1;
    if (!SkipWhitespace(p)) {
      return nullptr;
    }
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
    if (!SkipWhitespace(p)) {
      return nullptr;
    }
    if (p->data[0] == '(') {
      ++p->data;
      if (!SkipWhitespace(p)) {
        return nullptr;
      }
      AstNode* group = ParseGroup(p);
      if (!group) {
        return nullptr;
      }
      if (!SkipWhitespace(p)) {
        return nullptr;
      }
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
        if (!SkipUint(p)) {
          return nullptr;
        }
        it = p->data;
      }
      if (it[0] != '(') {
        return nullptr;
      }
      p->data = ++it;
      if (!SkipWhitespace(p)) {
        return nullptr;
      }
      AstNode* type = ParseType(p);
      if (!type) {
        return nullptr;
      }
      if (!SkipWhitespace(p)) {
        return nullptr;
      }
      if (p->data[0] != ')') {
        return nullptr;
      }
      ++p->data;
      node->children = type;
    } else if (IsDigit(it[0])) {
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
  char* orig = p->data;
  AstNode* type2 = ParseType2(p);
  if (!type2) {
    return nullptr;
  }
  // TODO(btolsch): rangeop + ctlop
  // if (!HandleSpace(p)) {
  //   return false;
  // }
  return AddNode(p, AstNode::Type::kType1, std::string(orig, p->data - orig),
                 type2);
}

AstNode* ParseType(Parser* p) {
  Parser p_speculative{p->data};
  AstNode* type1 = ParseType1(&p_speculative);
  if (!type1) {
    return nullptr;
  }
  if (!SkipWhitespace(&p_speculative)) {
    return nullptr;
  }
  AstNode* tail = type1;
  while (*p_speculative.data == '/') {
    ++p_speculative.data;
    if (!SkipWhitespace(&p_speculative)) {
      return nullptr;
    }
    AstNode* next_type1 = ParseType1(&p_speculative);
    if (!next_type1) {
      return nullptr;
    }
    tail->sibling = next_type1;
    tail = next_type1;
    if (!SkipWhitespace(&p_speculative)) {
      return nullptr;
    }
  }
  AstNode* node =
      AddNode(p, AstNode::Type::kType,
              std::string(p->data, p_speculative.data - p->data), type1);
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

AstNode* ParseId(Parser* p) {
  char* id = p->data;
  if (!IsExtendedAlpha(id[0])) {
    return nullptr;
  }
  char* it = id + 1;
  while (true) {
    if (it[0] == '-' || it[0] == '.') {
      ++it;
      if (!IsExtendedAlpha(it[0]) && !IsDigit(it[0])) {
        return nullptr;
      }
      ++it;
    } else if (IsExtendedAlpha(it[0]) || IsDigit(it[0])) {
      ++it;
    } else {
      break;
    }
  }
  AstNode* node =
      AddNode(p, AstNode::Type::kId, std::string(p->data, it - p->data));
  p->data = it;
  return node;
}

AstNode* ParseGroupEntry1(Parser* p) {
  Parser p_speculative{p->data};
  AstNode* occur = ParseOccur(&p_speculative);
  if (occur) {
    if (!SkipWhitespace(&p_speculative)) {
      return nullptr;
    }
  }
  AstNode* member_key = ParseMemberKey(&p_speculative);
  if (member_key) {
    if (!SkipWhitespace(&p_speculative)) {
      return nullptr;
    }
  }
  AstNode* type = ParseType(&p_speculative);
  if (!type) {
    return nullptr;
  }
  AstNode* node = AddNode(p, AstNode::Type::kGrpent, std::string());
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
    if (!SkipWhitespace(&p_speculative)) {
      return nullptr;
    }
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
  AstNode* node = AddNode(p, AstNode::Type::kGrpent, std::string());
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
    if (!SkipWhitespace(&p_speculative)) {
      return nullptr;
    }
  }
  if (*p_speculative.data != '(') {
    return nullptr;
  }
  ++p_speculative.data;
  if (!SkipWhitespace(&p_speculative)) {
    return nullptr;
  }
  AstNode* group = ParseGroup(&p_speculative);
  if (!group) {
    return nullptr;
  }
  if (!SkipWhitespace(&p_speculative)) {
    return nullptr;
  }
  if (*p_speculative.data != ')') {
    return nullptr;
  }
  ++p_speculative.data;
  AstNode* node = AddNode(p, AstNode::Type::kGrpent, std::string());
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
  char* start = p->data;
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
  if (!SkipWhitespace(p)) {
    return nullptr;
  }
  char* assign_start = p->data;
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
      std::string(assign_start, p->data - assign_start));
  id->sibling = assign_node;

  if (!SkipWhitespace(p)) {
    return nullptr;
  }
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
  if (!SkipWhitespace(p)) {
    return nullptr;
  }
  return AddNode(p, AstNode::Type::kRule, std::string(start, p->data - start),
                 id);
}

ParseResult ParseCddl(std::string& data) {
  if (data[0] == 0) {
    return {nullptr, {}};
  }
  Parser p{(char*)data.data()};
  if (!SkipWhitespace(&p)) {
    return {nullptr, {}};
  }
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
    if (!SkipWhitespace(&p)) {
      return {nullptr, {}};
    }
  } while (p.data[0]);
  return {root, std::move(p.nodes)};
}

void PrintCollapsed(int size, const char* text) {
  for (int i = 0; i < size; ++i, ++text) {
    if (*text == ' ' || *text == '\n') {
      printf(" ");
      while (i < size && (*text == ' ' || *text == '\n')) {
        ++i;
        ++text;
      }
    }
    if (i < size) {
      printf("%c", *text);
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
