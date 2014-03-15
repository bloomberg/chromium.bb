// Copyright (C) 2014 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "trie.h"

#include <cassert>
#include <cstddef>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <utility>

#include "stl_util.h"

namespace i18n {
namespace addressinput {

template <typename T>
Trie<T>::Trie() {}

template <typename T>
Trie<T>::~Trie() {
  STLDeleteValues(&sub_nodes_);
}

template <typename T>
void Trie<T>::AddDataForKey(const std::string& key, const T& data_item) {
  Trie<T>* current_node = this;
  for (size_t key_start = 0; key_start < key.length(); ++key_start) {
    typename std::map<char, Trie<T>*>::const_iterator sub_node_it =
        current_node->sub_nodes_.find(key[key_start]);
    if (sub_node_it == current_node->sub_nodes_.end()) {
      sub_node_it = current_node->sub_nodes_.insert(
          std::make_pair(key[key_start], new Trie<T>)).first;
    }
    current_node = sub_node_it->second;
    assert(current_node != NULL);
  }
  current_node->data_list_.push_back(data_item);
}

template <typename T>
void Trie<T>::FindDataForKeyPrefix(const std::string& key_prefix,
                                   std::set<T>* results) const {
  assert(results != NULL);

  // Find the sub-trie for the key prefix.
  const Trie<T>* current_node = this;
  for (size_t key_prefix_start = 0; key_prefix_start < key_prefix.length();
       ++key_prefix_start) {
    typename std::map<char, Trie<T>*>::const_iterator sub_node_it =
        current_node->sub_nodes_.find(key_prefix[key_prefix_start]);
    if (sub_node_it == current_node->sub_nodes_.end()) {
      return;
    }
    current_node = sub_node_it->second;
    assert(current_node != NULL);
  }

  // Collect data from all sub-tries.
  std::queue<const Trie<T>*> node_queue;
  node_queue.push(current_node);
  while (!node_queue.empty()) {
    const Trie<T>* queue_front = node_queue.front();
    node_queue.pop();

    results->insert(
        queue_front->data_list_.begin(), queue_front->data_list_.end());

    for (typename std::map<char, Trie<T>*>::const_iterator sub_node_it =
             queue_front->sub_nodes_.begin();
         sub_node_it != queue_front->sub_nodes_.end();
         ++sub_node_it) {
      node_queue.push(sub_node_it->second);
    }
  }
}

// Separating template definitions and declarations requires defining all
// possible template parameters to avoid linking errors.
class Ruleset;
template class Trie<const Ruleset*>;
template class Trie<std::string>;

}  // namespace addressinput
}  // namespace i18n
