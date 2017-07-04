/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2010 Google, Inc.
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

#ifndef DocumentParser_h
#define DocumentParser_h

#include <memory>
#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class Document;
class DocumentParserClient;
class SegmentedString;
class ScriptableDocumentParser;
class TextResourceDecoder;

class CORE_EXPORT DocumentParser
    : public GarbageCollectedFinalized<DocumentParser>,
      public TraceWrapperBase {
 public:
  virtual ~DocumentParser();
  DECLARE_VIRTUAL_TRACE();

  virtual ScriptableDocumentParser* AsScriptableDocumentParser() {
    return nullptr;
  }

  // http://www.whatwg.org/specs/web-apps/current-work/#insertion-point
  virtual bool HasInsertionPoint() { return true; }

  // insert is used by document.write.
  virtual void insert(const SegmentedString&) = 0;

  // The below functions are used by DocumentWriter (the loader).
  virtual void AppendBytes(const char* bytes, size_t length) = 0;
  virtual bool NeedsDecoder() const { return false; }
  virtual void SetDecoder(std::unique_ptr<TextResourceDecoder>);
  virtual TextResourceDecoder* Decoder();
  virtual void SetHasAppendedData() {}

  // FIXME: append() should be private, but DocumentLoader and DOMPatchSupport
  // uses it for now.
  virtual void Append(const String&) = 0;

  virtual void Finish() = 0;

  // document() will return 0 after detach() is called.
  Document* GetDocument() const {
    DCHECK(document_);
    return document_;
  }

  bool IsParsing() const { return state_ == kParsingState; }
  bool IsStopping() const { return state_ == kStoppingState; }
  bool IsStopped() const { return state_ >= kStoppedState; }
  bool IsDetached() const { return state_ == kDetachedState; }

  // prepareToStop() is used when the EOF token is encountered and parsing is to
  // be stopped normally.
  virtual void PrepareToStopParsing();

  // stopParsing() is used when a load is canceled/stopped.
  // stopParsing() is currently different from detach(), but shouldn't be.
  // It should NOT be ok to call any methods on DocumentParser after either
  // detach() or stopParsing() but right now only detach() will ASSERT.
  virtual void StopParsing();

  // Document is expected to detach the parser before releasing its ref.
  // After detach, m_document is cleared.  The parser will unwind its
  // callstacks, but not produce any more nodes.
  // It is impossible for the parser to touch the rest of WebCore after
  // detach is called.
  // Oilpan: We don't need to call detach when a Document is destructed.
  virtual void Detach();

  // Notifies the parser that the document element is available. Used by
  // HTMLDocumentParser to dispatch preloads.
  virtual void DocumentElementAvailable() {}

  void SetDocumentWasLoadedAsPartOfNavigation() {
    document_was_loaded_as_part_of_navigation_ = true;
  }
  bool DocumentWasLoadedAsPartOfNavigation() const {
    return document_was_loaded_as_part_of_navigation_;
  }

  // FIXME: The names are not very accurate :(
  virtual void SuspendScheduledTasks();
  virtual void ResumeScheduledTasks();

  void AddClient(DocumentParserClient*);
  void RemoveClient(DocumentParserClient*);

 protected:
  explicit DocumentParser(Document*);

 private:
  enum ParserState {
    kParsingState,
    kStoppingState,
    kStoppedState,
    kDetachedState
  };
  ParserState state_;
  bool document_was_loaded_as_part_of_navigation_;

  // Every DocumentParser needs a pointer back to the document.
  // m_document will be 0 after the parser is stopped.
  Member<Document> document_;

  HeapHashSet<WeakMember<DocumentParserClient>> clients_;
};

}  // namespace blink

#endif  // DocumentParser_h
