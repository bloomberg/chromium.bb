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

namespace i18n {
namespace addressinput {

class Downloader;
class Storage;

// Manages downloading data and caching it locally. Sample usage:
//    scoped_ptr<Downloader> downloader(new Downloader);
//    scoped_ptr<Storage> storage(new Storage);
//    Retriever retriever("https://i18napis.appspot.com/ssl-aggregate-address/",
//                        downloader.Pass(), storage.Pass());
//    retriever.Retrieve("data/CA/AB--fr",
//                       BuildCallback(this, &MyClass::OnDataRetrieved));
class Retriever {
 public:
  typedef i18n::addressinput::Callback<std::string, std::string> Callback;

  Retriever(const std::string& validation_data_url,
            scoped_ptr<Downloader> downloader,
            scoped_ptr<Storage> storage);
  ~Retriever();

  // Retrieves the data for |key| and invokes the |retrieved| callback. Checks
  // for the data in storage first. If storage does not have the data for |key|,
  // then downloads the data and places it in storage.
  void Retrieve(const std::string& key, scoped_ptr<Callback> retrieved);

 private:
  friend class RetrieverTest;

  // Callback for when a rule is retrieved from |storage_|.
  void OnDataRetrievedFromStorage(bool success,
                                  const std::string& key,
                                  const std::string& stored_data);

  // Callback for when a rule is retrieved by |downloader_|.
  void OnDownloaded(bool success,
                    const std::string& url,
                    scoped_ptr<std::string> downloaded_data);

  // Returns the URL where the |key| can be retrieved. For example, returns
  // "https://i18napis.appspot.com/ssl-aggregate-address/data/US" for input
  // "data/US". Assumes that the input string is a valid URL segment.
  std::string GetUrlForKey(const std::string& key) const;

  // Returns the key for the |url|. For example, returns "data/US" for
  // "https://i18napis.appspot.com/ssl-aggregate-address/data/US". If the |url|
  // does not start with |validation_data_url| that was passed to the
  // constructor, then returns an empty string. (This can happen if the user of
  // the library returns a bad URL in their Downloader implementation.)
  std::string GetKeyForUrl(const std::string& url) const;

  // Returns true if the |url| starts with |validation_data_url| that was passed
  // to the constructor.
  bool IsValidationDataUrl(const std::string& url) const;

  // Looks up the callback for |key| in |requests_|, removes it from the map and
  // invokes it with |key|, |success|, and |data| parameters.
  void InvokeCallbackForKey(const std::string& key,
                            bool success,
                            const std::string& data);

  const std::string validation_data_url_;
  scoped_ptr<Downloader> downloader_;
  scoped_ptr<Storage> storage_;

  // Holds pending requests. The callback pointers are owned.
  std::map<std::string, Callback*> requests_;

  // Holds data from storage that has expired timestamps. If a download fails,
  // then this data is used as fallback.
  std::map<std::string, std::string> stale_data_;

  DISALLOW_COPY_AND_ASSIGN(Retriever);
};

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_RETRIEVER_H_
