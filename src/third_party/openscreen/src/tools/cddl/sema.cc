// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/cddl/sema.h"

#include <string.h>
#include <unistd.h>

#include <cinttypes>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "third_party/abseil/src/absl/strings/string_view.h"

CddlType::CddlType() : map(nullptr) {}
CddlType::~CddlType() {
  switch (which) {
    case CddlType::Which::kDirectChoice:
      direct_choice.std::vector<CddlType*>::~vector();
      break;
    case CddlType::Which::kValue:
      value.std::string::~basic_string();
      break;
    case CddlType::Which::kId:
      id.std::string::~basic_string();
      break;
    case CddlType::Which::kMap:
      break;
    case CddlType::Which::kArray:
      break;
    case CddlType::Which::kGroupChoice:
      break;
    case CddlType::Which::kGroupnameChoice:
      break;
    case CddlType::Which::kTaggedType:
      tagged_type.~TaggedType();
      break;
  }
}

CddlGroup::Entry::Entry() : group(nullptr) {}
CddlGroup::Entry::~Entry() {
  switch (which) {
    case CddlGroup::Entry::Which::kUninitialized:
      break;
    case CddlGroup::Entry::Which::kType:
      type.~EntryType();
      break;
    case CddlGroup::Entry::Which::kGroup:
      break;
  }
}

CppType::CppType() : vector_type() {}
CppType::~CppType() {
  switch (which) {
    case CppType::Which::kUninitialized:
      break;
    case CppType::Which::kUint64:
      break;
    case CppType::Which::kString:
      break;
    case CppType::Which::kBytes:
      break;
    case CppType::Which::kVector:
      break;
    case CppType::Which::kEnum:
      enum_type.~Enum();
      break;
    case CppType::Which::kStruct:
      struct_type.~Struct();
      break;
    case CppType::Which::kOptional:
      break;
    case CppType::Which::kDiscriminatedUnion:
      discriminated_union.~DiscriminatedUnion();
      break;
    case CppType::Which::kTaggedType:
      break;
  }
}

void CppType::InitVector() {
  which = Which::kVector;
}

void CppType::InitEnum() {
  which = Which::kEnum;
  new (&enum_type) Enum();
}

void CppType::InitStruct() {
  which = Which::kStruct;
  new (&struct_type) Struct();
}

void CppType::InitDiscriminatedUnion() {
  which = Which::kDiscriminatedUnion;
  new (&discriminated_union) DiscriminatedUnion();
}

void InitString(std::string* s, absl::string_view value) {
  new (s) std::string(value);
}

void InitDirectChoice(std::vector<CddlType*>* direct_choice) {
  new (direct_choice) std::vector<CddlType*>();
}

void InitGroupEntry(CddlGroup::Entry::EntryType* entry) {
  new (entry) CddlGroup::Entry::EntryType();
}

CddlType* AddCddlType(CddlSymbolTable* table, CddlType::Which which) {
  table->types.emplace_back(new CddlType);
  CddlType* value = table->types.back().get();
  value->which = which;
  return value;
}

CddlType* AnalyzeType(CddlSymbolTable* table, const AstNode& type);
CddlGroup* AnalyzeGroup(CddlSymbolTable* table, const AstNode& group);

