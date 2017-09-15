/*
 * Copyright (C) 2004, 2006, 2008, 2011 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef EncodedFormData_h
#define EncodedFormData_h

#include "platform/blob/BlobData.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class BlobDataHandle;

class PLATFORM_EXPORT FormDataElement final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  FormDataElement() : type_(kData) {}
  explicit FormDataElement(const Vector<char>& array)
      : type_(kData), data_(array) {}

  FormDataElement(const String& filename,
                  long long file_start,
                  long long file_length,
                  double expected_file_modification_time)
      : type_(kEncodedFile),
        filename_(filename),
        file_start_(file_start),
        file_length_(file_length),
        expected_file_modification_time_(expected_file_modification_time) {}
  explicit FormDataElement(const String& blob_uuid,
                           RefPtr<BlobDataHandle> optional_handle)
      : type_(kEncodedBlob),
        blob_uuid_(blob_uuid),
        optional_blob_data_handle_(std::move(optional_handle)) {}
  FormDataElement(const KURL& file_system_url,
                  long long start,
                  long long length,
                  double expected_file_modification_time)
      : type_(kEncodedFileSystemURL),
        file_system_url_(file_system_url),
        file_start_(start),
        file_length_(length),
        expected_file_modification_time_(expected_file_modification_time) {}

  bool IsSafeToSendToAnotherThread() const;

  enum Type { kData, kEncodedFile, kEncodedBlob, kEncodedFileSystemURL } type_;
  Vector<char> data_;
  String filename_;
  String blob_uuid_;
  RefPtr<BlobDataHandle> optional_blob_data_handle_;
  KURL file_system_url_;
  long long file_start_;
  long long file_length_;
  double expected_file_modification_time_;
};

inline bool operator==(const FormDataElement& a, const FormDataElement& b) {
  if (&a == &b)
    return true;

  if (a.type_ != b.type_)
    return false;
  if (a.type_ == FormDataElement::kData)
    return a.data_ == b.data_;
  if (a.type_ == FormDataElement::kEncodedFile)
    return a.filename_ == b.filename_ && a.file_start_ == b.file_start_ &&
           a.file_length_ == b.file_length_ &&
           a.expected_file_modification_time_ ==
               b.expected_file_modification_time_;
  if (a.type_ == FormDataElement::kEncodedBlob)
    return a.blob_uuid_ == b.blob_uuid_;
  if (a.type_ == FormDataElement::kEncodedFileSystemURL)
    return a.file_system_url_ == b.file_system_url_;

  return true;
}

inline bool operator!=(const FormDataElement& a, const FormDataElement& b) {
  return !(a == b);
}

class PLATFORM_EXPORT EncodedFormData : public RefCounted<EncodedFormData> {
 public:
  enum EncodingType {
    kFormURLEncoded,    // for application/x-www-form-urlencoded
    kTextPlain,         // for text/plain
    kMultipartFormData  // for multipart/form-data
  };

  static RefPtr<EncodedFormData> Create();
  static RefPtr<EncodedFormData> Create(const void*, size_t);
  static RefPtr<EncodedFormData> Create(const CString&);
  static RefPtr<EncodedFormData> Create(const Vector<char>&);
  RefPtr<EncodedFormData> Copy() const;
  RefPtr<EncodedFormData> DeepCopy() const;
  ~EncodedFormData();

  void AppendData(const void* data, size_t);
  void AppendFile(const String& file_path);
  void AppendFileRange(const String& filename,
                       long long start,
                       long long length,
                       double expected_modification_time);
  void AppendBlob(const String& blob_uuid,
                  RefPtr<BlobDataHandle> optional_handle);
  void AppendFileSystemURL(const KURL&);
  void AppendFileSystemURLRange(const KURL&,
                                long long start,
                                long long length,
                                double expected_modification_time);

  void Flatten(Vector<char>&) const;  // omits files
  String FlattenToString() const;     // omits files

  bool IsEmpty() const { return elements_.IsEmpty(); }
  const Vector<FormDataElement>& Elements() const { return elements_; }

  const Vector<char>& Boundary() const { return boundary_; }
  void SetBoundary(Vector<char> boundary) { boundary_ = boundary; }

  // Identifies a particular form submission instance.  A value of 0 is used
  // to indicate an unspecified identifier.
  void SetIdentifier(int64_t identifier) { identifier_ = identifier; }
  int64_t Identifier() const { return identifier_; }

  bool ContainsPasswordData() const { return contains_password_data_; }
  void SetContainsPasswordData(bool contains_password_data) {
    contains_password_data_ = contains_password_data;
  }

  static EncodingType ParseEncodingType(const String& type) {
    if (DeprecatedEqualIgnoringCase(type, "text/plain"))
      return kTextPlain;
    if (DeprecatedEqualIgnoringCase(type, "multipart/form-data"))
      return kMultipartFormData;
    return kFormURLEncoded;
  }

  // Size of the elements making up the EncodedFormData.
  unsigned long long SizeInBytes() const;

  bool IsSafeToSendToAnotherThread() const;

 private:
  EncodedFormData();
  EncodedFormData(const EncodedFormData&);

  Vector<FormDataElement> elements_;

  int64_t identifier_;
  Vector<char> boundary_;
  bool contains_password_data_;
};

inline bool operator==(const EncodedFormData& a, const EncodedFormData& b) {
  return a.Elements() == b.Elements();
}

inline bool operator!=(const EncodedFormData& a, const EncodedFormData& b) {
  return !(a == b);
}

}  // namespace blink

#endif
