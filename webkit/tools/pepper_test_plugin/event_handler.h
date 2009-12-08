/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WEBKIT_TOOLS_PEPPER_TEST_PLUGIN_EVENT_HANDLER_H_
#define WEBKIT_TOOLS_PEPPER_TEST_PLUGIN_EVENT_HANDLER_H_

#include <string>
#include "base/basictypes.h"
#include "webkit/glue/plugins/nphostapi.h"

class EventHandler {
 public:
  explicit EventHandler(NPP npp);
  ~EventHandler();
  int handle(void* event);
  bool set_text_box(NPObject* text_box_object);

 private:
  void addText(const char* cstr);
  static char* string_duplicate(const char* cstr, size_t* len);
  static void MakeNPVariant(const char* cstr, NPVariant* var);
  std::string EventName(double timestamp, int32 type);

  NPP npp_;
  NPObject* text_box_;

  DISALLOW_COPY_AND_ASSIGN(EventHandler);
};

extern EventHandler* event_handler;

#endif  // WEBKIT_TOOLS_PEPPER_TEST_PLUGIN_EVENT_HANDLER_H_
