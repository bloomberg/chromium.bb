/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/LengthBox.h"

namespace blink {

const Length& LengthBox::LogicalLeft(WritingMode writing_mode,
                                     const Length& left,
                                     const Length& top) {
  return IsHorizontalWritingMode(writing_mode) ? left : top;
}

const Length& LengthBox::LogicalRight(WritingMode writing_mode,
                                      const Length& right,
                                      const Length& bottom) {
  return IsHorizontalWritingMode(writing_mode) ? right : bottom;
}

const Length& LengthBox::Before(WritingMode writing_mode,
                                const Length& top,
                                const Length& left,
                                const Length& right) {
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      return top;
    case WritingMode::kVerticalLr:
      return left;
    case WritingMode::kVerticalRl:
      return right;
  }
  NOTREACHED();
  return top;
}

const Length& LengthBox::After(WritingMode writing_mode,
                               const Length& bottom,
                               const Length& left,
                               const Length& right) {
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      return bottom;
    case WritingMode::kVerticalLr:
      return right;
    case WritingMode::kVerticalRl:
      return left;
  }
  NOTREACHED();
  return bottom;
}

const Length& LengthBox::Start(WritingMode writing_mode,
                               TextDirection direction,
                               const Length& top,
                               const Length& left,
                               const Length& right,
                               const Length& bottom) {
  if (IsHorizontalWritingMode(writing_mode))
    return IsLtr(direction) ? left : right;
  return IsLtr(direction) ? top : bottom;
}

const Length& LengthBox::End(WritingMode writing_mode,
                             TextDirection direction,
                             const Length& top,
                             const Length& left,
                             const Length& right,
                             const Length& bottom) {
  if (IsHorizontalWritingMode(writing_mode))
    return IsLtr(direction) ? right : left;
  return IsLtr(direction) ? bottom : top;
}

const Length& LengthBox::Over(WritingMode writing_mode,
                              const Length& top,
                              const Length& right) {
  return IsHorizontalWritingMode(writing_mode) ? top : right;
}

const Length& LengthBox::Under(WritingMode writing_mode,
                               const Length& bottom,
                               const Length& left) {
  return IsHorizontalWritingMode(writing_mode) ? bottom : left;
}

}  // namespace blink
