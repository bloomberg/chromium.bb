// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const subpageImport = document.createElement('link');
subpageImport.rel = 'import';
const params = new URLSearchParams(window.location.search.substring(1));
subpageImport.href = params.get('file');
document.head.appendChild(subpageImport);