CddlType* AnalyzeType2(CddlSymbolTable* table, const AstNode& type2) {
  const AstNode* node = type2.children;
  if (node->type == AstNode::Type::kNumber ||
      node->type == AstNode::Type::kText ||
      node->type == AstNode::Type::kBytes) {
    CddlType* value = AddCddlType(table, CddlType::Which::kValue);
    InitString(&value->value, node->text);
    return value;
  } else if (node->type == AstNode::Type::kTypename) {
    if (type2.text[0] == '~') {
      dprintf(STDERR_FILENO, "We don't support the '~' operator.\n");
      return nullptr;
    }
    CddlType* id = AddCddlType(table, CddlType::Which::kId);
    InitString(&id->id, node->text);
    return id;
  } else if (node->type == AstNode::Type::kType) {
    if (type2.text[0] == '#' && type2.text[1] == '6' && type2.text[2] == '.') {
      CddlType* tagged_type = AddCddlType(table, CddlType::Which::kTaggedType);
      tagged_type->tagged_type.tag_value =
          atoll(type2.text.substr(3 /* #6. */).data());
      tagged_type->tagged_type.type = AnalyzeType(table, *node);
      return tagged_type;
    }
    dprintf(STDERR_FILENO, "Unknown type2 value, expected #6.[uint]\n");
  } else if (node->type == AstNode::Type::kGroup) {
    if (type2.text[0] == '{') {
      CddlType* map = AddCddlType(table, CddlType::Which::kMap);
      map->map = AnalyzeGroup(table, *node);
      return map;
    } else if (type2.text[0] == '[') {
      CddlType* array = AddCddlType(table, CddlType::Which::kArray);
      array->array = AnalyzeGroup(table, *node);
      return array;
    } else if (type2.text[0] == '&') {
      CddlType* group_choice =
          AddCddlType(table, CddlType::Which::kGroupChoice);
      group_choice->group_choice = AnalyzeGroup(table, *node);
      return group_choice;
    }
  } else if (node->type == AstNode::Type::kGroupname) {
    if (type2.text[0] == '&') {
      CddlType* group_choice =
          AddCddlType(table, CddlType::Which::kGroupnameChoice);
      InitString(&group_choice->id, node->text);
      return group_choice;
    }
  }
  return nullptr;
}

CddlType* AnalyzeType1(CddlSymbolTable* table, const AstNode& type1) {
  return AnalyzeType2(table, *type1.children);
}

CddlType* AnalyzeType(CddlSymbolTable* table, const AstNode& type) {
  const AstNode* type1 = type.children;
  if (type1->sibling) {
    CddlType* type_choice = AddCddlType(table, CddlType::Which::kDirectChoice);
    InitDirectChoice(&type_choice->direct_choice);
    while (type1) {
      type_choice->direct_choice.push_back(AnalyzeType1(table, *type1));
      type1 = type1->sibling;
    }
    return type_choice;
  } else {
    return AnalyzeType1(table, *type1);
  }
}

bool AnalyzeGroupEntry(CddlSymbolTable* table,
                       const AstNode& group_entry,
                       CddlGroup::Entry* entry);

CddlGroup* AnalyzeGroup(CddlSymbolTable* table, const AstNode& group) {
  // NOTE: |group.children| is a grpchoice, which we don't currently handle.
  // Therefore, we assume it has no siblings and move on to immediately handling
  // its grpent children.
  const AstNode* node = group.children->children;
  table->groups.emplace_back(new CddlGroup);
  CddlGroup* group_def = table->groups.back().get();
  while (node) {
    group_def->entries.emplace_back(new CddlGroup::Entry);
    AnalyzeGroupEntry(table, *node, group_def->entries.back().get());
    node = node->sibling;
  }
  return group_def;
}

bool AnalyzeGroupEntry(CddlSymbolTable* table,
                       const AstNode& group_entry,
                       CddlGroup::Entry* entry) {
  const AstNode* node = group_entry.children;
  if (node->type == AstNode::Type::kOccur) {
    entry->opt_occurrence = std::string(node->text);
    node = node->sibling;
  }
  if (node->type == AstNode::Type::kMemberKey) {
    if (node->text[node->text.size() - 1] == '>')
      return false;
    entry->which = CddlGroup::Entry::Which::kType;
    InitGroupEntry(&entry->type);
    entry->type.opt_key = std::string(node->children->text);
    node = node->sibling;
  }
  if (node->type == AstNode::Type::kType) {
    if (entry->which == CddlGroup::Entry::Which::kUninitialized) {
      entry->which = CddlGroup::Entry::Which::kType;
      InitGroupEntry(&entry->type);
    }
    entry->type.value = AnalyzeType(table, *node);
  } else if (node->type == AstNode::Type::kGroupname) {
    return false;
  } else if (node->type == AstNode::Type::kGroup) {
    entry->which = CddlGroup::Entry::Which::kGroup;
    entry->group = AnalyzeGroup(table, *node);
  }
  return true;
}

void DumpType(CddlType* type, int indent_level = 0);
void DumpGroup(CddlGroup* group, int indent_level = 0);

