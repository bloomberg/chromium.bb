// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConsoleTypes_h
#define ConsoleTypes_h

namespace blink {

enum MessageSource {
  XMLMessageSource,
  JSMessageSource,
  NetworkMessageSource,
  ConsoleAPIMessageSource,
  StorageMessageSource,
  AppCacheMessageSource,
  RenderingMessageSource,
  SecurityMessageSource,
  OtherMessageSource,
  DeprecationMessageSource,
  WorkerMessageSource,
  ViolationMessageSource,
  InterventionMessageSource
};

enum MessageLevel {
  VerboseMessageLevel,
  InfoMessageLevel,
  WarningMessageLevel,
  ErrorMessageLevel
};
}

#endif  // !defined(ConsoleTypes_h)
