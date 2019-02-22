/*
 * Copyright (C) 2010, 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_FILE_CHOOSER_PARAMS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_FILE_CHOOSER_PARAMS_H_

#include "third_party/blink/public/mojom/choosers/file_chooser.mojom-shared.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

struct WebFileChooserParams {
  // See FileChooserParams in file_chooser.mojom.
  using Mode = mojom::FileChooserParams_Mode;
  Mode mode = Mode::kOpen;
  // |title| is the title for a file chooser dialog. It can be an empty string.
  WebString title;
  // This contains MIME type strings such as "audio/*" "text/plain" or file
  // extensions beginning with a period (.) such as ".mp3" ".txt".
  // The dialog may restrict selectable files to files with the specified MIME
  // types or file extensions.
  // This list comes from an 'accept' attribute value of an INPUT element, and
  // it contains only lower-cased MIME type strings and file extensions.
  WebVector<WebString> accept_types;
  // |selected_files| has filenames which a file upload control already
  // selected. A WebLocalFrameClient implementation may ask a user to select
  //  - removing a file from the selected files,
  //  - appending other files, or
  //  - replacing with other files
  // before opening a file chooser dialog.
  WebVector<WebString> selected_files;
  // See http://www.w3.org/TR/html-media-capture/ for the semantics of the
  // capture attribute. If |use_media_capture| is true, the media types
  // indicated in |accept_types| should be obtained from the device's
  // environment using a media capture mechanism.
  bool use_media_capture = false;
  // Whether WebFileChooserCompletion needs local paths or not. If the result
  // of file chooser is handled by the implementation of
  // WebFileChooserCompletion that can handle files without local paths,
  // 'false' should be specified to the flag.
  bool need_local_path = true;
  // If non-empty, represents the URL of the requestor if the request was
  // initiated by a document.
  WebURL requestor;
};

}  // namespace blink

#endif
