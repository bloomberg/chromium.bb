// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AddStringToDigestor_h
#define AddStringToDigestor_h

namespace WTF {
class String;
}

namespace blink {
class WebCryptoDigestor;
void AddStringToDigestor(WebCryptoDigestor*, const WTF::String&);
}  // namespace blink

#endif  // AddStringToDigestor_h
