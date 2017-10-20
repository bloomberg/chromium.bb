/*
 * Copyright (C) 2011 Nokia Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef QuotesData_h
#define QuotesData_h

#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class QuotesData : public RefCounted<QuotesData> {
 public:
  static scoped_refptr<QuotesData> Create() {
    return WTF::AdoptRef(new QuotesData());
  }
  static scoped_refptr<QuotesData> Create(UChar open1,
                                          UChar close1,
                                          UChar open2,
                                          UChar close2);

  bool operator==(const QuotesData& o) const {
    return quote_pairs_ == o.quote_pairs_;
  }
  bool operator!=(const QuotesData& o) const { return !(*this == o); }

  void AddPair(const std::pair<String, String> quote_pair);
  const String GetOpenQuote(int index) const;
  const String GetCloseQuote(int index) const;
  int size() { return quote_pairs_.size(); }

 private:
  QuotesData() {}

  Vector<std::pair<String, String>> quote_pairs_;
};

}  // namespace blink

#endif  // QuotesData_h
