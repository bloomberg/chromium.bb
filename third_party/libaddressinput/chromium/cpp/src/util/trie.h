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

#ifndef I18N_ADDRESSINPUT_UTIL_TRIE_H_
#define I18N_ADDRESSINPUT_UTIL_TRIE_H_

#include <libaddressinput/util/basictypes.h>

#include <list>
#include <map>
#include <set>
#include <string>

namespace i18n {
namespace addressinput {

// A prefix search tree. Can return all objects whose keys start with a prefix
// string.
//
// Maps keys to multiple objects. This property is useful when mapping region
// names to region objects. For example, there's a "St. Petersburg" in Florida,
// and there's a "St. Petersburg" in Russia. A lookup for "St. Petersburg" key
// should return two distinct objects.
template <typename T>
class Trie {
 public:
  Trie();

  ~Trie();

  // Adds a mapping from |key| to |data_item|. Can be called with the same |key|
  // multiple times.
  void AddDataForKey(const std::string& key, const T& data_item);

  // Adds all objects whose keys start with |key_prefix| to the |results|
  // parameter. The |results| parameter should not be NULL.
  void FindDataForKeyPrefix(const std::string& key_prefix,
                            std::set<T>* results) const;

 private:
  // All objects for this node in the trie. This field is a collection to enable
  // mapping the same key to multiple objects.
  std::list<T> data_list_;

  // Trie sub nodes. The root trie stores the objects for the empty string key.
  // The children of the root trie store the objects for the one-letter keys.
  // The grand-children of the root trie store the objects for the two-letter
  // keys, and so on.
  std::map<char, Trie<T>*> sub_nodes_;

  DISALLOW_COPY_AND_ASSIGN(Trie);
};

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_UTIL_TRIE_H_
