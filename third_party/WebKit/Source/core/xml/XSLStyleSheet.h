/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#ifndef XSLStyleSheet_h
#define XSLStyleSheet_h

#include <libxml/tree.h>
#include <libxslt/transform.h>
#include "core/css/StyleSheet.h"
#include "core/dom/ProcessingInstruction.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/wtf/PassRefPtr.h"

namespace blink {

class XSLImportRule;

class XSLStyleSheet final : public StyleSheet {
 public:
  static XSLStyleSheet* Create(XSLImportRule* parent_import,
                               const String& original_url,
                               const KURL& final_url) {
    DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
    return new XSLStyleSheet(parent_import, original_url, final_url);
  }
  static XSLStyleSheet* Create(ProcessingInstruction* parent_node,
                               const String& original_url,
                               const KURL& final_url) {
    DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
    return new XSLStyleSheet(parent_node, original_url, final_url, false);
  }
  static XSLStyleSheet* CreateEmbedded(ProcessingInstruction* parent_node,
                                       const KURL& final_url) {
    DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
    return new XSLStyleSheet(parent_node, final_url.GetString(), final_url,
                             true);
  }

  // Taking an arbitrary node is unsafe, because owner node pointer can become
  // stale. XSLTProcessor ensures that the stylesheet doesn't outlive its
  // parent, in part by not exposing it to JavaScript.
  static XSLStyleSheet* CreateForXSLTProcessor(Document* document,
                                               Node* stylesheet_root_node,
                                               const String& original_url,
                                               const KURL& final_url) {
    DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
    return new XSLStyleSheet(document, stylesheet_root_node, original_url,
                             final_url, false);
  }

  ~XSLStyleSheet() override;

  bool ParseString(const String&);

  void CheckLoaded();

  const KURL& FinalURL() const { return final_url_; }

  void LoadChildSheets();
  void LoadChildSheet(const String& href);

  Document* OwnerDocument();
  XSLStyleSheet* parentStyleSheet() const override {
    return parent_style_sheet_;
  }
  void SetParentStyleSheet(XSLStyleSheet*);

  xmlDocPtr GetDocument();
  xsltStylesheetPtr CompileStyleSheet();
  xmlDocPtr LocateStylesheetSubResource(xmlDocPtr parent_doc,
                                        const xmlChar* uri);

  void ClearDocuments();

  void MarkAsProcessed();
  bool Processed() const { return processed_; }

  String type() const override { return "text/xml"; }
  bool disabled() const override { return is_disabled_; }
  void setDisabled(bool b) override { is_disabled_ = b; }
  Node* ownerNode() const override { return owner_node_; }
  String href() const override { return original_url_; }
  String title() const override { return g_empty_string; }

  void ClearOwnerNode() override { owner_node_ = nullptr; }
  KURL BaseURL() const override { return final_url_; }
  bool IsLoading() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  XSLStyleSheet(Node* parent_node,
                const String& original_url,
                const KURL& final_url,
                bool embedded);
  XSLStyleSheet(Document* owner_document,
                Node* style_sheet_root_node,
                const String& original_url,
                const KURL& final_url,
                bool embedded);
  XSLStyleSheet(XSLImportRule* parent_import,
                const String& original_url,
                const KURL& final_url);

  Member<Node> owner_node_;
  String original_url_;
  KURL final_url_;
  bool is_disabled_;

  HeapVector<Member<XSLImportRule>> children_;

  bool embedded_;
  bool processed_;

  xmlDocPtr stylesheet_doc_;
  bool stylesheet_doc_taken_;
  bool compilation_failed_;

  Member<XSLStyleSheet> parent_style_sheet_;
  Member<Document> owner_document_;
};

DEFINE_TYPE_CASTS(XSLStyleSheet,
                  StyleSheet,
                  sheet,
                  !sheet->IsCSSStyleSheet(),
                  !sheet.IsCSSStyleSheet());

}  // namespace blink

#endif  // XSLStyleSheet_h
