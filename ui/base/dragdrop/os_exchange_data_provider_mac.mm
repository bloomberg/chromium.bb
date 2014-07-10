// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/pickle.h"
#include "base/strings/sys_string_conversions.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#import "ui/base/dragdrop/cocoa_dnd_util.h"
#include "url/gurl.h"

namespace ui {

OSExchangeDataProviderMac::OSExchangeDataProviderMac()
    : pasteboard_([[NSPasteboard pasteboardWithUniqueName] retain]) {
}

OSExchangeDataProviderMac::OSExchangeDataProviderMac(NSPasteboard* pasteboard)
    : pasteboard_([pasteboard retain]) {
}

OSExchangeDataProviderMac::~OSExchangeDataProviderMac() {
}

OSExchangeData::Provider* OSExchangeDataProviderMac::Clone() const {
  return new OSExchangeDataProviderMac(pasteboard_);
}

void OSExchangeDataProviderMac::MarkOriginatedFromRenderer() {
  NOTIMPLEMENTED();
}

bool OSExchangeDataProviderMac::DidOriginateFromRenderer() const {
  NOTIMPLEMENTED();
  return false;
}

void OSExchangeDataProviderMac::SetString(const base::string16& string) {
  [pasteboard_ writeObjects:@[ base::SysUTF16ToNSString(string) ]];
}

void OSExchangeDataProviderMac::SetURL(const GURL& url,
                                       const base::string16& title) {
  [pasteboard_ setDataForURL:base::SysUTF8ToNSString(url.spec())
                       title:base::SysUTF16ToNSString(title)];
}

void OSExchangeDataProviderMac::SetFilename(const base::FilePath& path) {
  NOTIMPLEMENTED();
}

void OSExchangeDataProviderMac::SetFilenames(
    const std::vector<FileInfo>& filenames) {
  NOTIMPLEMENTED();
}

void OSExchangeDataProviderMac::SetPickledData(
    const OSExchangeData::CustomFormat& format,
    const Pickle& data) {
  NSData* ns_data = [NSData dataWithBytes:data.data() length:data.size()];
  [pasteboard_ setData:ns_data forType:format.ToNSString()];
}

bool OSExchangeDataProviderMac::GetString(base::string16* data) const {
  DCHECK(data);
  NSArray* items = [pasteboard_ readObjectsForClasses:@[ [NSString class] ]
                                              options:@{ }];
  if ([items count] == 0)
    return false;

  *data = base::SysNSStringToUTF16([items objectAtIndex:0]);
  return true;
}

bool OSExchangeDataProviderMac::GetURLAndTitle(
    OSExchangeData::FilenameToURLPolicy policy,
    GURL* url,
    base::string16* title) const {
  return PopulateURLAndTitleFromPasteboard(
      url, title, pasteboard_, policy == OSExchangeData::CONVERT_FILENAMES);
}

bool OSExchangeDataProviderMac::GetFilename(base::FilePath* path) const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeDataProviderMac::GetFilenames(
    std::vector<FileInfo>* filenames) const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeDataProviderMac::GetPickledData(
    const OSExchangeData::CustomFormat& format,
    Pickle* data) const {
  DCHECK(data);
  NSData* ns_data = [pasteboard_ dataForType:format.ToNSString()];
  if (!ns_data)
    return false;

  *data = Pickle(static_cast<const char*>([ns_data bytes]), [ns_data length]);
  return true;
}

bool OSExchangeDataProviderMac::HasString() const {
  NSArray* classes = @[ [NSString class] ];
  return [pasteboard_ canReadObjectForClasses:classes options:nil];
}

bool OSExchangeDataProviderMac::HasURL(
    OSExchangeData::FilenameToURLPolicy policy) const {
  GURL url;
  base::string16 title;
  return GetURLAndTitle(policy, &url, &title);
}

bool OSExchangeDataProviderMac::HasFile() const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeDataProviderMac::HasCustomFormat(
    const OSExchangeData::CustomFormat& format) const {
  return [[pasteboard_ types] containsObject:format.ToNSString()];
}

///////////////////////////////////////////////////////////////////////////////
// OSExchangeData, public:

// static
OSExchangeData::Provider* OSExchangeData::CreateProvider() {
  return new OSExchangeDataProviderMac;
}

}  // namespace ui
