// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/persistence/remoting_keychain.h"

#import <Security/Security.h>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/no_destructor.h"

namespace remoting {

namespace {

using ScopedMutableDictionary = base::ScopedCFTypeRef<CFMutableDictionaryRef>;

const char kServicePrefix[] = "com.google.ChromeRemoteDesktop.";

CFStringRef WrapStdString(const std::string& str) {
  // Note that kCFAllocatorDefault as allocator will not do anything, but
  // kCFAllocatorDefault as deallocator will release the buffer, which leads to
  // double-free because it's owned by the std::string.
  return CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, str.c_str(),
                                         kCFStringEncodingUTF8,
                                         kCFAllocatorNull);
}

CFDataRef WrapStringToData(const std::string& data) {
  const UInt8* data_pointer = reinterpret_cast<const UInt8*>(data.data());
  return CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, data_pointer,
                                     data.size(), kCFAllocatorNull);
}

ScopedMutableDictionary CreateScopedMutableDictionary() {
  return ScopedMutableDictionary(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
}

ScopedMutableDictionary CreateQueryForUpdate(const std::string& service,
                                             const std::string& account) {
  ScopedMutableDictionary dictionary = CreateScopedMutableDictionary();
  CFDictionarySetValue(dictionary.get(), kSecClass, kSecClassGenericPassword);
  CFDictionarySetValue(dictionary.get(), kSecAttrService,
                       WrapStdString(service));
  CFDictionarySetValue(dictionary.get(), kSecAttrAccount,
                       WrapStdString(account));

  return dictionary;
}

ScopedMutableDictionary CreateQueryForLookup(const std::string& service,
                                             const std::string& account) {
  ScopedMutableDictionary dictionary = CreateQueryForUpdate(service, account);
  CFDictionarySetValue(dictionary.get(), kSecMatchLimit, kSecMatchLimitOne);
  CFDictionarySetValue(dictionary.get(), kSecReturnData, kCFBooleanTrue);
  return dictionary;
}

ScopedMutableDictionary CreateDictionaryForInsertion(const std::string& service,
                                                     const std::string& account,
                                                     const std::string& data) {
  ScopedMutableDictionary dictionary = CreateQueryForUpdate(service, account);
  CFDictionarySetValue(dictionary.get(), kSecValueData, WrapStringToData(data));
  return dictionary;
}

}  // namespace

RemotingKeychain::RemotingKeychain() : service_prefix_(kServicePrefix) {}

RemotingKeychain::~RemotingKeychain() {}

// static
RemotingKeychain* RemotingKeychain::GetInstance() {
  static base::NoDestructor<RemotingKeychain> instance;
  return instance.get();
}

void RemotingKeychain::SetData(Key key,
                               const std::string& account,
                               const std::string& data) {
  DCHECK(data.size());

  std::string service = KeyToService(key);

  std::string existing_data = GetData(key, account);
  if (!existing_data.empty()) {
    // Item already exists. Update it.

    ScopedMutableDictionary update_query =
        CreateQueryForUpdate(service, account);

    ScopedMutableDictionary updated_attributes =
        CreateScopedMutableDictionary();
    CFDictionarySetValue(updated_attributes.get(), kSecValueData,
                         WrapStringToData(data));
    OSStatus status = SecItemUpdate(update_query, updated_attributes);
    if (status != errSecSuccess) {
      LOG(FATAL) << "Failed to update keychain item. Status: " << status;
    }
    return;
  }

  // Item doesn't exist. Add it.
  ScopedMutableDictionary insertion_dictionary =
      CreateDictionaryForInsertion(service, account, data);
  OSStatus status = SecItemAdd(insertion_dictionary.get(), NULL);
  if (status != errSecSuccess) {
    LOG(FATAL) << "Failed to add new keychain item. Status: " << status;
  }
}

std::string RemotingKeychain::GetData(Key key,
                                      const std::string& account) const {
  std::string service = KeyToService(key);

  ScopedMutableDictionary query = CreateQueryForLookup(service, account);
  base::ScopedCFTypeRef<CFDataRef> cf_result;
  OSStatus status =
      SecItemCopyMatching(query, (CFTypeRef*)cf_result.InitializeInto());
  if (status == errSecItemNotFound) {
    return "";
  }
  if (status != errSecSuccess) {
    LOG(FATAL) << "Failed to query keychain data. Status: " << status;
    return "";
  }
  const char* data_pointer =
      reinterpret_cast<const char*>(CFDataGetBytePtr(cf_result.get()));
  CFIndex data_size = CFDataGetLength(cf_result.get());
  return std::string(data_pointer, data_size);
}

void RemotingKeychain::RemoveData(Key key, const std::string& account) {
  std::string service = KeyToService(key);

  ScopedMutableDictionary query = CreateQueryForUpdate(service, account);
  OSStatus status = SecItemDelete(query);
  if (status != errSecSuccess && status != errSecItemNotFound) {
    LOG(FATAL) << "Failed to delete a keychain item. Status: " << status;
  }
}

void RemotingKeychain::SetServicePrefixForTesting(
    const std::string& service_prefix) {
  DCHECK(service_prefix.length());
  service_prefix_ = service_prefix;
}

std::string RemotingKeychain::KeyToService(Key key) const {
  return service_prefix_ + KeyToString(key);
}

}  // namespace remoting
