/*
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SegmentedString_h
#define SegmentedString_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/TextPosition.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class SegmentedString;

class PLATFORM_EXPORT SegmentedSubstring {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  SegmentedSubstring()
      : length_(0), do_not_exclude_line_numbers_(true), is8_bit_(false) {
    data_.string16_ptr = 0;
  }

  SegmentedSubstring(const String& str)
      : length_(str.length()),
        do_not_exclude_line_numbers_(true),
        string_(str) {
    if (length_) {
      if (string_.Is8Bit()) {
        is8_bit_ = true;
        data_.string8_ptr = string_.Characters8();
      } else {
        is8_bit_ = false;
        data_.string16_ptr = string_.Characters16();
      }
    } else {
      is8_bit_ = false;
      data_.string8_ptr = nullptr;
    }
  }

  void Clear() {
    length_ = 0;
    data_.string16_ptr = nullptr;
    is8_bit_ = false;
  }

  bool Is8Bit() { return is8_bit_; }

  bool ExcludeLineNumbers() const { return !do_not_exclude_line_numbers_; }
  bool DoNotExcludeLineNumbers() const { return do_not_exclude_line_numbers_; }

  void SetExcludeLineNumbers() { do_not_exclude_line_numbers_ = false; }

  int NumberOfCharactersConsumed() const { return string_.length() - length_; }

  void AppendTo(StringBuilder& builder) const {
    int offset = string_.length() - length_;

    if (!offset) {
      if (length_)
        builder.Append(string_);
    } else {
      builder.Append(string_.Substring(offset, length_));
    }
  }

  bool PushIfPossible(UChar c) {
    if (!length_)
      return false;

    if (is8_bit_) {
      if (data_.string8_ptr == string_.Characters8())
        return false;

      if (*(data_.string8_ptr - 1) != c)
        return false;

      --data_.string8_ptr;
      ++length_;
      return true;
    }

    if (data_.string16_ptr == string_.Characters16())
      return false;

    if (*(data_.string16_ptr - 1) != c)
      return false;

    --data_.string16_ptr;
    ++length_;
    return true;
  }

  UChar GetCurrentChar8() { return *data_.string8_ptr; }

  UChar GetCurrentChar16() {
    return data_.string16_ptr ? *data_.string16_ptr : 0;
  }

  UChar IncrementAndGetCurrentChar8() {
    DCHECK(data_.string8_ptr);
    return *++data_.string8_ptr;
  }

  UChar IncrementAndGetCurrentChar16() {
    DCHECK(data_.string16_ptr);
    return *++data_.string16_ptr;
  }

  String CurrentSubString(unsigned length) {
    int offset = string_.length() - length_;
    return string_.Substring(offset, length);
  }

  ALWAYS_INLINE UChar GetCurrentChar() {
    DCHECK(length_);
    if (Is8Bit())
      return GetCurrentChar8();
    return GetCurrentChar16();
  }

  ALWAYS_INLINE UChar IncrementAndGetCurrentChar() {
    DCHECK(length_);
    if (Is8Bit())
      return IncrementAndGetCurrentChar8();
    return IncrementAndGetCurrentChar16();
  }

  ALWAYS_INLINE bool HaveOneCharacterLeft() const { return length_ == 1; }

  ALWAYS_INLINE void DecrementLength() { --length_; }

  ALWAYS_INLINE int length() const { return length_; }

 private:
  union {
    const LChar* string8_ptr;
    const UChar* string16_ptr;
  } data_;
  int length_;
  bool do_not_exclude_line_numbers_;
  bool is8_bit_;
  String string_;
};

class PLATFORM_EXPORT SegmentedString {
  DISALLOW_NEW();

 public:
  SegmentedString()
      : current_char_(0),
        number_of_characters_consumed_prior_to_current_string_(0),
        number_of_characters_consumed_prior_to_current_line_(0),
        current_line_(0),
        closed_(false),
        empty_(true),
        fast_path_flags_(kNoFastPath),
        advance_func_(&SegmentedString::AdvanceEmpty),
        advance_and_update_line_number_func_(&SegmentedString::AdvanceEmpty) {}

  SegmentedString(const String& str)
      : current_string_(str),
        current_char_(0),
        number_of_characters_consumed_prior_to_current_string_(0),
        number_of_characters_consumed_prior_to_current_line_(0),
        current_line_(0),
        closed_(false),
        empty_(!str.length()),
        fast_path_flags_(kNoFastPath) {
    if (current_string_.length())
      current_char_ = current_string_.GetCurrentChar();
    UpdateAdvanceFunctionPointers();
  }

  void Clear();
  void Close();

  void Append(const SegmentedString&);
  enum class PrependType {
    kNewInput = 0,
    kUnconsume = 1,
  };
  void Prepend(const SegmentedString&, PrependType);

  bool ExcludeLineNumbers() const {
    return current_string_.ExcludeLineNumbers();
  }
  void SetExcludeLineNumbers();

  void Push(UChar);

  bool IsEmpty() const { return empty_; }
  unsigned length() const;

  bool IsClosed() const { return closed_; }

  enum LookAheadResult {
    kDidNotMatch,
    kDidMatch,
    kNotEnoughCharacters,
  };

  LookAheadResult LookAhead(const String& string) {
    return LookAheadInline(string, kTextCaseSensitive);
  }
  LookAheadResult LookAheadIgnoringCase(const String& string) {
    return LookAheadInline(string, kTextCaseASCIIInsensitive);
  }

  void Advance() {
    if (fast_path_flags_ & kUse8BitAdvance) {
      current_char_ = current_string_.IncrementAndGetCurrentChar8();
      DecrementAndCheckLength();
      return;
    }

    (this->*advance_func_)();
  }

  inline void AdvanceAndUpdateLineNumber() {
    if (fast_path_flags_ & kUse8BitAdvance) {
      bool have_new_line =
          (current_char_ == '\n') &
          !!(fast_path_flags_ & kUse8BitAdvanceAndUpdateLineNumbers);
      current_char_ = current_string_.IncrementAndGetCurrentChar8();
      DecrementAndCheckLength();

      if (have_new_line) {
        ++current_line_;
        number_of_characters_consumed_prior_to_current_line_ =
            number_of_characters_consumed_prior_to_current_string_ +
            current_string_.NumberOfCharactersConsumed();
      }

      return;
    }

    (this->*advance_and_update_line_number_func_)();
  }

  void AdvanceAndASSERT(UChar expected_character) {
    DCHECK_EQ(expected_character, CurrentChar());
    Advance();
  }

  void AdvanceAndASSERTIgnoringCase(UChar expected_character) {
    DCHECK_EQ(WTF::Unicode::FoldCase(CurrentChar()),
              WTF::Unicode::FoldCase(expected_character));
    Advance();
  }

  void AdvancePastNonNewline() {
    DCHECK_NE(CurrentChar(), '\n');
    Advance();
  }

  void AdvancePastNewlineAndUpdateLineNumber() {
    DCHECK_EQ(CurrentChar(), '\n');
    if (current_string_.length() > 1) {
      int new_line_flag = current_string_.DoNotExcludeLineNumbers();
      current_line_ += new_line_flag;
      if (new_line_flag)
        number_of_characters_consumed_prior_to_current_line_ =
            NumberOfCharactersConsumed() + 1;
      DecrementAndCheckLength();
      current_char_ = current_string_.IncrementAndGetCurrentChar();
      return;
    }
    AdvanceAndUpdateLineNumberSlowCase();
  }

  // Writes the consumed characters into consumedCharacters, which must
  // have space for at least |count| characters.
  void Advance(unsigned count, UChar* consumed_characters);

  int NumberOfCharactersConsumed() const {
    int number_of_pushed_characters = 0;
    return number_of_characters_consumed_prior_to_current_string_ +
           current_string_.NumberOfCharactersConsumed() -
           number_of_pushed_characters;
  }

  String ToString() const;

  UChar CurrentChar() const { return current_char_; }

  // The method is moderately slow, comparing to currentLine method.
  OrdinalNumber CurrentColumn() const;
  OrdinalNumber CurrentLine() const;
  // Sets value of line/column variables. Column is specified indirectly by a
  // parameter columnAftreProlog which is a value of column that we should get
  // after a prolog (first prologLength characters) has been consumed.
  void SetCurrentPosition(OrdinalNumber line,
                          OrdinalNumber column_aftre_prolog,
                          int prolog_length);

 private:
  enum FastPathFlags {
    kNoFastPath = 0,
    kUse8BitAdvanceAndUpdateLineNumbers = 1 << 0,
    kUse8BitAdvance = 1 << 1,
  };

  void Append(const SegmentedSubstring&);
  void Prepend(const SegmentedSubstring&, PrependType);

  void Advance8();
  void Advance16();
  void AdvanceAndUpdateLineNumber8();
  void AdvanceAndUpdateLineNumber16();
  void AdvanceSlowCase();
  void AdvanceAndUpdateLineNumberSlowCase();
  void AdvanceEmpty();
  void AdvanceSubstring();

  void UpdateSlowCaseFunctionPointers();

  void DecrementAndCheckLength() {
    DCHECK_GT(current_string_.length(), 1);
    current_string_.DecrementLength();
    if (current_string_.HaveOneCharacterLeft())
      UpdateSlowCaseFunctionPointers();
  }

  void UpdateAdvanceFunctionPointers() {
    if (current_string_.length() > 1) {
      if (current_string_.Is8Bit()) {
        advance_func_ = &SegmentedString::Advance8;
        fast_path_flags_ = kUse8BitAdvance;
        if (current_string_.DoNotExcludeLineNumbers()) {
          advance_and_update_line_number_func_ =
              &SegmentedString::AdvanceAndUpdateLineNumber8;
          fast_path_flags_ |= kUse8BitAdvanceAndUpdateLineNumbers;
        } else {
          advance_and_update_line_number_func_ = &SegmentedString::Advance8;
        }
        return;
      }

      advance_func_ = &SegmentedString::Advance16;
      fast_path_flags_ = kNoFastPath;
      if (current_string_.DoNotExcludeLineNumbers())
        advance_and_update_line_number_func_ =
            &SegmentedString::AdvanceAndUpdateLineNumber16;
      else
        advance_and_update_line_number_func_ = &SegmentedString::Advance16;
      return;
    }

    if (!current_string_.length() && !IsComposite()) {
      advance_func_ = &SegmentedString::AdvanceEmpty;
      fast_path_flags_ = kNoFastPath;
      advance_and_update_line_number_func_ = &SegmentedString::AdvanceEmpty;
    }

    UpdateSlowCaseFunctionPointers();
  }

  inline LookAheadResult LookAheadInline(const String& string,
                                         TextCaseSensitivity case_sensitivity) {
    if (string.length() <= static_cast<unsigned>(current_string_.length())) {
      String current_substring =
          current_string_.CurrentSubString(string.length());
      if (current_substring.StartsWith(string, case_sensitivity))
        return kDidMatch;
      return kDidNotMatch;
    }
    return LookAheadSlowCase(string, case_sensitivity);
  }

  LookAheadResult LookAheadSlowCase(const String& string,
                                    TextCaseSensitivity case_sensitivity) {
    unsigned count = string.length();
    if (count > length())
      return kNotEnoughCharacters;
    UChar* consumed_characters;
    String consumed_string =
        String::CreateUninitialized(count, consumed_characters);
    Advance(count, consumed_characters);
    LookAheadResult result = kDidNotMatch;
    if (consumed_string.StartsWith(string, case_sensitivity))
      result = kDidMatch;
    Prepend(SegmentedString(consumed_string), PrependType::kUnconsume);
    return result;
  }

  bool IsComposite() const { return !substrings_.IsEmpty(); }

  SegmentedSubstring current_string_;
  UChar current_char_;
  int number_of_characters_consumed_prior_to_current_string_;
  int number_of_characters_consumed_prior_to_current_line_;
  int current_line_;
  Deque<SegmentedSubstring> substrings_;
  bool closed_;
  bool empty_;
  unsigned char fast_path_flags_;
  void (SegmentedString::*advance_func_)();
  void (SegmentedString::*advance_and_update_line_number_func_)();
};

}  // namespace blink

#endif
