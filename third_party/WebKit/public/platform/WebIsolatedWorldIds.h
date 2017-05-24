// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebIsolatedWorldIds_h
#define WebIsolatedWorldIds_h

namespace blink {

enum IsolatedWorldId {
  // Embedder isolated worlds can use IDs in [1, 1<<29).
  kEmbedderWorldIdLimit = (1 << 29),
  kDocumentXMLTreeViewerWorldId,
  kDevToolsFirstIsolatedWorldId,
  kDevToolsLastIsolatedWorldId = kDevToolsFirstIsolatedWorldId + 100,
  kIsolatedWorldIdLimit,
};

}  // namespace blink

#endif  // WebIsolatedWorldIds_h
