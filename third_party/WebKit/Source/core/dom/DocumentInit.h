/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef DocumentInit_h
#define DocumentInit_h

#include "core/CoreExport.h"
#include "core/dom/SandboxFlags.h"
#include "core/dom/SecurityContext.h"
#include "core/html/custom/V0CustomElementRegistrationContext.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebInsecureRequestPolicy.h"

namespace blink {

class Document;
class LocalFrame;
class HTMLImportsController;
class Settings;

class CORE_EXPORT DocumentInit final {
  STACK_ALLOCATED();

 public:
  DocumentInit(const KURL& = KURL(),
               LocalFrame* = nullptr,
               Document* context_document = nullptr,
               HTMLImportsController* = nullptr);
  DocumentInit(Document* owner_document,
               const KURL&,
               LocalFrame*,
               Document* context_document = nullptr,
               HTMLImportsController* = nullptr);
  DocumentInit(const DocumentInit&);
  ~DocumentInit();

  const KURL& Url() const { return url_; }
  LocalFrame* GetFrame() const { return frame_; }
  HTMLImportsController* ImportsController() const {
    return imports_controller_;
  }

  bool HasSecurityContext() const { return FrameForSecurityContext(); }
  bool ShouldTreatURLAsSrcdocDocument() const;
  bool ShouldSetURL() const;
  bool IsSeamlessAllowedFor(Document* child) const;
  bool ShouldReuseDefaultView() const { return should_reuse_default_view_; }
  SandboxFlags GetSandboxFlags() const;
  bool IsHostedInReservedIPRange() const;
  WebInsecureRequestPolicy GetInsecureRequestPolicy() const;
  SecurityContext::InsecureNavigationsSet* InsecureNavigationsToUpgrade() const;

  Document* Parent() const { return parent_.Get(); }
  Document* Owner() const { return owner_.Get(); }
  KURL ParentBaseURL() const;
  LocalFrame* OwnerFrame() const;
  Settings* GetSettings() const;

  DocumentInit& WithRegistrationContext(V0CustomElementRegistrationContext*);
  DocumentInit& WithNewRegistrationContext();
  V0CustomElementRegistrationContext* RegistrationContext(Document*) const;
  Document* ContextDocument() const;

  static DocumentInit FromContext(Document* context_document,
                                  const KURL& = KURL());

 private:
  LocalFrame* FrameForSecurityContext() const;

  KURL url_;
  Member<LocalFrame> frame_;
  Member<Document> parent_;
  Member<Document> owner_;
  Member<Document> context_document_;
  Member<HTMLImportsController> imports_controller_;
  Member<V0CustomElementRegistrationContext> registration_context_;
  bool create_new_registration_context_;

  // In some rare cases, we'll re-use a LocalDOMWindow for a new Document. For
  // example, when a script calls window.open("..."), the browser gives
  // JavaScript a window synchronously but kicks off the load in the window
  // asynchronously. Web sites expect that modifications that they make to the
  // window object synchronously won't be blown away when the network load
  // commits. To make that happen, we "securely transition" the existing
  // LocalDOMWindow to the Document that results from the network load. See also
  // SecurityContext::isSecureTransitionTo.
  // FIXME: This is for DocumentWriter creation, not for one of Document.
  bool should_reuse_default_view_;
};

}  // namespace blink

#endif  // DocumentInit_h
