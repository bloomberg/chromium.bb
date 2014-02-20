// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/gnubby_util.h"

#include <algorithm>
#include <vector>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "remoting/base/logging.h"

namespace remoting {

namespace {

// Failure code to use when the code from the webapp response isn't available.
const int kGnubbyResponseFail = 1;

const int kSsh2AgentcGnubbySignRequest = 101;   // 0x65
const int kSsh2AgentcGnubbySignResponse = 102;  // 0x66

const char kAppIdHash[] = "appIdHash";
const char kChallengeHash[] = "challengeHash";
const char kCode[] = "code";
const char kKeyHandle[] = "keyHandle";
const char kResponseData[] = "responseData";
const char kSignData[] = "signData";
const char kSignReply[] = "sign_helper_reply";
const char kSignRequest[] = "sign_helper_request";
const char kSignatureData[] = "signatureData";
const char kTimeout[] = "timeout";
const char kType[] = "type";
const char kVersion[] = "version";

void WebSafeBase64Encode(const std::string& data, std::string* encoded_data) {
  base::Base64Encode(data, encoded_data);

  std::replace(encoded_data->begin(), encoded_data->end(), '+', '-');
  std::replace(encoded_data->begin(), encoded_data->end(), '/', '_');
  encoded_data->erase(
      std::remove(encoded_data->begin(), encoded_data->end(), '='),
      encoded_data->end());
}

void WebSafeBase64Decode(const std::string& encoded_data, std::string* data) {
  std::string temp(encoded_data);
  std::replace(temp.begin(), temp.end(), '-', '+');
  std::replace(temp.begin(), temp.end(), '_', '/');

  int num_equals = temp.length() % 3;
  temp.append(num_equals, '=');

  base::Base64Decode(temp, data);
}

bool DecodeDataFromDictionary(const base::DictionaryValue& dictionary,
                              const std::string& path,
                              std::string* data) {
  std::string encoded_data;
  bool result = dictionary.GetString(path, &encoded_data);
  if (result) {
    WebSafeBase64Decode(encoded_data, data);
  } else {
    LOG(ERROR) << "Failed to get dictionary value " << path;
    data->erase();
  }
  return result;
}

// Class to read gnubby blob data.
class BlobReader {
 public:
  // Create a blob with the given data. Does not take ownership of the memory.
  BlobReader(const uint8_t* data, size_t data_len);
  virtual ~BlobReader();

  // Read a byte from the blob. Returns true on success.
  bool ReadByte(uint8_t* value);

  // Read a four byte size from the blob. Returns true on success.
  bool ReadSize(size_t* value);

  // Read a size-prefixed blob. Returns true on success.
  bool ReadBlobReader(scoped_ptr<BlobReader>* value);

  // Read a size-prefixed string from the blob. Returns true on success.
  bool ReadString(std::string* value);

 private:
  // The blob data.
  const uint8_t* data_;

  // The length of the blob data.
  size_t data_len_;

  // The current read index.
  size_t index_;

  DISALLOW_COPY_AND_ASSIGN(BlobReader);
};

// Class to write gnubby blob data.
class BlobWriter {
 public:
  BlobWriter();
  virtual ~BlobWriter();

  // Write a byte to the blob.
  void WriteByte(uint8_t value);

  // Write a four byte size to the blob.
  void WriteSize(size_t value);

  // Write a size-prefixed blob to the blob.
  void WriteBlobWriter(const BlobWriter& value);

  // Write a size-prefixed string to the blob.
  void WriteString(const std::string& value);

  // Returns the blob data.
  std::string GetData() const;

 private:
  // The blob data.
  std::vector<uint8_t> data_;