void DumpType(CddlType* type, int indent_level) {
  for (int i = 0; i <= indent_level; ++i)
    printf("--");
  switch (type->which) {
    case CddlType::Which::kDirectChoice:
      printf("kDirectChoice:\n");
      for (auto& option : type->direct_choice)
        DumpType(option, indent_level + 1);
      break;
    case CddlType::Which::kValue:
      printf("kValue: %s\n", type->value.c_str());
      break;
    case CddlType::Which::kId:
      printf("kId: %s\n", type->id.c_str());
      break;
    case CddlType::Which::kMap:
      printf("kMap:\n");
      DumpGroup(type->map, indent_level + 1);
      break;
    case CddlType::Which::kArray:
      printf("kArray:\n");
      DumpGroup(type->array, indent_level + 1);
      break;
    case CddlType::Which::kGroupChoice:
      printf("kGroupChoice:\n");
      DumpGroup(type->group_choice, indent_level + 1);
      break;
    case CddlType::Which::kGroupnameChoice:
      printf("kGroupnameChoice:\n");
      break;
    case CddlType::Which::kTaggedType:
      printf("kTaggedType: %" PRIu64 "\n", type->tagged_type.tag_value);
      DumpType(type->tagged_type.type, indent_level + 1);
      break;
  }
}

void DumpGroup(CddlGroup* group, int indent_level) {
  for (auto& entry : group->entries) {
    for (int i = 0; i <= indent_level; ++i)
      printf("--");
    switch (entry->which) {
      case CddlGroup::Entry::Which::kUninitialized:
        break;
      case CddlGroup::Entry::Which::kType:
        printf("kType:");
        if (!entry->opt_occurrence.empty())
          printf(" %s", entry->opt_occurrence.c_str());
        if (!entry->type.opt_key.empty())
          printf(" %s =>", entry->type.opt_key.c_str());
        printf("\n");
        DumpType(entry->type.value, indent_level + 1);
        break;
      case CddlGroup::Entry::Which::kGroup:
        printf("kGroup: %s\n", entry->opt_occurrence.c_str());
        DumpGroup(entry->group, indent_level + 1);
        break;
    }
  }
}

void DumpSymbolTable(CddlSymbolTable* table) {
  for (auto& entry : table->type_map) {
    printf("%s\n", entry.first.c_str());
    DumpType(entry.second);
    printf("\n");
  }
  for (auto& entry : table->group_map) {
    printf("%s\n", entry.first.c_str());
    DumpGroup(entry.second);
    printf("\n");
  }
}

std::pair<bool, CddlSymbolTable> BuildSymbolTable(const AstNode& rules) {
  std::pair<bool, CddlSymbolTable> result;
  result.first = false;
  auto& table = result.second;
  const AstNode* node = rules.children;
  if (node->type != AstNode::Type::kTypename &&
      node->type != AstNode::Type::kGroupname) {
    return result;
  }
  bool is_type = node->type == AstNode::Type::kTypename;
  table.root_rule = std::string(node->text);

  node = node->sibling;
  if (node->type != AstNode::Type::kAssign)
    return result;

  node = node->sibling;
  if (is_type) {
    CddlType* type = AnalyzeType(&table, *node);
    if (!type)
      return result;
    table.type_map.emplace(table.root_rule, type);
  } else {
    table.groups.emplace_back(new CddlGroup);
    CddlGroup* group = table.groups.back().get();
    group->entries.emplace_back(new CddlGroup::Entry);
    AnalyzeGroupEntry(&table, *node, group->entries.back().get());
    table.group_map.emplace(table.root_rule, group);
  }

  const AstNode* rule = rules.sibling;
  while (rule) {
    node = rule->children;
    if (node->type != AstNode::Type::kTypename &&
        node->type != AstNode::Type::kGroupname) {
      return result;
    }
    bool is_type = node->type == AstNode::Type::kTypename;
    absl::string_view name = node->text;

    node = node->sibling;
    if (node->type != AstNode::Type::kAssign)
      return result;

    node = node->sibling;
    if (is_type) {
      CddlType* type = AnalyzeType(&table, *node);
      if (!type)
        return result;
      table.type_map.emplace(std::string(name), type);
    } else {
      table.groups.emplace_back(new CddlGroup);
      CddlGroup* group = table.groups.back().get();
      group->entries.emplace_back(new CddlGroup::Entry);
      AnalyzeGroupEntry(&table, *node, group->entries.back().get());
      table.group_map.emplace(std::string(name), group);
    }
    rule = rule->sibling;
  }

  result.first = true;
  return result;
}

