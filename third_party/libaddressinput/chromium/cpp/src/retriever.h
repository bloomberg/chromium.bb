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
//
// An object to retrieve data.

#ifndef I18N_ADDRESSINPUT_RETRIEVER_H_
#define I18N_ADDRESSINPUT_RETRIEVER_H_

#include <libaddressinput/callback.h>
#include <libaddressinput/util/basictypes.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <map>
#include <string>

#include "lookup_key_util.h"

namespace i18n {
namespace addressinput {

class Downloader;
class Storage;

// Retrieves data. Sample usage:
//    Storage* storage = ...;
//    Downloader* downloader = ...;
//    Retriever retriever("https://i18napis.appspot.com/ssl-address/",
//                        downloader, storage);
//    retriever.Retrieve("data/CA/AB--fr",
//                       BuildCallback(this, &MyClass::OnDataRetrieved));
class Retriever {
 public:
  typedef i18n::addressinput::Callback<std::string, std::string> Callback;

  Retriever(const std::string& validation_data_url,
            scoped_ptr<const Downloader> downloader,
            scoped_ptr<Storage> storage);
  ~Retriever();

  // Retrieves the data for |key| and invokes the |retrieved| callback. Checks
  // for the data in storage first. If storage does not have the data for |key|,
  // then downloads the data and places it in storage.
  void Retrieve(const std::string& key, scoped_ptr<Callback> retrieved);

 private:
  // Callback for when a rule is retrieved from |storage_|.
  void OnDataRetrievedFromStorage(bool success,
                                  const std::string& key,
                                  const std::string& data);

  // Callback for when a rule is retrieved by |downloader_|.
  void OnDownloaded(bool success,
                    const std::string& url,
                    const std::string& data);

  // Looks up the callback for |key| in |requests_|, removes it from the map
  // and returns it. Assumes that |key| is in fact in |requests_|.
  scoped_ptr<Callback> GetCallbackForKey(const std::string& key);

  const LookupKeyUtil lookup_key_util_;
  scoped_ptr<const Downloader> downloader_;
  scoped_ptr<Storage> storage_;
  // Holds pending requests. The callback pointers are owned.
  std::map<std::string, Callback*> requests_;

  DISALLOW_COPY_AND_ASSIGN(Retriever);
};

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_RETRIEVER_H_
