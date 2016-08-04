// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.robolectric.template;

import java.nio.file.Path;

/**
 *  Simple container class for path info about template files.
 */
public class TemplateFileInfo {

    private Path mOutputFile;
    private Path mTemplateFile;

    public TemplateFileInfo(Path templateFile, Path outputFile) {
        mOutputFile = outputFile;
        mTemplateFile = templateFile;
    }

    public Path getTemplateFile() {
        return mTemplateFile;
    }

    public Path getOutputFile() {
        return mOutputFile;
    }
}