CppType* GetCppType(CppSymbolTable* table, const std::string& name) {
  if (name.empty()) {
    table->cpp_types.emplace_back(new CppType);
    return table->cpp_types.back().get();
  }
  auto entry = table->cpp_type_map.find(name);
  if (entry != table->cpp_type_map.end())
    return entry->second;
  table->cpp_types.emplace_back(new CppType);
  table->cpp_type_map.emplace(name, table->cpp_types.back().get());
  return table->cpp_types.back().get();
}

bool IncludeGroupMembersInEnum(CppSymbolTable* table,
                               const CddlSymbolTable& cddl_table,
                               CppType* cpp_type,
                               const CddlGroup& group) {
  for (const auto& x : group.entries) {
    if (!x->opt_occurrence.empty() ||
        x->which != CddlGroup::Entry::Which::kType) {
      return false;
    }
    if (x->type.value->which == CddlType::Which::kValue &&
        !x->type.opt_key.empty()) {
      cpp_type->enum_type.members.emplace_back(
          x->type.opt_key, atoi(x->type.value->value.c_str()));
    } else if (x->type.value->which == CddlType::Which::kId) {
      auto group_entry = cddl_table.group_map.find(x->type.value->id);
      if (group_entry == cddl_table.group_map.end())
        return false;
      if (group_entry->second->entries.size() != 1 ||
          group_entry->second->entries[0]->which !=
              CddlGroup::Entry::Which::kGroup) {
        return false;
      }
      CppType* sub_enum = GetCppType(table, x->type.value->id);
      if (sub_enum->which == CppType::Which::kUninitialized) {
        sub_enum->InitEnum();
        sub_enum->name = x->type.value->id;
        if (!IncludeGroupMembersInEnum(
                table, cddl_table, sub_enum,
                *group_entry->second->entries[0]->group)) {
          return false;
        }
      }
      cpp_type->enum_type.sub_members.push_back(sub_enum);
    } else {
      return false;
    }
  }
  return true;
}

CppType* MakeCppType(CppSymbolTable* table,
                     const CddlSymbolTable& cddl_table,
                     const std::string& name,
                     const CddlType& type);

bool AddMembersToStruct(
    CppSymbolTable* table,
    const CddlSymbolTable& cddl_table,
    CppType* cpp_type,
    const std::vector<std::unique_ptr<CddlGroup::Entry>>& entries) {
  for (const auto& x : entries) {
    if (x->which == CddlGroup::Entry::Which::kType) {
      if (x->type.opt_key.empty()) {
        if (x->type.value->which != CddlType::Which::kId ||
            !x->opt_occurrence.empty()) {
          return false;
        }
        auto group_entry = cddl_table.group_map.find(x->type.value->id);
        if (group_entry == cddl_table.group_map.end())
          return false;
        if (group_entry->second->entries.size() != 1 ||
            group_entry->second->entries[0]->which !=
                CddlGroup::Entry::Which::kGroup) {
          return false;
        }
        if (!AddMembersToStruct(
                table, cddl_table, cpp_type,
                group_entry->second->entries[0]->group->entries)) {
          return false;
        }
      } else {
        CppType* member_type =
            MakeCppType(table, cddl_table,
                        cpp_type->name + std::string("_") + x->type.opt_key,
                        *x->type.value);
        if (!member_type)
          return false;
        if (member_type->name.empty())
          member_type->name = x->type.opt_key;
        if (x->opt_occurrence == "?") {
          table->cpp_types.emplace_back(new CppType);
          CppType* optional_type = table->cpp_types.back().get();
          optional_type->which = CppType::Which::kOptional;
          optional_type->optional_type = member_type;
          cpp_type->struct_type.members.emplace_back(x->type.opt_key,
                                                     optional_type);
        } else {
          cpp_type->struct_type.members.emplace_back(x->type.opt_key,
                                                     member_type);
        }
      }
    } else {
      if (!AddMembersToStruct(table, cddl_table, cpp_type, x->group->entries))
        return false;
    }
  }
  return true;
}

