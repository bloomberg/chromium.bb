// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_APP_ID_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_APP_ID_H_

namespace file_manager {

// The file manager's app ID.
//
// Note that file_manager::kFileManagerAppId is a bit redundant but a shorter
// name like kAppId would be cryptic inside "file_manager" namespace.
const char kFileManagerAppId[] = "hhaomjibdihmijegdhdafkllkbggdgoj";

// The video player's app ID.
const char kVideoPlayerAppId[] = "jcgeabjmjgoblfofpppfkcoakmfobdko";

// The gallery's app ID.
const char kGalleryAppId[] = "nlkncpkkdoccmpiclbokaimcnedabhhm";

// The audio player's app ID.
const char kAudioPlayerAppId[] = "cjbfomnbifhcdnihkgipgfcihmgjfhbf";

// The text editor's app ID.
const char kTextEditorAppId[] = "mmfbcljfglbokpmkimbfghdkjmjhdgbg";

// The image loader extension's ID.
const char kImageLoaderExtensionId[] = "pmfjbimdmchhbnneeidfognadeopoehp";

// Zip Archiver extension's ID.
const char kZipArchiverId[] = "dmboannefpncccogfdikhmhpmdnddgoe";

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_APP_ID_H_
