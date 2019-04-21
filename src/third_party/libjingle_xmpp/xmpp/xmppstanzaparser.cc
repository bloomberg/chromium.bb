/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "third_party/libjingle_xmpp/xmpp/xmppstanzaparser.h"

#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"
#ifdef EXPAT_RELATIVE_PATH
#include "expat.h"
#else
#include "third_party/expat/v2_0_1/Source/lib/expat.h"
#endif

namespace jingle_xmpp {

XmppStanzaParser::XmppStanzaParser(XmppStanzaParseHandler *psph) :
  psph_(psph),
  innerHandler_(this),
  parser_(&innerHandler_),
  depth_(0),
  builder_() {
}

void
XmppStanzaParser::Reset() {
  parser_.Reset();
  depth_ = 0;
  builder_.Reset();
}

void
XmppStanzaParser::IncomingStartElement(
    XmlParseContext * pctx, const char * name, const char ** atts) {
  if (depth_++ == 0) {
    XmlElement * pelStream = XmlBuilder::BuildElement(pctx, name, atts);
    if (pelStream == NULL) {
      pctx->RaiseError(XML_ERROR_SYNTAX);
      return;
    }
    psph_->StartStream(pelStream);
    delete pelStream;
    return;
  }

  builder_.StartElement(pctx, name, atts);
}

void
XmppStanzaParser::IncomingCharacterData(
    XmlParseContext * pctx, const char * text, int len) {
  if (depth_ > 1) {
    builder_.CharacterData(pctx, text, len);
  }
}

void
XmppStanzaParser::IncomingEndElement(
    XmlParseContext * pctx, const char * name) {
  if (--depth_ == 0) {
    psph_->EndStream();
    return;
  }

  builder_.EndElement(pctx, name);

  if (depth_ == 1) {
    XmlElement *element = builder_.CreateElement();
    psph_->Stanza(element);
    delete element;
  }
}

void
XmppStanzaParser::IncomingError(
    XmlParseContext * pctx, XML_Error errCode) {
  psph_->XmlError();
}

}
