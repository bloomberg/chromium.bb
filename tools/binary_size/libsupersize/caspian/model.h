// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_MODEL_H_
#define TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_MODEL_H_

#include <stdlib.h>

#include <deque>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "third_party/jsoncpp/source/include/json/json.h"

// Copied from representation in tools/binary_size/libsupersize/models.py

namespace caspian {

enum class ContainerType : char {
  kSymbol = '\0',
  kDirectory = 'D',
  kComponent = 'C',
  kFile = 'F',
  kJavaClass = 'J',
};

enum class SectionId : char {
  // kNone is unused except for default-initializing in containers
  kNone = '\0',
  kBss = 'b',
  kData = 'd',
  kDataRelRo = 'R',
  kDex = 'x',
  kDexMethod = 'm',
  kOther = 'o',
  kRoData = 'r',
  kText = 't',
  kPakNontranslated = 'P',
  kPakTranslations = 'p',
};

struct BaseSizeInfo;

struct Symbol {
  Symbol();
  Symbol(const Symbol& other);
  Symbol& operator=(const Symbol& other);
  static Symbol DiffSymbolFrom(const Symbol* before, const Symbol* after);

  bool IsOverhead() const { return full_name.substr(0, 10) == "Overhead: "; }

  bool IsBss() const { return sectionId == SectionId::kBss; }

  bool IsDex() const {
    return sectionId == SectionId::kDex || sectionId == SectionId::kDexMethod;
  }

  bool IsOther() const { return sectionId == SectionId::kOther; }

  bool IsPak() const {
    return sectionId == SectionId::kPakNontranslated ||
           sectionId == SectionId::kPakTranslations;
  }

  bool IsNative() const {
    return (sectionId == SectionId::kBss || sectionId == SectionId::kData ||
            sectionId == SectionId::kDataRelRo ||
            sectionId == SectionId::kText || sectionId == SectionId::kRoData);
  }

  bool IsStringLiteral() const { return full_name.substr(0, 1) == "\""; }

  int32_t SizeWithoutPadding() const { return size - padding; }

  int32_t EndAddress() const { return address + SizeWithoutPadding(); }

  // Derived from |full_name|. Generated lazily and cached.
  void DeriveNames() const;
  std::string_view TemplateName() const;
  std::string_view Name() const;

  int32_t address = 0;
  int32_t size = 0;
  int32_t flags = 0;
  int32_t padding = 0;
  float pss = 0.0f;
  SectionId sectionId = SectionId::kNone;
  std::string_view full_name;
  // Pointers into SizeInfo->raw_decompressed;
  const char* section_name = nullptr;
  const char* object_path = nullptr;
  const char* source_path = nullptr;
  const char* component = nullptr;
  std::vector<Symbol*>* aliases = nullptr;

  // The SizeInfo the symbol was constructed from. Primarily used for
  // allocating commonly-reused strings in a context where they won't outlive
  // the symbol.
  BaseSizeInfo* size_info = nullptr;

 private:
  mutable std::string_view template_name;
  mutable std::string_view name;
};

std::ostream& operator<<(std::ostream& os, const Symbol& sym);

struct BaseSizeInfo {
  BaseSizeInfo();
  ~BaseSizeInfo();
  std::vector<caspian::Symbol> raw_symbols;
  Json::Value metadata;
  std::deque<std::string> owned_strings;
};

struct SizeInfo : BaseSizeInfo {
  SizeInfo();
  ~SizeInfo();
  SizeInfo(const SizeInfo& other) = delete;
  SizeInfo& operator=(const SizeInfo& other) = delete;
  SectionId ShortSectionName(const char* section_name);

  // Entries in |raw_symbols| hold pointers to this data.
  std::vector<const char*> object_paths;
  std::vector<const char*> source_paths;
  std::vector<const char*> components;
  std::vector<const char*> section_names;
  std::vector<char> raw_decompressed;

  // A container for each symbol group.
  std::deque<std::vector<Symbol*>> alias_groups;
};

struct DiffSizeInfo : BaseSizeInfo {
  DiffSizeInfo(SizeInfo* before, SizeInfo* after);
  ~DiffSizeInfo();
  DiffSizeInfo(const DiffSizeInfo&) = delete;
  DiffSizeInfo& operator=(const DiffSizeInfo&) = delete;

  SizeInfo* before = nullptr;
  SizeInfo* after = nullptr;
};

struct Stat {
  int32_t count = 0;
  float size = 0.0f;

  void operator+=(const Stat& other) {
    count += other.count;
    size += other.size;
  }
};

struct NodeStats {
  NodeStats();
  ~NodeStats();
  NodeStats(SectionId section, int32_t count, float size);
  void WriteIntoJson(Json::Value* out) const;
  NodeStats& operator+=(const NodeStats& other);
  SectionId ComputeBiggestSection() const;

  std::map<SectionId, Stat> child_stats;
};

struct TreeNode {
  TreeNode();
  ~TreeNode();
  void WriteIntoJson(Json::Value* out, int depth);

  std::string_view id_path;
  const char* src_path = nullptr;
  const char* component = nullptr;
  float size = 0.0f;
  NodeStats node_stats;
  int32_t flags = 0;
  int32_t short_name_index = 0;

  ContainerType container_type = ContainerType::kSymbol;

  std::vector<TreeNode*> children;
  TreeNode* parent = nullptr;
  const Symbol* symbol = nullptr;
};

}  // namespace caspian
#endif  // TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_MODEL_H_
