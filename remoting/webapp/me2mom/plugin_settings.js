// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains all the settings that may need massaging by the build script.
// Keeping all that cenetralized here allows us to use symlinks for the other
// files making for a faster compile/run cycle when only modifying HTML/JS.

var remoting = chrome.extension.getBackgroundPage().remoting;
remoting.PLUGIN_MIMETYPE='HOST_PLUGIN_MIMETYPE';
