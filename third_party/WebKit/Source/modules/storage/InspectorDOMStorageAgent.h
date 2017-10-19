/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorDOMStorageAgent_h
#define InspectorDOMStorageAgent_h

#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/DOMStorage.h"
#include "modules/ModulesExport.h"
#include "modules/storage/StorageArea.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class LocalFrame;
class Page;
class StorageArea;

class MODULES_EXPORT InspectorDOMStorageAgent final
    : public InspectorBaseAgent<protocol::DOMStorage::Metainfo> {
 public:
  static InspectorDOMStorageAgent* Create(Page* page) {
    return new InspectorDOMStorageAgent(page);
  }

  ~InspectorDOMStorageAgent() override;
  virtual void Trace(blink::Visitor*);

  void DidDispatchDOMStorageEvent(const String& key,
                                  const String& old_value,
                                  const String& new_value,
                                  StorageType,
                                  SecurityOrigin*);

 private:
  explicit InspectorDOMStorageAgent(Page*);

  // InspectorBaseAgent overrides.
  void Restore() override;

  // protocol::Dispatcher::DOMStorageCommandHandler overrides.
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response clear(
      std::unique_ptr<protocol::DOMStorage::StorageId>) override;

  protocol::Response getDOMStorageItems(
      std::unique_ptr<protocol::DOMStorage::StorageId>,
      std::unique_ptr<protocol::Array<protocol::Array<String>>>* entries)
      override;
  protocol::Response setDOMStorageItem(
      std::unique_ptr<protocol::DOMStorage::StorageId>,
      const String& key,
      const String& value) override;
  protocol::Response removeDOMStorageItem(
      std::unique_ptr<protocol::DOMStorage::StorageId>,
      const String& key) override;

  protocol::Response FindStorageArea(
      std::unique_ptr<protocol::DOMStorage::StorageId>,
      LocalFrame*&,
      StorageArea*&);
  std::unique_ptr<protocol::DOMStorage::StorageId> GetStorageId(
      SecurityOrigin*,
      bool is_local_storage);

  Member<Page> page_;
  bool is_enabled_;
};

}  // namespace blink

#endif  // !defined(InspectorDOMStorageAgent_h)
