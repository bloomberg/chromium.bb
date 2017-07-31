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

#ifndef ContextMenuController_h
#define ContextMenuController_h

#include <memory>
#include "core/CoreExport.h"
#include "core/layout/HitTestResult.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class ContextMenu;
class ContextMenuClient;
class ContextMenuItem;
class ContextMenuProvider;
class Document;
class LocalFrame;
class MouseEvent;
class Page;

class CORE_EXPORT ContextMenuController final
    : public GarbageCollectedFinalized<ContextMenuController> {
  WTF_MAKE_NONCOPYABLE(ContextMenuController);

 public:
  static ContextMenuController* Create(Page*, ContextMenuClient*);
  ~ContextMenuController();
  DECLARE_TRACE();

  ContextMenu* GetContextMenu() const { return context_menu_.get(); }
  void ClearContextMenu();

  void DocumentDetached(Document*);

  void HandleContextMenuEvent(MouseEvent*);
  void ShowContextMenuAtPoint(LocalFrame*,
                              float x,
                              float y,
                              ContextMenuProvider*);

  void ContextMenuItemSelected(const ContextMenuItem*);

  const HitTestResult& GetHitTestResult() { return hit_test_result_; }

 private:
  ContextMenuController(Page*, ContextMenuClient*);

  std::unique_ptr<ContextMenu> CreateContextMenu(MouseEvent*);
  std::unique_ptr<ContextMenu> CreateContextMenu(LocalFrame*,
                                                 const LayoutPoint&);
  void ShowContextMenu(MouseEvent*);

  ContextMenuClient* client_;
  std::unique_ptr<ContextMenu> context_menu_;
  Member<ContextMenuProvider> menu_provider_;
  HitTestResult hit_test_result_;
};

}  // namespace blink

#endif  // ContextMenuController_h
