// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace ui {

class ContextFactory;

// Set up the compositor ContextFactory for a test environment. Unit tests
// that do not have a full content environment need to call this before
// initializing the Compositor.
// Some tests expect pixel output, and they should pass true for
// |enable_pixel_output|. Most unit tests should pass false. Once this has been
// called, the caller must call TerminateContextFactoryForTests() to clean up.
// TODO(sky): this should return a scoped_ptr and then nuke
// TerminateContextFactoryForTests().
ui::ContextFactory* InitializeContextFactoryForTests(bool enable_pixel_output);
void TerminateContextFactoryForTests();

}  // namespace ui
