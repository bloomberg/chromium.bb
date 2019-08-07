// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/bho/logging.h"

#include <codecvt>
#include <iostream>

std::wostream* gLogStream = &std::wcout;
std::wofstream gLogFileStream;
const std::locale gLocale(std::locale::classic(),
                          new std::codecvt_utf8_utf16<wchar_t>);

LogLevels gLogLevel = INFO;

// Must be called before the first logging call is made.
void InitLog(const std::wstring& file) {
  gLogFileStream.open(file.c_str(), std::ios_base::out | std::ios_base::trunc);
  gLogFileStream.imbue(gLocale);
  gLogStream = &gLogFileStream;
}

// Closes the log file. No more logging is possible after this call.
void CloseLog() {
  gLogFileStream.close();
}

void SetLogLevel(LogLevels level) {
  gLogLevel = level;
}
