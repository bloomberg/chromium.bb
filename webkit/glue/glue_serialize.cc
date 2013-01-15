// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/glue_serialize.h"

#include <string>

#include "base/pickle.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebData.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebHTTPBody.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSerializedScriptValue.h"
#include "webkit/base/file_path_string_conversions.h"

using WebKit::WebData;
using WebKit::WebHistoryItem;
using WebKit::WebHTTPBody;
using WebKit::WebPoint;
using WebKit::WebSerializedScriptValue;
using WebKit::WebString;
using WebKit::WebUChar;
using WebKit::WebVector;

namespace webkit_glue {

namespace {

enum IncludeFormData {
  NEVER_INCLUDE_FORM_DATA,
  INCLUDE_FORM_DATA_WITHOUT_PASSWORDS,
  ALWAYS_INCLUDE_FORM_DATA
};

struct SerializeObject {
  SerializeObject() : version(0) {}
  SerializeObject(const char* data, int len)
      : pickle(data, len), version(0) { iter = PickleIterator(pickle); }

  std::string GetAsString() {
    return std::string(static_cast<const char*>(pickle.data()), pickle.size());
  }

  Pickle pickle;
  mutable PickleIterator iter;
  mutable int version;
};

// TODO(mpcomplete): obsolete versions 1 and 2 after 1/1/2008.
// Version ID used in reading/writing history items.
// 1: Initial revision.
// 2: Added case for NULL string versus "". Version 2 code can read Version 1
//    data, but not vice versa.
// 3: Version 2 was broken, it stored number of WebUChars, not number of bytes.
//    This version checks and reads v1 and v2 correctly.
// 4: Adds support for storing FormData::identifier().
// 5: Adds support for empty FormData
// 6: Adds support for documentSequenceNumbers
// 7: Adds support for stateObject
// 8: Adds support for file range and modification time
// 9: Adds support for itemSequenceNumbers
// 10: Adds support for blob
// 11: Adds support for pageScaleFactor
// 12: Adds support for hasPasswordData in HTTP body
// 13: Adds support for URL (FileSystem URL)
// Should be const, but unit tests may modify it.
//
// NOTE: If the version is -1, then the pickle contains only a URL string.
// See CreateHistoryStateForURL.
//
int kVersion = 13;

// A bunch of convenience functions to read/write to SerializeObjects.
// The serializers assume the input data is in the correct format and so does
// no error checking.
inline void WriteData(const void* data, int length, SerializeObject* obj) {
  obj->pickle.WriteData(static_cast<const char*>(data), length);
}

inline void ReadData(const SerializeObject* obj, const void** data,
                     int* length) {
  const char* tmp;
  if (obj->pickle.ReadData(&obj->iter, &tmp, length)) {
    *data = tmp;
  } else {
    *data = NULL;
    *length = 0;
  }
}

inline bool ReadBytes(const SerializeObject* obj, const void** data,
                     int length) {
  const char *tmp;
  if (!obj->pickle.ReadBytes(&obj->iter, &tmp, length))
    return false;
  *data = tmp;
  return true;
}

inline void WriteInteger(int data, SerializeObject* obj) {
  obj->pickle.WriteInt(data);
}

inline int ReadInteger(const SerializeObject* obj) {
  int tmp;
  if (obj->pickle.ReadInt(&obj->iter, &tmp))
    return tmp;
  return 0;
}

inline void WriteInteger64(int64 data, SerializeObject* obj) {
  obj->pickle.WriteInt64(data);
}

inline int64 ReadInteger64(const SerializeObject* obj) {
  int64 tmp = 0;
  obj->pickle.ReadInt64(&obj->iter, &tmp);
  return tmp;
}

inline void WriteReal(double data, SerializeObject* obj) {
  WriteData(&data, sizeof(double), obj);
}

inline double ReadReal(const SerializeObject* obj) {
  const void* tmp = NULL;
  int length = 0;
  ReadData(obj, &tmp, &length);
  if (tmp && length > 0 && length >= static_cast<int>(sizeof(0.0)))
    return *static_cast<const double*>(tmp);
  else
    return 0.0;
}

inline void WriteBoolean(bool data, SerializeObject* obj) {
  obj->pickle.WriteInt(data ? 1 : 0);
}

inline bool ReadBoolean(const SerializeObject* obj) {
  bool tmp;
  if (obj->pickle.ReadBool(&obj->iter, &tmp))
    return tmp;
  return false;
}

inline void WriteGURL(const GURL& url, SerializeObject* obj) {
  obj->pickle.WriteString(url.possibly_invalid_spec());
}

inline GURL ReadGURL(const SerializeObject* obj) {
  std::string spec;
  if (obj->pickle.ReadString(&obj->iter, &spec))
    return GURL(spec);
  return GURL();
}

// Read/WriteString pickle the WebString as <int length><WebUChar* data>.
// If length == -1, then the WebString itself is NULL (WebString()).
// Otherwise the length is the number of WebUChars (not bytes) in the WebString.
inline void WriteString(const WebString& str, SerializeObject* obj) {
  switch (kVersion) {
    case 1:
      // Version 1 writes <length in bytes><string data>.
      // It saves WebString() and "" as "".
      obj->pickle.WriteInt(str.length() * sizeof(WebUChar));
      obj->pickle.WriteBytes(str.data(), str.length() * sizeof(WebUChar));
      break;
    case 2:
      // Version 2 writes <length in WebUChar><string data>.
      // It uses -1 in the length field to mean WebString().
      if (str.isNull()) {
        obj->pickle.WriteInt(-1);
      } else {
        obj->pickle.WriteInt(str.length());
        obj->pickle.WriteBytes(str.data(),
                               str.length() * sizeof(WebUChar));
      }
      break;
    default:
      // Version 3+ writes <length in bytes><string data>.
      // It uses -1 in the length field to mean WebString().
      if (str.isNull()) {
        obj->pickle.WriteInt(-1);
      } else {
        obj->pickle.WriteInt(str.length() * sizeof(WebUChar));
        obj->pickle.WriteBytes(str.data(),
                               str.length() * sizeof(WebUChar));
      }
      break;
  }
}

// This reads a serialized WebString from obj. If a string can't be read,
// WebString() is returned.
inline WebString ReadString(const SerializeObject* obj) {
  int length;

  // Versions 1, 2, and 3 all start with an integer.
  if (!obj->pickle.ReadInt(&obj->iter, &length))
    return WebString();

  // Starting with version 2, -1 means WebString().
  if (length == -1)
    return WebString();

  // In version 2, the length field was the length in WebUChars.
  // In version 1 and 3 it is the length in bytes.
  int bytes = length;
  if (obj->version == 2)
    bytes *= sizeof(WebUChar);

  const void* data;
  if (!ReadBytes(obj, &data, bytes))
    return WebString();
  return WebString(static_cast<const WebUChar*>(data),
                   bytes / sizeof(WebUChar));
}

// Writes a Vector of Strings into a SerializeObject for serialization.
void WriteStringVector(
    const WebVector<WebString>& data, SerializeObject* obj) {
  WriteInteger(static_cast<int>(data.size()), obj);
  for (size_t i = 0, c = data.size(); i < c; ++i) {
    unsigned ui = static_cast<unsigned>(i);  // sigh
    WriteString(data[ui], obj);
  }
}

WebVector<WebString> ReadStringVector(const SerializeObject* obj) {
  int num_elements = ReadInteger(obj);
  WebVector<WebString> result(static_cast<size_t>(num_elements));
  for (int i = 0; i < num_elements; ++i)
    result[i] = ReadString(obj);
  return result;
}

// Writes a FormData object into a SerializeObject for serialization.
void WriteFormData(const WebHTTPBody& http_body, SerializeObject* obj) {
  WriteBoolean(!http_body.isNull(), obj);

  if (http_body.isNull())
    return;

  WriteInteger(static_cast<int>(http_body.elementCount()), obj);
  WebHTTPBody::Element element;
  for (size_t i = 0; http_body.elementAt(i, element); ++i) {
    WriteInteger(element.type, obj);
    if (element.type == WebHTTPBody::Element::TypeData) {
      WriteData(element.data.data(), static_cast<int>(element.data.size()),
                obj);
    } else if (element.type == WebHTTPBody::Element::TypeFile) {
      WriteString(element.filePath, obj);
      WriteInteger64(element.fileStart, obj);
      WriteInteger64(element.fileLength, obj);
      WriteReal(element.modificationTime, obj);
    } else if (element.type == WebHTTPBody::Element::TypeURL) {
      WriteGURL(element.url, obj);
      WriteInteger64(element.fileStart, obj);
      WriteInteger64(element.fileLength, obj);
      WriteReal(element.modificationTime, obj);
    } else {
      WriteGURL(element.url, obj);
    }
  }
  WriteInteger64(http_body.identifier(), obj);
  WriteBoolean(http_body.containsPasswordData(), obj);
}

WebHTTPBody ReadFormData(const SerializeObject* obj) {
  // In newer versions, an initial boolean indicates if we have form data.
  if (obj->version >= 5 && !ReadBoolean(obj))
    return WebHTTPBody();

  // In older versions, 0 elements implied no form data.
  int num_elements = ReadInteger(obj);
  if (num_elements == 0 && obj->version < 5)
    return WebHTTPBody();

  WebHTTPBody http_body;
  http_body.initialize();

  for (int i = 0; i < num_elements; ++i) {
    int type = ReadInteger(obj);
    if (type == WebHTTPBody::Element::TypeData) {
      const void* data;
      int length = -1;
      ReadData(obj, &data, &length);
      if (length >= 0)
        http_body.appendData(WebData(static_cast<const char*>(data), length));
    } else if (type == WebHTTPBody::Element::TypeFile) {
      WebString file_path = ReadString(obj);
      long long file_start = 0;
      long long file_length = -1;
      double modification_time = 0.0;
      if (obj->version >= 8) {
        file_start = ReadInteger64(obj);
        file_length = ReadInteger64(obj);
        modification_time = ReadReal(obj);
      }
      http_body.appendFileRange(file_path, file_start, file_length,
                                modification_time);
    } else if (type == WebHTTPBody::Element::TypeURL) {
      GURL url = ReadGURL(obj);
      long long file_start = 0;
      long long file_length = -1;
      double modification_time = 0.0;
      file_start = ReadInteger64(obj);
      file_length = ReadInteger64(obj);
      modification_time = ReadReal(obj);
      http_body.appendURLRange(url, file_start, file_length,
                               modification_time);
    } else if (obj->version >= 10) {
      GURL blob_url = ReadGURL(obj);
      http_body.appendBlob(blob_url);
    }
  }
  if (obj->version >= 4)
    http_body.setIdentifier(ReadInteger64(obj));

  if (obj->version >= 12)
    http_body.setContainsPasswordData(ReadBoolean(obj));

  return http_body;
}

// Writes the HistoryItem data into the SerializeObject object for
// serialization.
void WriteHistoryItem(
    const WebHistoryItem& item, SerializeObject* obj) {
  // WARNING: This data may be persisted for later use. As such, care must be
  // taken when changing the serialized format. If a new field needs to be
  // written, only adding at the end will make it easier to deal with loading
  // older versions. Similarly, this should NOT save fields with sensitive
  // data, such as password fields.
  WriteInteger(kVersion, obj);
  WriteString(item.urlString(), obj);
  WriteString(item.originalURLString(), obj);
  WriteString(item.target(), obj);
  WriteString(item.parent(), obj);
  WriteString(item.title(), obj);
  WriteString(item.alternateTitle(), obj);
  WriteReal(item.lastVisitedTime(), obj);
  WriteInteger(item.scrollOffset().x, obj);
  WriteInteger(item.scrollOffset().y, obj);
  WriteBoolean(item.isTargetItem(), obj);
  WriteInteger(item.visitCount(), obj);
  WriteString(item.referrer(), obj);

  WriteStringVector(item.documentState(), obj);

  if (kVersion >= 11)
    WriteReal(item.pageScaleFactor(), obj);
  if (kVersion >= 9)
    WriteInteger64(item.itemSequenceNumber(), obj);
  if (kVersion >= 6)
    WriteInteger64(item.documentSequenceNumber(), obj);
  if (kVersion >= 7) {
    bool has_state_object = !item.stateObject().isNull();
    WriteBoolean(has_state_object, obj);
    if (has_state_object)
      WriteString(item.stateObject().toString(), obj);
  }

  // Yes, the referrer is written twice.  This is for backwards
  // compatibility with the format.
  WriteFormData(item.httpBody(), obj);
  WriteString(item.httpContentType(), obj);
  WriteString(item.referrer(), obj);

  // Subitems
  const WebVector<WebHistoryItem>& children = item.children();
  WriteInteger(static_cast<int>(children.size()), obj);
  for (size_t i = 0, c = children.size(); i < c; ++i)
    WriteHistoryItem(children[i], obj);
}

// Creates a new HistoryItem tree based on the serialized string.
// Assumes the data is in the format returned by WriteHistoryItem.
WebHistoryItem ReadHistoryItem(
    const SerializeObject* obj,
    IncludeFormData include_form_data,
    bool include_scroll_offset) {
  // See note in WriteHistoryItem. on this.
  obj->version = ReadInteger(obj);

  if (obj->version == -1) {
    GURL url = ReadGURL(obj);
    WebHistoryItem item;
    item.initialize();
    item.setURLString(WebString::fromUTF8(url.possibly_invalid_spec()));
    return item;
  }

  if (obj->version > kVersion || obj->version < 1)
    return WebHistoryItem();

  WebHistoryItem item;
  item.initialize();

  item.setURLString(ReadString(obj));
  item.setOriginalURLString(ReadString(obj));
  item.setTarget(ReadString(obj));
  item.setParent(ReadString(obj));
  item.setTitle(ReadString(obj));
  item.setAlternateTitle(ReadString(obj));
  item.setLastVisitedTime(ReadReal(obj));

  int x = ReadInteger(obj);
  int y = ReadInteger(obj);
  if (include_scroll_offset)
    item.setScrollOffset(WebPoint(x, y));

  item.setIsTargetItem(ReadBoolean(obj));
  item.setVisitCount(ReadInteger(obj));
  item.setReferrer(ReadString(obj));

  item.setDocumentState(ReadStringVector(obj));

  if (obj->version >= 11)
    item.setPageScaleFactor(ReadReal(obj));
  if (obj->version >= 9)
    item.setItemSequenceNumber(ReadInteger64(obj));
  if (obj->version >= 6)
    item.setDocumentSequenceNumber(ReadInteger64(obj));
  if (obj->version >= 7) {
    bool has_state_object = ReadBoolean(obj);
    if (has_state_object) {
      item.setStateObject(
          WebSerializedScriptValue::fromString(ReadString(obj)));
    }
  }

  // The extra referrer string is read for backwards compat.
  const WebHTTPBody& http_body = ReadFormData(obj);
  const WebString& http_content_type = ReadString(obj);
  ALLOW_UNUSED const WebString& unused_referrer = ReadString(obj);
  if (include_form_data == ALWAYS_INCLUDE_FORM_DATA ||
      (include_form_data == INCLUDE_FORM_DATA_WITHOUT_PASSWORDS &&
       !http_body.isNull() && !http_body.containsPasswordData())) {
    // Include the full HTTP body.
    item.setHTTPBody(http_body);
    item.setHTTPContentType(http_content_type);
  } else if (!http_body.isNull()) {
    // Don't include the data in the HTTP body, but include its identifier. This
    // enables fetching data from the cache.
    WebHTTPBody empty_http_body;
    empty_http_body.initialize();
    empty_http_body.setIdentifier(http_body.identifier());
    item.setHTTPBody(empty_http_body);
  }

#if defined(OS_ANDROID)
  // Now-unused values that shipped in this version of Chrome for Android when
  // it was on a private branch.
  if (obj->version == 11) {
    ReadReal(obj);
    ReadBoolean(obj);
  }
#endif

  // Subitems
  int num_children = ReadInteger(obj);
  for (int i = 0; i < num_children; ++i)
    item.appendToChildren(ReadHistoryItem(obj,
                                          include_form_data,
                                          include_scroll_offset));

  return item;
}

// Reconstruct a HistoryItem from a string, using our JSON Value deserializer.
// This assumes that the given serialized string has all the required key,value
// pairs, and does minimal error checking. The form data of the post is restored
// if |include_form_data| is |ALWAYS_INCLUDE_FORM_DATA| or if the data doesn't
// contain passwords and |include_form_data| is
// |INCLUDE_FORM_DATA_WITHOUT_PASSWORDS|. Otherwise the form data is empty. If
// |include_scroll_offset| is true, the scroll offset is restored.
WebHistoryItem HistoryItemFromString(
    const std::string& serialized_item,
    IncludeFormData include_form_data,
    bool include_scroll_offset) {
  if (serialized_item.empty())
    return WebHistoryItem();

  SerializeObject obj(serialized_item.data(),
                      static_cast<int>(serialized_item.length()));
  return ReadHistoryItem(&obj, include_form_data, include_scroll_offset);
}

}  // namespace

// Serialize a HistoryItem to a string, using our JSON Value serializer.
std::string HistoryItemToString(const WebHistoryItem& item) {
  if (item.isNull())
    return std::string();

  SerializeObject obj;
  WriteHistoryItem(item, &obj);
  return obj.GetAsString();
}

WebHistoryItem HistoryItemFromString(const std::string& serialized_item) {
  return HistoryItemFromString(serialized_item, ALWAYS_INCLUDE_FORM_DATA, true);
}

std::vector<FilePath> FilePathsFromHistoryState(
    const std::string& content_state) {
  std::vector<FilePath> to_return;
  // TODO(darin): We should avoid using the WebKit API here, so that we do not
  // need to have WebKit initialized before calling this method.
  const WebHistoryItem& item =
      HistoryItemFromString(content_state, ALWAYS_INCLUDE_FORM_DATA, true);
  if (item.isNull()) {
    // Couldn't parse the string.
    return to_return;
  }
  const WebVector<WebString> file_paths = item.getReferencedFilePaths();
  for (size_t i = 0; i < file_paths.size(); ++i)
    to_return.push_back(webkit_base::WebStringToFilePath(file_paths[i]));
  return to_return;
}

// For testing purposes only.
void HistoryItemToVersionedString(const WebHistoryItem& item, int version,
                                  std::string* serialized_item) {
  if (item.isNull()) {
    serialized_item->clear();
    return;
  }

  // Temporarily change the version.
  int real_version = kVersion;
  kVersion = version;

  SerializeObject obj;
  WriteHistoryItem(item, &obj);
  *serialized_item = obj.GetAsString();

  kVersion = real_version;
}

int HistoryItemCurrentVersion() {
  return kVersion;
}

std::string RemoveFormDataFromHistoryState(const std::string& content_state) {
  // TODO(darin): We should avoid using the WebKit API here, so that we do not
  // need to have WebKit initialized before calling this method.
  const WebHistoryItem& item =
      HistoryItemFromString(content_state, NEVER_INCLUDE_FORM_DATA, true);
  if (item.isNull()) {
    // Couldn't parse the string, return an empty string.
    return std::string();
  }

  return HistoryItemToString(item);
}

std::string RemovePasswordDataFromHistoryState(
    const std::string& content_state) {
  // TODO(darin): We should avoid using the WebKit API here, so that we do not
  // need to have WebKit initialized before calling this method.
  const WebHistoryItem& item =
      HistoryItemFromString(
          content_state, INCLUDE_FORM_DATA_WITHOUT_PASSWORDS, true);
  if (item.isNull()) {
    // Couldn't parse the string, return an empty string.
    return std::string();
  }

  return HistoryItemToString(item);
}

std::string RemoveScrollOffsetFromHistoryState(
    const std::string& content_state) {
  // TODO(darin): We should avoid using the WebKit API here, so that we do not
  // need to have WebKit initialized before calling this method.
  const WebHistoryItem& item =
      HistoryItemFromString(content_state, ALWAYS_INCLUDE_FORM_DATA, false);
  if (item.isNull()) {
    // Couldn't parse the string, return an empty string.
    return std::string();
  }

  return HistoryItemToString(item);
}

std::string CreateHistoryStateForURL(const GURL& url) {
  // We avoid using the WebKit API here, so that we do not need to have WebKit
  // initialized before calling this method.  Instead, we write a simple
  // serialization of the given URL with a dummy version number of -1.  This
  // will be interpreted by ReadHistoryItem as a request to create a default
  // WebHistoryItem.
  SerializeObject obj;
  WriteInteger(-1, &obj);
  WriteGURL(url, &obj);
  return obj.GetAsString();
}

}  // namespace webkit_glue
