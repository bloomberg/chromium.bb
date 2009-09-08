// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Some plugins on older Linuxes depend on libxul.so and libxpcom.so
// despite not actually requiring any symbols from them.  So we build
// a fake libxul.so and libxpcom.so and include them in our library
// path.
//
// This source file is therefore empty, and used to build those
// libraries.
//
// This may sound like a terrible hack, but after I thought of it I
// noticed that nspluginwrapper does the same thing.
