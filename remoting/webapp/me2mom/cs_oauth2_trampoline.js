// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.location.replace(
    chrome.extension.getURL('oauth2_callback.html') + window.location.search);
