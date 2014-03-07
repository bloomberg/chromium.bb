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

#include "fake_downloader.h"

#include <cassert>
#include <fstream>
#include <map>
#include <string>
#include <utility>

namespace i18n {
namespace addressinput {

// static
const char FakeDownloader::kFakeDataUrl[] = "test:///";

namespace {

// The name of the test data file.
const char kDataFileName[] = TEST_DATA_DIR "/countryinfo.txt";

// The number of characters in the fake data URL prefix.
const size_t kFakeDataUrlLength = sizeof FakeDownloader::kFakeDataUrl - 1;

// Returns "data/HK" for "data/HK--en".
std::string RemoveLanguageCode(const std::string& key) {
  std::string::size_type language_code_pos = key.find("--");
  return language_code_pos == std::string::npos
      ? key
      : key.substr(0, language_code_pos);
}

std::string CCKey(const std::string& key) {
  const char kSplitChar = '/';

  std::string::size_type split = key.find(kSplitChar);
  if (split == std::string::npos) {
    return key;
  }
  split = key.find(kSplitChar, split + 1);
  if (split == std::string::npos) {
    return key;
  }

  return key.substr(0, split);
}

std::map<std::string, std::string> InitData() {
  std::map<std::string, std::string> data;
  std::ifstream file(kDataFileName);
  assert(file.is_open());

  std::string line;
  while (file.good()) {
    std::getline(file, line);

    std::string::size_type divider = line.find('=');
    if (divider == std::string::npos) {
      continue;
    }

    std::string key = line.substr(0, divider);
    std::string cc_key = RemoveLanguageCode(CCKey(key));
    std::string value = line.substr(divider + 1);
    std::string url = FakeDownloader::kFakeDataUrl + cc_key;
    std::map<std::string, std::string>::iterator data_it = data.find(url);
    if (data_it != data.end()) {
      data_it->second += ", \"" + key + "\": " + value;
    } else {
      data.insert(std::make_pair(url, "{\"" + key + "\": " + value));
    }
  }
  file.close();

  for (std::map<std::string, std::string>::iterator data_it = data.begin();
       data_it != data.end(); ++data_it) {
    data_it->second += "}";
  }

  return data;
}

const std::map<std::string, std::string>& GetData() {
  static const std::map<std::string, std::string> kData(InitData());
  return kData;
}

}  // namespace

FakeDownloader::FakeDownloader() {}

FakeDownloader::~FakeDownloader() {}

void FakeDownloader::Download(const std::string& url,
                              scoped_ptr<Callback> downloaded) {
  std::map<std::string, std::string>::const_iterator data_it =
      GetData().find(url);
  bool success = data_it != GetData().end();
  std::string data = success ? data_it->second : std::string();
  if (!success &&
      url.compare(
          0, kFakeDataUrlLength, kFakeDataUrl, kFakeDataUrlLength) == 0) {
    // URLs that start with
    // "https://i18napis.appspot.com/ssl-aggregate-address/" prefix, but do not
    // have associated data will always return "{}" with status code 200.
    // FakeDownloader imitates this behavior for URLs that start with "test:///"
    // prefix.
    success = true;
    data = "{}";
  }
  (*downloaded)(success, url, make_scoped_ptr(new std::string(data)));
}

}  // namespace addressinput
}  // namespace i18n
