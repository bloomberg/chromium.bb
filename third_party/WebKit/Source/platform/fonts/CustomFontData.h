/*
 * Copyright (C) 2013 Google, Inc.
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
 *
 */

#ifndef CustomFontData_h
#define CustomFontData_h

#include "platform/PlatformExport.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class PLATFORM_EXPORT CustomFontData : public RefCounted<CustomFontData> {
 public:
  static RefPtr<CustomFontData> Create() {
    return AdoptRef(new CustomFontData());
  }

  virtual ~CustomFontData() {}

  virtual void BeginLoadIfNeeded() const {}
  virtual bool IsLoading() const { return false; }
  virtual bool IsLoadingFallback() const { return false; }
  virtual bool ShouldSkipDrawing() const { return false; }
  virtual void ClearFontFaceSource() {}

 protected:
  CustomFontData() {}
};

}  // namespace blink

#endif  // CustomFontData_h
