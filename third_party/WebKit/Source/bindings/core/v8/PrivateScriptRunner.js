// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

var installedClasses = {};

function installClass(className, implementation) {
    installedClasses[className] = implementation(window);
}

// A parenthesis is needed, because the caller of this script (PrivateScriptRunner.cpp)
// is depending on the completion value of this script.
(installedClasses);