CppType* MakeCppType(CppSymbolTable* table,
                     const CddlSymbolTable& cddl_table,
                     const std::string& name,
                     const CddlType& type) {
  CppType* cpp_type = nullptr;
  switch (type.which) {
    case CddlType::Which::kId: {
      if (type.id == "uint") {
        cpp_type = GetCppType(table, name);
        cpp_type->which = CppType::Which::kUint64;
      } else if (type.id == "text") {
        cpp_type = GetCppType(table, name);
        cpp_type->which = CppType::Which::kString;
      } else if (type.id == "bytes") {
        cpp_type = GetCppType(table, name);
        cpp_type->which = CppType::Which::kBytes;
      } else {
        cpp_type = GetCppType(table, type.id);
      }
    } break;
    case CddlType::Which::kMap: {
      cpp_type = GetCppType(table, name);
      cpp_type->InitStruct();
      cpp_type->struct_type.key_type = CppType::Struct::KeyType::kMap;
      cpp_type->name = name;
      if (!AddMembersToStruct(table, cddl_table, cpp_type, type.map->entries))
        return nullptr;
    } break;
    case CddlType::Which::kArray: {
      cpp_type = GetCppType(table, name);
      if (type.array->entries.size() == 1 ||
          type.array->entries[0]->opt_occurrence == "*") {
        cpp_type->InitVector();
        cpp_type->vector_type.element_type =
            GetCppType(table, type.array->entries[0]->type.value->id);
      } else {
        cpp_type->InitStruct();
        cpp_type->struct_type.key_type = CppType::Struct::KeyType::kArray;
        cpp_type->name = name;
        if (!AddMembersToStruct(table, cddl_table, cpp_type,
                                type.map->entries)) {
          return nullptr;
        }
      }
    } break;
    case CddlType::Which::kGroupChoice: {
      cpp_type = GetCppType(table, name);
      cpp_type->InitEnum();
      cpp_type->name = name;
      if (!IncludeGroupMembersInEnum(table, cddl_table, cpp_type,
                                     *type.group_choice)) {
        return nullptr;
      }
    } break;
    case CddlType::Which::kDirectChoice: {
      cpp_type = GetCppType(table, name);
      cpp_type->InitDiscriminatedUnion();
      for (const auto* cddl_choice : type.direct_choice) {
        CppType* member = MakeCppType(table, cddl_table, "", *cddl_choice);
        if (!member)
          return nullptr;
        cpp_type->discriminated_union.members.push_back(member);
      }
      return cpp_type;
    } break;
    case CddlType::Which::kTaggedType: {
      cpp_type = GetCppType(table, name);
      cpp_type->which = CppType::Which::kTaggedType;
      cpp_type->tagged_type.tag = type.tagged_type.tag_value;
      cpp_type->tagged_type.real_type =
          MakeCppType(table, cddl_table, "", *type.tagged_type.type);
    } break;
    default:
      return nullptr;
  }
  return cpp_type;
}

std::pair<bool, CppSymbolTable> BuildCppTypes(
    const CddlSymbolTable& cddl_table) {
  std::pair<bool, CppSymbolTable> result;
  result.first = false;
  auto& table = result.second;
  table.root_rule = cddl_table.root_rule;
  for (const auto& type_entry : cddl_table.type_map) {
    if (!MakeCppType(&table, cddl_table, type_entry.first,
                     *type_entry.second)) {
      return result;
    }
  }
  auto root_rule_entry = table.cpp_type_map.find(table.root_rule);
  if (root_rule_entry == table.cpp_type_map.end())
    return result;
  CppType* root_rule = root_rule_entry->second;
  if (root_rule->which != CppType::Which::kDiscriminatedUnion)
    return result;
  for (const auto* choice : root_rule->discriminated_union.members) {
    if (choice->which != CppType::Which::kTaggedType)
      return result;
    if (choice->tagged_type.real_type->name.empty())
      return result;
  }

  result.first = true;
  return result;
}
