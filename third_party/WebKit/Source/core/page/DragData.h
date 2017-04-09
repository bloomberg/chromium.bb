/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DragData_h
#define DragData_h

#include "core/CoreExport.h"
#include "core/page/DragActions.h"
#include "platform/geometry/IntPoint.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"

namespace blink {

class DataObject;
class DocumentFragment;
class LocalFrame;

enum DragApplicationFlags {
  kDragApplicationNone = 0,
  kDragApplicationIsModal = 1,
  kDragApplicationIsSource = 2,
  kDragApplicationHasAttachedSheet = 4,
  kDragApplicationIsCopyKeyDown = 8
};

class CORE_EXPORT DragData {
  STACK_ALLOCATED();

 public:
  enum FilenameConversionPolicy { kDoNotConvertFilenames, kConvertFilenames };

  // clientPosition is taken to be the position of the drag event within the
  // target window, with (0,0) at the top left.
  DragData(DataObject*,
           const IntPoint& client_position,
           const IntPoint& global_position,
           DragOperation,
           DragApplicationFlags = kDragApplicationNone);
  const IntPoint& ClientPosition() const { return client_position_; }
  const IntPoint& GlobalPosition() const { return global_position_; }
  DragApplicationFlags Flags() const { return application_flags_; }
  DataObject* PlatformData() const { return platform_drag_data_; }
  DragOperation DraggingSourceOperationMask() const {
    return dragging_source_operation_mask_;
  }
  bool ContainsURL(
      FilenameConversionPolicy filename_policy = kConvertFilenames) const;
  bool ContainsPlainText() const;
  bool ContainsCompatibleContent() const;
  String AsURL(FilenameConversionPolicy filename_policy = kConvertFilenames,
               String* title = nullptr) const;
  String AsPlainText() const;
  void AsFilePaths(Vector<String>&) const;
  unsigned NumberOfFiles() const;
  DocumentFragment* AsFragment(LocalFrame*) const;
  bool CanSmartReplace() const;
  bool ContainsFiles() const;
  int GetModifiers() const;

  String DroppedFileSystemId() const;

 private:
  const IntPoint client_position_;
  const IntPoint global_position_;
  const Member<DataObject> platform_drag_data_;
  const DragOperation dragging_source_operation_mask_;
  const DragApplicationFlags application_flags_;

  bool ContainsHTML() const;
};

}  // namespace blink

#endif  // !DragData_h
