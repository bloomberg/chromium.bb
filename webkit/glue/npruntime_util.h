// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_NPRUNTIME_UTIL_H_
#define WEBKIT_GLUE_NPRUNTIME_UTIL_H_

#include "third_party/npapi/bindings/npruntime.h"
#include "webkit/glue/webkit_glue_export.h"

class Pickle;

namespace webkit_glue {

// Efficiently serialize/deserialize a NPIdentifier
WEBKIT_GLUE_EXPORT bool SerializeNPIdentifier(NPIdentifier identifier,
                                              Pickle* pickle);
WEBKIT_GLUE_EXPORT bool DeserializeNPIdentifier(const Pickle& pickle,
                                                void** pickle_iter,
                                                NPIdentifier* identifier);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_NPRUNTIME_UTIL_H_
