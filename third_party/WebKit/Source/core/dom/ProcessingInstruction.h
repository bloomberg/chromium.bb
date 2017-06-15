/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Apple Inc. All rights reserved.
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

#ifndef ProcessingInstruction_h
#define ProcessingInstruction_h

#include "core/dom/CharacterData.h"
#include "core/dom/StyleEngineContext.h"
#include "core/loader/resource/StyleSheetResource.h"
#include "core/loader/resource/StyleSheetResourceClient.h"
#include "platform/loader/fetch/ResourceOwner.h"

namespace blink {

class StyleSheet;
class EventListener;

class ProcessingInstruction final : public CharacterData,
                                    private ResourceOwner<StyleSheetResource> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(ProcessingInstruction);

 public:
  static ProcessingInstruction* Create(Document&,
                                       const String& target,
                                       const String& data);
  ~ProcessingInstruction() override;
  DECLARE_VIRTUAL_TRACE();

  const String& target() const { return target_; }
  const String& LocalHref() const { return local_href_; }
  StyleSheet* sheet() const { return sheet_.Get(); }

  bool IsCSS() const { return is_css_; }
  bool IsXSL() const { return is_xsl_; }

  void DidAttributeChanged();
  bool IsLoading() const;

  // For XSLT
  class DetachableEventListener : public GarbageCollectedMixin {
   public:
    virtual ~DetachableEventListener() {}
    virtual EventListener* ToEventListener() = 0;
    // Detach event listener from its processing instruction.
    virtual void Detach() = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() {}
  };

  void SetEventListenerForXSLT(DetachableEventListener* listener) {
    listener_for_xslt_ = listener;
  }
  EventListener* EventListenerForXSLT();
  void ClearEventListenerForXSLT();

 private:
  ProcessingInstruction(Document&, const String& target, const String& data);

  String nodeName() const override;
  NodeType getNodeType() const override;
  Node* cloneNode(bool deep, ExceptionState&) override;

  InsertionNotificationRequest InsertedInto(ContainerNode*) override;
  void RemovedFrom(ContainerNode*) override;

  bool CheckStyleSheet(String& href, String& charset);
  void Process(const String& href, const String& charset);

  void SetCSSStyleSheet(const String& href,
                        const KURL& base_url,
                        ReferrerPolicy,
                        const WTF::TextEncoding&,
                        const CSSStyleSheetResource*) override;
  void SetXSLStyleSheet(const String& href,
                        const KURL& base_url,
                        const String& sheet) override;

  bool SheetLoaded() override;

  void ParseStyleSheet(const String& sheet);
  void ClearSheet();

  String DebugName() const override { return "ProcessingInstruction"; }

  String target_;
  String local_href_;
  String title_;
  String media_;
  Member<StyleSheet> sheet_;
  StyleEngineContext style_engine_context_;
  bool loading_;
  bool alternate_;
  bool is_css_;
  bool is_xsl_;

  Member<DetachableEventListener> listener_for_xslt_;
};

DEFINE_NODE_TYPE_CASTS(ProcessingInstruction,
                       getNodeType() == Node::kProcessingInstructionNode);

inline bool IsXSLStyleSheet(const Node& node) {
  return node.getNodeType() == Node::kProcessingInstructionNode &&
         ToProcessingInstruction(node).IsXSL();
}

}  // namespace blink

#endif  // ProcessingInstruction_h
