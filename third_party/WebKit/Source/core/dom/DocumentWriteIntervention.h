// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentWriteIntervention_h
#define DocumentWriteIntervention_h

#include "platform/loader/fetch/FetchParameters.h"

namespace blink {

class Document;
class ResourceRequest;

// Returns true if the fetch should be blocked due to the document.write
// intervention. In that case, the request's cache policy is set to
// kReturnCacheDataDontLoad to ensure a network request is not generated.
// This function may also set an Intervention header, log the intervention in
// the console, etc.
// This only affects scripts added via document.write() in the main frame.
bool MaybeDisallowFetchForDocWrittenScript(ResourceRequest&,
                                           FetchParameters::DeferOption,
                                           Document&);

}  // namespace blink

#endif  // DocumentWriteIntervention_h
