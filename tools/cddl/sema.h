// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CDDL_SEMA_H_
#define TOOLS_CDDL_SEMA_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "tools/cddl/parse.h"

struct CddlGroup;

struct CddlType {
  enum class Which {
    kDirectChoice,
    kValue,
    kId,
    kMap,
    kArray,
    kGroupChoice,
    kGroupnameChoice,
    kTaggedType,
  };
  struct TaggedType {
    uint64_t tag_value;
    CddlType* type;
  };
  CddlType();
  ~CddlType();

  Which which;
  union {
    // A direct choice comes from the CDDL syntax "a-type / b-type / c-type".
    // This is in contrast to a "group choice".
    std::vector<CddlType*> direct_choice;

    // A literal value (number, text, or bytes) stored as its original string
    // form.
    std::string value;

    std::string id;
    CddlGroup* map;
    CddlGroup* array;

    // A group choice comes from the CDDL syntax:
    // a-group = (
    //   key1: uint, key2: text //
    //   key3: float, key4: bytes //
    //   key5: text
    // )
    CddlGroup* group_choice;
    TaggedType tagged_type;
  };
};

// TODO(btolsch): group choices
struct CddlGroup {
  struct Entry {
    enum class Which {
      kUninitialized = 0,
      kType,
      kGroup,
    };
    struct EntryType {
      std::string opt_key;
      CddlType* value;
    };
    Entry();
    ~Entry();

    std::string opt_occurrence;
    Which which = Which::kUninitialized;
    union {
      EntryType type;
      CddlGroup* group;
    };
  };

  std::vector<std::unique_ptr<Entry>> entries;
};

struct CddlSymbolTable {
  std::vector<std::unique_ptr<CddlType>> types;
  std::vector<std::unique_ptr<CddlGroup>> groups;
  std::string root_rule;
  std::map<std::string, CddlType*> type_map;
  std::map<std::string, CddlGroup*> group_map;
};

struct CppType {
  enum class Which {
    kUninitialized = 0,
    kUint64,
    kString,
    kBytes,
    kVector,
    kEnum,
    kStruct,
    kOptional,
    kDiscriminatedUnion,
    kTaggedType,
  };

  struct Vector {
    CppType* element_type;
  };

  struct Enum {
    std::string name;
    std::vector<CppType*> sub_members;
    std::vector<std::pair<std::string, uint64_t>> members;
  };

  struct Struct {
    enum class KeyType {
      kMap,
      kArray,
      kPlainGroup,
    };
    std::vector<std::pair<std::string, CppType*>> members;
    KeyType key_type;
  };

  struct DiscriminatedUnion {
    std::vector<CppType*> members;
  };

  struct TaggedType {
    uint64_t tag;
    CppType* real_type;
  };

  CppType();
  ~CppType();

  void InitVector();
  void InitEnum();
  void InitStruct();
  void InitDiscriminatedUnion();

  Which which = Which::kUninitialized;
  std::string name;
  union {
    Vector vector_type;
    Enum enum_type;
    Struct struct_type;
    CppType* optional_type;
    DiscriminatedUnion discriminated_union;
    TaggedType tagged_type;
  };
};

struct CppSymbolTable {
  std::vector<std::unique_ptr<CppType>> cpp_types;
  std::map<std::string, CppType*> cpp_type_map;
  std::string root_rule;
};

std::pair<bool, CddlSymbolTable> BuildSymbolTable(const AstNode& rules);
std::pair<bool, CppSymbolTable> BuildCppTypes(
    const CddlSymbolTable& cddl_table);

#endif  // TOOLS_CDDL_SEMA_H_
