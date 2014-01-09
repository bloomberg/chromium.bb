// Copyright (C) 2013 Google Inc.
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

#include "retriever.h"

#include <libaddressinput/callback.h>
#include <libaddressinput/downloader.h>
#include <libaddressinput/storage.h>
#include <libaddressinput/util/basictypes.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <cassert>
#include <cstddef>
#include <map>
#include <string>
#include <utility>

#include "lookup_key_util.h"
#include "util/stl_util.h"

namespace i18n {
namespace addressinput {

Retriever::Retriever(const std::string& validation_data_url,
                     scoped_ptr<const Downloader> downloader,
                     scoped_ptr<Storage> storage)
    : lookup_key_util_(validation_data_url),
      downloader_(downloader.Pass()),
      storage_(storage.Pass()) {
  assert(storage_ != NULL);
  assert(downloader_ != NULL);
}

Retriever::~Retriever() {
  STLDeleteValues(&requests_);
}

void Retriever::Retrieve(const std::string& key,
                         scoped_ptr<Callback> retrieved) {
  std::map<std::string, Callback*>::iterator request_it =
      requests_.find(key);
  if (request_it != requests_.end()) {
    // Abandon a previous request.
    delete request_it->second;
    requests_.erase(request_it);
  }

  requests_[key] = retrieved.release();
  storage_->Get(key,
                BuildCallback(this, &Retriever::OnDataRetrievedFromStorage));
}

void Retriever::OnDataRetrievedFromStorage(bool success,
                                           const std::string& key,
                                           const std::string& data) {
  // TODO(rouslan): Add validation for data integrity and freshness. If a
  // download fails, then it's OK to use stale data.
  if (success) {
    scoped_ptr<Callback> retrieved = GetCallbackForKey(key);
    if (retrieved != NULL) {
      (*retrieved)(success, key, data);
    }
  } else {
    downloader_->Download(lookup_key_util_.GetUrlForKey(key),
                          BuildCallback(this, &Retriever::OnDownloaded));
  }
}

void Retriever::OnDownloaded(bool success,
                             const std::string& url,
                             const std::string& data) {
  const std::string& key = lookup_key_util_.GetKeyForUrl(url);
  if (success) {
    storage_->Put(key, data);
  }
  scoped_ptr<Callback> retrieved = GetCallbackForKey(key);
  if (retrieved != NULL) {
    (*retrieved)(success, key, success ? data : std::string());
  }
}

scoped_ptr<Retriever::Callback> Retriever::GetCallbackForKey(
    const std::string& key) {
  std::map<std::string, Callback*>::iterator iter =
      requests_.find(key);
  if (iter == requests_.end()) {
    // An abandonened request.
    return scoped_ptr<Callback>();
  }
  scoped_ptr<Callback> callback(iter->second);
  requests_.erase(iter);
  return callback.Pass();
}

}  // namespace addressinput
}  // namespace i18n
