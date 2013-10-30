// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace ui {

// Set up the compositor ContextFactory for a test environment. Unit tests
// that do not have a full content environment need to call this before
// initializing the Compositor.
// Some tests expect pixel output, and they should pass false for
// |allow_test_contexts|. Most unit tests should pass true. Once this has been
// called, the caller must call TerminateContextFactoryForTests() to clean up.
void InitializeContextFactoryForTests(bool allow_test_contexts);
void TerminateContextFactoryForTests();

}  // namespace ui
