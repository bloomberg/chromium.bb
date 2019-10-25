// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Command-line interface for checking the integrity of .size files.
// Intended to be called from WebAssembly code.

#include <stdlib.h>

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

#include "third_party/jsoncpp/source/include/json/json.h"
#include "tools/binary_size/libsupersize/caspian/file_format.h"
#include "tools/binary_size/libsupersize/caspian/model.h"

namespace {
caspian::SizeInfo info;

caspian::TreeNode root;
// TODO: A full hash table might be overkill here - could walk tree to find
// node.
std::unordered_map<std::string_view, caspian::TreeNode*> parents;

std::unique_ptr<Json::StreamWriter> writer;

/** Name used by a directory created to hold symbols with no name. */
const char* _NO_NAME = "(No path)";

std::string ToJson(const Json::Value& value) {
  std::stringstream s;
  writer->write(value, &s);
  return s.str();
}

void OpenIntoJson(const caspian::TreeNode* node, Json::Value* out, int depth) {
  (*out)["idPath"] = std::string(node->id_path);
  (*out)["shortNameIndex"] = node->short_name_index;
  // TODO: Put correct information.
  (*out)["type"] = "D";
  (*out)["size"] = node->size;
  (*out)["flags"] = node->flags;
  (*out)["childStats"] = Json::Value(Json::objectValue);
  if (depth < 0) {
    (*out)["children"] = Json::Value();  // null
  } else {
    (*out)["children"] = Json::Value(Json::arrayValue);
    for (unsigned int i = 0; i < node->children.size(); i++) {
      OpenIntoJson(node->children[i], &(*out)["children"][i], depth - 1);
    }
  }
}

std::string_view DirName(std::string_view path, char sep) {
  auto end = path.find_last_of(sep);
  if (end != std::string_view::npos) {
    std::string_view ret = path;
    ret.remove_suffix(path.size() - end);
    return ret;
  }
  return "";
}

void AttachToParent(caspian::TreeNode* child, caspian::TreeNode* parent) {
  if (child->parent != nullptr) {
    std::cerr << "Child " << child->id_path << " already attached to parent "
              << child->parent->id_path << std::endl;
    std::cerr << "Cannot be attached to " << parent->id_path << std::endl;
    exit(1);
  }

  parent->children.push_back(child);
  child->parent = parent;

  // Update size information along tree.
  caspian::TreeNode* node = child;
  while (node->parent) {
    node->parent->size += child->size;
    node = node->parent;
  }
}

caspian::TreeNode* GetOrMakeParentNode(caspian::TreeNode* child_node) {
  std::string_view parent_path;
  if (child_node->id_path.empty()) {
    parent_path = _NO_NAME;
  } else {
    parent_path = DirName(child_node->id_path, '/');
  }

  caspian::TreeNode*& parent = parents[parent_path];
  if (parent == nullptr) {
    parent = new caspian::TreeNode();
    parent->id_path = parent_path;
    parent->short_name_index = parent_path.find_last_of('/') + 1;
    // TODO: Set parent type to directory or component.
    parents[parent_path] = parent;
  }
  if (child_node->parent != parent) {
    AttachToParent(child_node, parent);
  }
  return parent;
}

void AddFileEntry(const std::string_view source_path,
                  const std::vector<const caspian::Symbol*>& symbols) {
  // Creates a single file node with a child for each symbol in that file.
  caspian::TreeNode* file_node = new caspian::TreeNode();
  if (source_path.empty()) {
    file_node->id_path = _NO_NAME;
  } else {
    file_node->id_path = source_path;
  }
  file_node->short_name_index = source_path.find_last_of('/') + 1;
  // TODO: Initialize file type, source path, component

  // Create symbol nodes.
  for (const auto sym : symbols) {
    caspian::TreeNode* symbol_node = new caspian::TreeNode();
    symbol_node->id_path = sym->full_name;
    symbol_node->size = sym->size;
    AttachToParent(symbol_node, file_node);
  }

  // TODO: Only add if there are unfiltered symbols in this file.
  caspian::TreeNode* orphan_node = file_node;
  while (orphan_node != &root) {
    orphan_node = GetOrMakeParentNode(orphan_node);
  }
}

void AddSymbolsAndFileNodes(caspian::SizeInfo* size_info) {
  // Group symbols by source path.
  std::unordered_map<std::string_view, std::vector<const caspian::Symbol*>>
      symbols;
  for (auto& sym : size_info->raw_symbols) {
    std::string_view key = sym.source_path;
    if (key == nullptr) {
      key = sym.object_path;
    }
    symbols[key].push_back(&sym);
  }
  for (const auto& pair : symbols) {
    AddFileEntry(pair.first, pair.second);
  }
}
}  // namespace

extern "C" {
bool LoadSizeFile(const char* compressed, size_t size) {
  writer.reset(Json::StreamWriterBuilder().newStreamWriter());
  caspian::ParseSizeInfo(compressed, size, &info);
  // Build tree
  root.id_path = "/";
  parents[""] = &root;
  AddSymbolsAndFileNodes(&info);
  return true;
}

const char* Open(const char* path) {
  // Returns a string that can be parsed to a JS object.
  static std::string result;
  const auto node = parents.find(path);

  if (node != parents.end()) {
    Json::Value v;
    OpenIntoJson(node->second, &v, 1);
    result = ToJson(v);
    return result.c_str();
  } else {
    std::cerr << "Tried to open nonexistent node with path: " << path
              << std::endl;
    exit(1);
  }
}
}  // extern "C"
