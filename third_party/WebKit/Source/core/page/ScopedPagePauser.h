/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef ScopedPagePauser_h
#define ScopedPagePauser_h

#include "core/CoreExport.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class Page;

class CORE_EXPORT ScopedPagePauser final {
  WTF_MAKE_NONCOPYABLE(ScopedPagePauser);
  USING_FAST_MALLOC(ScopedPagePauser);

 public:
  explicit ScopedPagePauser();
  ~ScopedPagePauser();

 private:
  friend class Page;

  static void SetPaused(bool);
  static bool IsActive();

  std::unique_ptr<WebScheduler::RendererPauseHandle> pause_handle_;
};

}  // namespace blink

#endif  // ScopedPagePauser_h
