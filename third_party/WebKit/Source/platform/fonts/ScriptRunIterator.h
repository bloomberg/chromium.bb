// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptRunIterator_h
#define ScriptRunIterator_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/dtoa/utils.h"

#include <unicode/uchar.h>
#include <unicode/uscript.h>

namespace blink {

class ScriptData;

class PLATFORM_EXPORT ScriptRunIterator {
  USING_FAST_MALLOC(ScriptRunIterator);
  WTF_MAKE_NONCOPYABLE(ScriptRunIterator);

 public:
  ScriptRunIterator(const UChar* text, size_t length);

  // This maintains a reference to data. It must exist for the lifetime of
  // this object. Typically data is a singleton that exists for the life of
  // the process.
  ScriptRunIterator(const UChar* text, size_t length, const ScriptData*);

  bool Consume(unsigned& limit, UScriptCode&);

 private:
  struct BracketRec {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    UChar32 ch;
    UScriptCode script;
  };
  void OpenBracket(UChar32);
  void CloseBracket(UChar32);
  bool MergeSets();
  void FixupStack(UScriptCode resolved_script);
  bool Fetch(size_t* pos, UChar32*);

  UScriptCode ResolveCurrentScript() const;

  const UChar* text_;
  const size_t length_;

  Deque<BracketRec> brackets_;
  size_t brackets_fixup_depth_;
  // Limit max brackets so that the bracket tracking buffer does not grow
  // excessively large when processing long runs of text.
  static const int kMaxBrackets = 32;

  Vector<UScriptCode> current_set_;
  Vector<UScriptCode> next_set_;
  Vector<UScriptCode> ahead_set_;

  UChar32 ahead_character_;
  size_t ahead_pos_;

  UScriptCode common_preferred_;

  const ScriptData* script_data_;
};

// ScriptData is a wrapper which returns a set of scripts for a particular
// character retrieved from the character's primary script and script
// extensions, as per ICU / Unicode data. ScriptData maintains a certain
// priority order of the returned values, which are essential for mergeSets
// method to work correctly.
class PLATFORM_EXPORT ScriptData {
  USING_FAST_MALLOC(ScriptData);
  WTF_MAKE_NONCOPYABLE(ScriptData);

 protected:
  ScriptData() = default;

 public:
  virtual ~ScriptData();

  enum PairedBracketType {
    kBracketTypeNone,
    kBracketTypeOpen,
    kBracketTypeClose,
    kBracketTypeCount
  };

  static const int kMaxScriptCount;

  virtual void GetScripts(UChar32, Vector<UScriptCode>& dst) const = 0;

  virtual UChar32 GetPairedBracket(UChar32) const = 0;

  virtual PairedBracketType GetPairedBracketType(UChar32) const = 0;
};

class PLATFORM_EXPORT ICUScriptData : public ScriptData {
 public:
  ~ICUScriptData() override = default;

  static const ICUScriptData* Instance();

  void GetScripts(UChar32, Vector<UScriptCode>& dst) const override;

  UChar32 GetPairedBracket(UChar32) const override;

  PairedBracketType GetPairedBracketType(UChar32) const override;
};
}  // namespace blink

#endif
