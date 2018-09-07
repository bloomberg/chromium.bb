// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// loadTimeData contains localized content.  It is populated with
// FileManager strings during build.

loadTimeData.data = $DATA;

// Overwrite LoadTimeData.prototype.data setter as nop.
// Default implementation throws errors when both background and
// foreground re-set loadTimeData.data.
Object.defineProperty(LoadTimeData.prototype, 'data', {set: () => {}});