  DISALLOW_COPY_AND_ASSIGN(BlobWriter);
};

BlobReader::BlobReader(const uint8_t* data, size_t data_len)
    : data_(data), data_len_(data_len), index_(0) {}

BlobReader::~BlobReader() {}

bool BlobReader::ReadByte(uint8_t* value) {
  if (data_len_ < index_) {
    *value = 0;
    return false;
  }
  *value = data_[index_++];
  return true;
}

bool BlobReader::ReadSize(size_t* value) {
  if (data_len_ < (index_ + 4)) {
    *value = 0;
    return false;
  }
  *value = ((data_[index_] & 255) << 24) + ((data_[index_ + 1] & 255) << 16) +
           ((data_[index_ + 2] & 255) << 8) + (data_[index_ + 3] & 255);
  index_ += 4;
  return true;
}

bool BlobReader::ReadBlobReader(scoped_ptr<BlobReader>* value) {
  size_t blob_size;
  if (!ReadSize(&blob_size) || data_len_ < (index_ + blob_size)) {
    value->reset();
    return 0;
  }
  value->reset(new BlobReader(data_ + index_, blob_size));
  index_ += blob_size;
  return true;
}

bool BlobReader::ReadString(std::string* value) {
  size_t length;
  if (!ReadSize(&length) || data_len_ < (index_ + length)) {
    value->erase();
    return 0;
  }
  value->assign(reinterpret_cast<const char*>(data_ + index_), length);
  index_ += length;
  return true;
}

BlobWriter::BlobWriter() {}

BlobWriter::~BlobWriter() {}

void BlobWriter::WriteByte(uint8_t value) { data_.push_back(value); }

void BlobWriter::WriteSize(size_t value) {
  data_.push_back((value & 0xff000000) >> 24);
  data_.push_back((value & 0xff0000) >> 16);
  data_.push_back((value & 0xff00) >> 8);
  data_.push_back(value & 0xff);
}

void BlobWriter::WriteBlobWriter(const BlobWriter& value) {
  WriteString(value.GetData());
}

void BlobWriter::WriteString(const std::string& value) {
  WriteSize(value.length());
  data_.insert(data_.end(), value.begin(), value.end());
}

std::string BlobWriter::GetData() const {
  return std::string(reinterpret_cast<const char*>(data_.data()), data_.size());
}

}  // namespace

bool GetJsonFromGnubbyRequest(const char* data,
                              int data_len,
                              std::string* json) {
  json->empty();

  BlobReader ssh_request(reinterpret_cast<const uint8_t*>(data), data_len);
  scoped_ptr<BlobReader> blob;
  if (!ssh_request.ReadBlobReader(&blob))
    return false;

  uint8_t cmd = 0;
  uint8_t timeout = 0;
  size_t request_count = 0;
  bool result = blob->ReadByte(&cmd);
  result = result && blob->ReadByte(&timeout);
  result = result && blob->ReadSize(&request_count);
  if (!result || cmd != kSsh2AgentcGnubbySignRequest)
    return false;

  base::DictionaryValue request;
  request.SetString(kType, kSignRequest);
  request.SetInteger(kTimeout, timeout);

  base::ListValue* sign_requests = new base::ListValue();
  request.Set(kSignData, sign_requests);

  for (unsigned int i = 0; i < request_count; ++i) {
    scoped_ptr<BlobReader> sign_request;
    std::string version;
    std::string challenge_hash;
    std::string origin_hash;
    std::string key_handle;

    if (!(blob->ReadBlobReader(&sign_request) &&
          sign_request->ReadString(&version) &&
          sign_request->ReadString(&challenge_hash) &&
          sign_request->ReadString(&origin_hash) &&
          sign_request->ReadString(&key_handle)))
      return false;

    std::string encoded_origin_hash;
    std::string encoded_challenge_hash;
    std::string encoded_key_handle;

    WebSafeBase64Encode(origin_hash, &encoded_origin_hash);
    WebSafeBase64Encode(challenge_hash, &encoded_challenge_hash);
    WebSafeBase64Encode(key_handle, &encoded_key_handle);

    base::DictionaryValue* request = new base::DictionaryValue();
    request->SetString(kAppIdHash, encoded_origin_hash);
    request->SetString(kChallengeHash, encoded_challenge_hash);
    request->SetString(kKeyHandle, encoded_key_handle);
    request->SetString(kVersion, version);
    sign_requests->Append(request);
  }

  base::JSONWriter::Write(&request, json);
  return true;
}

void GetGnubbyResponseFromJson(const std::string& json, std::string* data) {
  data->erase();

  scoped_ptr<base::Value> json_value(base::JSONReader::Read(json));
  base::DictionaryValue* reply;
  if (json_value && json_value->GetAsDictionary(&reply)) {
    BlobWriter response;
    response.WriteByte(kSsh2AgentcGnubbySignResponse);

    int code;
    if (reply->GetInteger(kCode, &code) && code == 0) {
      response.WriteSize(code);

      std::string type;
      if (!(reply->GetString(kType, &type) && type == kSignReply)) {
        LOG(ERROR) << "Invalid type";
        return;
      }

      base::DictionaryValue* reply_data;
      if (!reply->GetDictionary(kResponseData, &reply_data)) {
        LOG(ERROR) << "Invalid response data";
        return;
      }

      BlobWriter tmp;
      std::string version;
      if (reply_data->GetString(kVersion, &version)) {
        tmp.WriteString(version);
      } else {
        tmp.WriteSize(0);
      }

      std::string challenge_hash;
      if (!DecodeDataFromDictionary(
               *reply_data, kChallengeHash, &challenge_hash)) {
        LOG(ERROR) << "Invalid challenge hash";
        return;
      }
      tmp.WriteString(challenge_hash);

      std::string app_id_hash;
      if (!DecodeDataFromDictionary(*reply_data, kAppIdHash, &app_id_hash)) {
        LOG(ERROR) << "Invalid app id hash";
        return;
      }
      tmp.WriteString(app_id_hash);

      std::string key_handle;
      if (!DecodeDataFromDictionary(*reply_data, kKeyHandle, &key_handle)) {
        LOG(ERROR) << "Invalid key handle";
        return;
      }
      tmp.WriteString(key_handle);

      std::string signature_data;
      if (!DecodeDataFromDictionary(
               *reply_data, kSignatureData, &signature_data)) {
        LOG(ERROR) << "Invalid signature data";
        return;
      }
      tmp.WriteString(signature_data);

      response.WriteBlobWriter(tmp);
    } else {
      response.WriteSize(kGnubbyResponseFail);
    }

    BlobWriter ssh_response;
    ssh_response.WriteBlobWriter(response);
    data->assign(ssh_response.GetData());
  } else {
    LOG(ERROR) << "Could not parse json: " << json;
  }
}

}  // namespace remoting
