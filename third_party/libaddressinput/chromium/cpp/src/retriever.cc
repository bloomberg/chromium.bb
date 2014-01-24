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

// The prefix for the timestamp line in the header.
const char kTimestampPrefix[] = "timestamp=";
const size_t kTimestampPrefixLength = sizeof kTimestampPrefix - 1;

// The prefix for the checksum line in the header.
const char kChecksumPrefix[] = "checksum=";
const size_t kChecksumPrefixLength = sizeof kChecksumPrefix - 1;

// The separator between lines of header and data.
const char kSeparator = '\n';

// Returns |data| with attached checksum and current timestamp. Format:
//
//    timestamp=<timestamp>
//    checksum=<checksum>
//    <data>
//
// The timestamp is the time_t that was returned from time(NULL) function. The
// timestamp does not need to be portable because it is written and read only by
// Retriever. The value is somewhat human-readable: it is the number of seconds
// since the epoch.
//
// The checksum is the 32-character hexadecimal MD5 checksum of <data>. It is
// meant to protect from random file changes on disk.
std::string PrependTimestamp(const std::string& data) {
  std::string wrapped;
  wrapped.append(kTimestampPrefix, kTimestampPrefixLength);
  wrapped.append(TimeToString(time(NULL)));
  wrapped.push_back(kSeparator);

  wrapped.append(kChecksumPrefix, kChecksumPrefixLength);
  wrapped.append(MD5String(data));
  wrapped.push_back(kSeparator);
  wrapped.append(data);

  return wrapped;
}

// Places the header value into |header_value| parameter and the rest of the
// data into |data| parameter. Returns |true| if the header format is valid.
bool ExtractHeader(const std::string& header_and_data,
                   const char* header_prefix,
                   size_t header_prefix_length,
                   std::string* header_value,
                   std::string* data) {
  assert(header_prefix != NULL);
  assert(header_value != NULL);
  assert(data != NULL);

  if (header_and_data.compare(
          0, header_prefix_length, header_prefix, header_prefix_length) != 0) {
    return false;
  }

  std::string::size_type separator_position =
      header_and_data.find(kSeparator, header_prefix_length);
  if (separator_position == std::string::npos) {
    return false;
  }

  data->assign(header_and_data, separator_position + 1, std::string::npos);
  header_value->assign(header_and_data, header_prefix_length,
                       separator_position - header_prefix_length);
  return true;
}

// Strips out the timestamp and checksum from |header_and_data|. Validates the
// checksum. Saves the header-less data into |data|. Compares the parsed
// timestamp with current time and saves the difference into |age_in_seconds|.
//
// The parameters should not be NULL. Does not take ownership of its parameters.
//
// Returns |true| if |header_and_data| is correctly formatted and has the
// correct checksum.
bool VerifyAndExtractTimestamp(const std::string& header_and_data,
                               std::string* data,
                               double* age_in_seconds) {
  assert(data != NULL);
  assert(age_in_seconds != NULL);

  std::string timestamp_string;
  std::string checksum_and_data;
  if (!ExtractHeader(header_and_data, kTimestampPrefix, kTimestampPrefixLength,
                     &timestamp_string, &checksum_and_data)) {
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
  if (!ExtractHeader(checksum_and_data, kChecksumPrefix, kChecksumPrefixLength,
                     &checksum, data)) {
    return false;
  }

  return checksum == MD5String(*data);
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
    std::string unwrapped;
    double age_in_seconds = 0.0;
    if (VerifyAndExtractTimestamp(stored_data, &unwrapped, &age_in_seconds)) {
      if (age_in_seconds < kStaleDataAgeInSeconds) {
        InvokeCallbackForKey(key, success, unwrapped);
        return;
      }
      stale_data_[key] = unwrapped;
    }
  }

  downloader_->Download(GetUrlForKey(key),
                        BuildCallback(this, &Retriever::OnDownloaded));
}

void Retriever::OnDownloaded(bool success,
                             const std::string& url,
                             const std::string& downloaded_data) {
  const std::string& key = GetKeyForUrl(url);
  std::map<std::string, std::string>::iterator stale_data_it =
      stale_data_.find(key);

  if (success) {
    storage_->Put(key, PrependTimestamp(downloaded_data));
    InvokeCallbackForKey(key, success, downloaded_data);
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
