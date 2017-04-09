/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WTF_NonCopyingSort_h
#define WTF_NonCopyingSort_h

namespace WTF {

using std::swap;

template <typename RandomAccessIterator, typename Predicate>
inline void SiftDown(RandomAccessIterator array,
                     ptrdiff_t start,
                     ptrdiff_t end,
                     Predicate compare_less) {
  ptrdiff_t root = start;

  while (root * 2 + 1 <= end) {
    ptrdiff_t child = root * 2 + 1;
    if (child < end && compare_less(array[child], array[child + 1]))
      child++;

    if (compare_less(array[root], array[child])) {
      swap(array[root], array[child]);
      root = child;
    } else {
      return;
    }
  }
}

template <typename RandomAccessIterator, typename Predicate>
inline void Heapify(RandomAccessIterator array,
                    ptrdiff_t count,
                    Predicate compare_less) {
  ptrdiff_t start = (count - 2) / 2;

  while (start >= 0) {
    SiftDown(array, start, count - 1, compare_less);
    start--;
  }
}

template <typename RandomAccessIterator, typename Predicate>
void HeapSort(RandomAccessIterator start,
              RandomAccessIterator end,
              Predicate compare_less) {
  ptrdiff_t count = end - start;
  Heapify(start, count, compare_less);

  ptrdiff_t end_index = count - 1;
  while (end_index > 0) {
    swap(start[end_index], start[0]);
    SiftDown(start, 0, end_index - 1, compare_less);
    end_index--;
  }
}

template <typename RandomAccessIterator, typename Predicate>
inline void NonCopyingSort(RandomAccessIterator start,
                           RandomAccessIterator end,
                           Predicate compare_less) {
  // heapsort happens to use only swaps, not copies, but the essential thing
  // about this function is the fact that it does not copy, not the specific
  // algorithm
  HeapSort(start, end, compare_less);
}

}  // namespace WTF

using WTF::NonCopyingSort;

#endif  // WTF_NonCopyingSort_h
