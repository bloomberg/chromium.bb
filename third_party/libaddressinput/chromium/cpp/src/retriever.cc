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
#include <cstdlib>
#include <ctime>
#include <map>
#include <string>

#include "fallback_data_store.h"
#include "time_to_string.h"
#include "util/md5.h"
#include "util/stl_util.h"

namespace i18n {
namespace addressinput {

namespace {

// The number of seconds after which data is considered stale. The staleness
// threshold is 30 days:
//    30 days *
//    24 hours per day *
//    60 minutes per hour *
//    60 seconds per minute.
static const double kStaleDataAgeInSeconds = 30.0 * 24.0 * 60.0 * 60.0;

// The prefix for the timestamp line in the footer.
const char kTimestampPrefix[] = "timestamp=";

// The prefix for the checksum line in the footer.
const char kChecksumPrefix[] = "checksum=";

// The separator between lines of footer and data.
const char kSeparator = '\n';

// Returns |data| with attached checksum and current timestamp. Format:
//
//    <data>
//    checksum=<checksum>
//    timestamp=<timestamp>
//
// The timestamp is the time_t that was returned from time(NULL) function. The
// timestamp does not need to be portable because it is written and read only by
// Retriever. The value is somewhat human-readable: it is the number of seconds
// since the epoch.
//
// The checksum is the 32-character hexadecimal MD5 checksum of <data>. It is
// meant to protect from random file changes on disk.
void AppendTimestamp(std::string* data) {
  std::string md5 = MD5String(*data);

  data->push_back(kSeparator);
  data->append(kChecksumPrefix);
  data->append(md5);

  data->push_back(kSeparator);
  data->append(kTimestampPrefix);
  data->append(TimeToString(time(NULL)));
}

// Places the footer value into |footer_value| parameter and the rest of the
// data into |data| parameter. Returns |true| if the footer format is valid.
bool ExtractFooter(scoped_ptr<std::string> data_and_footer,
                   const std::string& footer_prefix,
                   std::string* footer_value,
                   scoped_ptr<std::string>* data) {
  assert(footer_value != NULL);
  assert(data != NULL);

  std::string::size_type separator_position =
      data_and_footer->rfind(kSeparator);
  if (separator_position == std::string::npos) {
    return false;
  }

  std::string::size_type footer_start = separator_position + 1;
  if (data_and_footer->compare(footer_start,
                               footer_prefix.length(),
                               footer_prefix) != 0) {
    return false;
  }

  *footer_value =
      data_and_footer->substr(footer_start + footer_prefix.length());
  *data = data_and_footer.Pass();
  (*data)->resize(separator_position);
  return true;
}

// Strips out the timestamp and checksum from |data_and_footer|. Validates the
// checksum. Saves the footer-less data into |data|. Compares the parsed
// timestamp with current time and saves the difference into |age_in_seconds|.
//
// The parameters should not be NULL.
//
// Returns |true| if |data_and_footer| is correctly formatted and has the
// correct checksum.
bool VerifyAndExtractTimestamp(const std::string& data_and_footer,
                               scoped_ptr<std::string>* data,
                               double* age_in_seconds) {
  assert(data != NULL);
  assert(age_in_seconds != NULL);

  std::string timestamp_string;
  scoped_ptr<std::string> checksum_and_data;
  if (!ExtractFooter(make_scoped_ptr(new std::string(data_and_footer)),
                     kTimestampPrefix, &timestamp_string, &checksum_and_data)) {
    return false;
  }

  time_t timestamp = atol(timestamp_string.c_str());
  if (timestamp < 0) {
    return false;
  }

  *age_in_seconds = difftime(time(NULL), timestamp);
  if (*age_in_seconds < 0.0) {
    return false;
  }

  std::string checksum;
  if (!ExtractFooter(checksum_and_data.Pass(),
                     kChecksumPrefix, &checksum, data)) {
    return false;
  }

  return checksum == MD5String(**data);
}

}  // namespace

Retriever::Retriever(const std::string& validation_data_url,
                     scoped_ptr<Downloader> downloader,
                     scoped_ptr<Storage> storage)
    : validation_data_url_(validation_data_url),
      downloader_(downloader.Pass()),
      storage_(storage.Pass()),
      stale_data_() {
  assert(validation_data_url_.length() > 0);
  assert(validation_data_url_[validation_data_url_.length() - 1] == '/');
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
                                           const std::string& stored_data) {
  if (success) {
    scoped_ptr<std::string> unwrapped;
    double age_in_seconds = 0.0;
    if (VerifyAndExtractTimestamp(stored_data, &unwrapped, &age_in_seconds)) {
      if (age_in_seconds < kStaleDataAgeInSeconds) {
        InvokeCallbackForKey(key, success, *unwrapped);
        return;
      }
      stale_data_[key].swap(*unwrapped);
    }
  }

  downloader_->Download(GetUrlForKey(key),
                        BuildScopedPtrCallback(this, &Retriever::OnDownloaded));
}

void Retriever::OnDownloaded(bool success,
                             const std::string& url,
                             scoped_ptr<std::string> downloaded_data) {
  const std::string& key = GetKeyForUrl(url);
  std::map<std::string, std::string>::iterator stale_data_it =
      stale_data_.find(key);

  if (success) {
    InvokeCallbackForKey(key, success, *downloaded_data);
    AppendTimestamp(downloaded_data.get());
    storage_->Put(key, downloaded_data.Pass());
  } else if (stale_data_it != stale_data_.end()) {
    InvokeCallbackForKey(key, true, stale_data_it->second);
  } else {
    std::string fallback;
    success = FallbackDataStore::Get(key, &fallback);
    InvokeCallbackForKey(key, success, fallback);
  }

  if (stale_data_it != stale_data_.end()) {
    stale_data_.erase(stale_data_it);
  }
}

std::string Retriever::GetUrlForKey(const std::string& key) const {
  return validation_data_url_ + key;
}

std::string Retriever::GetKeyForUrl(const std::string& url) const {
  return
      url.compare(0, validation_data_url_.length(), validation_data_url_) == 0
          ? url.substr(validation_data_url_.length())
          : std::string();
}

bool Retriever::IsValidationDataUrl(const std::string& url) const {
  return
      url.compare(0, validation_data_url_.length(), validation_data_url_) == 0;
}

void Retriever::InvokeCallbackForKey(const std::string& key,
                                     bool success,
                                     const std::string& data) {
  std::map<std::string, Callback*>::iterator iter =
      requests_.find(key);
  if (iter == requests_.end()) {
    // An abandoned request.
    return;
  }
  scoped_ptr<Callback> callback(iter->second);
  requests_.erase(iter);
  if (callback == NULL) {
    return;
  }
  (*callback)(success, key, data);
}

}  // namespace addressinput
}  // namespace i18n
