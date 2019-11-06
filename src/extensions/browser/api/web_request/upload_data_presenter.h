// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_UPLOAD_DATA_PRESENTER_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_UPLOAD_DATA_PRESENTER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"

namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
class Value;
}

namespace extensions {
class FormDataParser;
}

namespace net {
class HttpRequestHeaders;
}

namespace extensions {

namespace subtle {

// Helpers shared with unit-tests.

// Appends a dictionary {'key': 'value'} to |list|.
void AppendKeyValuePair(const char* key,
                        std::unique_ptr<base::Value> value,
                        base::ListValue* list);

}  // namespace subtle

FORWARD_DECLARE_TEST(WebRequestUploadDataPresenterTest, RawData);

// UploadDataPresenter is an interface for objects capable to consume a series
// of UploadElementReader and represent this data as a base:Value.
//
// Workflow for objects implementing this interface:
// 1. Call object->FeedNext(reader) for each element from the request's body.
// 2. Check if object->Succeeded().
// 3. If that check passed then retrieve object->Result().
class UploadDataPresenter {
 public:
  virtual ~UploadDataPresenter();
  virtual void FeedBytes(base::StringPiece bytes) = 0;
  virtual void FeedFile(const base::FilePath& path) = 0;
  virtual bool Succeeded() = 0;
  virtual std::unique_ptr<base::Value> Result() = 0;

 protected:
  UploadDataPresenter() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(UploadDataPresenter);
};

// This class passes all the bytes from bytes elements as a BinaryValue for each
// such element. File elements are presented as StringValue containing the path
// for that file.
class RawDataPresenter : public UploadDataPresenter {
 public:
  RawDataPresenter();
  ~RawDataPresenter() override;

  // Implementation of UploadDataPresenter.
  void FeedBytes(base::StringPiece bytes) override;
  void FeedFile(const base::FilePath& path) override;
  bool Succeeded() override;
  std::unique_ptr<base::Value> Result() override;

 private:
  void FeedNextBytes(const char* bytes, size_t size);
  void FeedNextFile(const std::string& filename);
  FRIEND_TEST_ALL_PREFIXES(WebRequestUploadDataPresenterTest, RawData);

  const bool success_;
  std::unique_ptr<base::ListValue> list_;

  DISALLOW_COPY_AND_ASSIGN(RawDataPresenter);
};

// This class inspects the contents of bytes elements. It uses the
// parser classes inheriting from FormDataParser to parse the concatenated
// content of such elements. If the parsing is successful, the parsed form is
// returned as a DictionaryValue. For example, a form consisting of
// <input name="check" type="checkbox" value="A" checked />
// <input name="check" type="checkbox" value="B" checked />
// <input name="text" type="text" value="abc" />
// would be represented as {"check": ["A", "B"], "text": ["abc"]} (although as a
// DictionaryValue, not as a JSON string).
class ParsedDataPresenter : public UploadDataPresenter {
 public:
  explicit ParsedDataPresenter(const net::HttpRequestHeaders& request_headers);
  ~ParsedDataPresenter() override;

  // Implementation of UploadDataPresenter.
  void FeedBytes(base::StringPiece bytes) override;
  void FeedFile(const base::FilePath& path) override;
  bool Succeeded() override;
  std::unique_ptr<base::Value> Result() override;

  // Allows to create ParsedDataPresenter without request headers. Uses the
  // parser for "application/x-www-form-urlencoded" form encoding. Only use this
  // in tests.
  static std::unique_ptr<ParsedDataPresenter> CreateForTests();

 private:
  // This constructor is used in CreateForTests.
  explicit ParsedDataPresenter(const std::string& form_type);

  // Clears resources and the success flag.
  void Abort();

  std::unique_ptr<FormDataParser> parser_;
  bool success_;
  std::unique_ptr<base::DictionaryValue> dictionary_;

  DISALLOW_COPY_AND_ASSIGN(ParsedDataPresenter);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_UPLOAD_DATA_PRESENTER_H_
