// Copyright (c) 2011, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ---
// Author: Rebecca Shapiro <bxx@google.com>
//
// This file contains functions that implement doubly linked and
// singly linked lists.  The singly linked lists are null terminated,
// use raw pointers to link neighboring elements, and these pointers
// are stored at the start of each element, independently of the
// elements's size.  Because pointers are stored within each element,
// each element must be large enough to store two raw pointers if
// doubly linked lists are employed, or one raw pointer if singly
// linked lists are employed.  On machines with 64 bit pointers, this
// means elements must be at least 16 bytes in size for doubly linked
// list support, and 8 bytes for singly linked list support.  No
// attempts are made to preserve the data in elements stored in the
// list.
//
// Given a machine with pointers of size N (on a 64bit machine N=8, on
// a 32bit machine, N=4), the list pointers are stored in the
// following manner:
// -In doubly linked lists, the |next| pointer is stored in the first N
// bytes of the node and the |previous| pointer is writtend into the
// second N bytes.
// -In singly linked lists, the |next| pointer is stored in the first N
// bytes of the node.
//
// For both types of lists: when a pop operation is performed on a non
// empty list, the new list head becomes that which is pointed to by
// the former head's |next| pointer.  If the list is doubly linked, the
// new head |previous| pointer gets changed from pointing to the former
// head to NULL.


#ifdef TCMALLOC_USE_DOUBLYLINKED_FREELIST

#include <stddef.h>
#include "internal_logging.h" //for ASSERT

#define MEMORY_CHECK(v1, v2) \
  if (v1 != v2) CRASH("Memory corruption detected.\n")

namespace {
// Returns value of the |previous| pointer w/out running a sanity
// check.
inline void *FL_Previous_No_Check(void *t) {
  return reinterpret_cast<void**>(t)[1];
}

// Returns value of the |next| pointer w/out running a sanity check.
inline void *FL_Next_No_Check(void *t) {
  return reinterpret_cast<void**>(t)[0];
}

void *FL_Previous(void *t) {
  void *previous = FL_Previous_No_Check(t);
  if (previous) {
    MEMORY_CHECK(FL_Next_No_Check(previous), t);
  }
  return previous;
}

inline void FL_SetPrevious(void *t, void *n) {
  reinterpret_cast<void**>(t)[1] = n;
}

inline void FL_SetNext(void *t, void *n) {
  reinterpret_cast<void**>(t)[0] = n;
}

} // namespace

namespace tcmalloc {

void *FL_Next(void *t) {
  void *next = FL_Next_No_Check(t);
  if (next) {
    MEMORY_CHECK(FL_Previous_No_Check(next), t);
  }
  return next;
}

// Makes the element at |t| a singleton doubly linked list.
inline void FL_Init(void *t) {
  FL_SetPrevious(t, NULL);
  FL_SetNext(t, NULL);
}

// Pushes element to a linked list whose first element is at
// |*list|. When this call returns, |list| will point to the new head
// of the linked list.
void FL_Push(void **list, void *element) {
  void *old = *list;
  if (old == NULL) { // Builds singleton list.
    FL_Init(element);
  } else {
    ASSERT(FL_Previous_No_Check(old) == NULL);
    FL_SetNext(element, old);
    FL_SetPrevious(old, element);
    FL_SetPrevious(element, NULL);
  }
  *list = element;
}

// Pops the top element off the linked list whose first element is at
// |*list|, and updates |*list| to point to the next element in the
// list.  Returns the address of the element that was removed from the
// linked list.  |list| must not be NULL.
void *FL_Pop(void **list) {
  void *result = *list;
  ASSERT(FL_Previous_No_Check(result) == NULL);
  *list = FL_Next(result);
  if (*list != NULL) {
    FL_SetPrevious(*list, NULL);
  }
  return result;
}

// Remove |n| elements from linked list at whose first element is at
// |*head|.  |head| will be modified to point to the new head.
// |start| will point to the first node of the range, |end| will point
// to the last node in the range. |n| must be <= FL_Size(|*head|)
// If |n| > 0, |head| must not be NULL.
void FL_PopRange(void **head, int n, void **start, void **end) {
  if (n == 0) {
    *start = NULL;
    *end = NULL;
    return;
  }

  *start = *head; // Remember the first node in the range.
  void *tmp = *head;
  for (int i = 1; i < n; ++i) { // Find end of range.
    tmp = FL_Next(tmp);
  }
  *end = tmp; // |end| now set to point to last node in range.
  *head = FL_Next(*end);
  FL_SetNext(*end, NULL); // Unlink range from list.

  if (*head ) { // Fixup popped list.
    FL_SetPrevious(*head, NULL);
  }
}

// Pushes the nodes in the list begginning at |start| whose last node
// is |end| into the linked list at |*head|. |*head| is updated to
// point be the new head of the list.  |head| must not be NULL.
void FL_PushRange(void **head, void *start, void *end) {
  if (!start) return;

  // Sanity checking of ends of list to push is done by calling
  // FL_Next and FL_Previous.
  FL_Next(start);
  FL_Previous(end);
  ASSERT(FL_Previous_No_Check(start) == NULL);
  ASSERT(FL_Next_No_Check(end) == NULL);

  if (*head) {
    MEMORY_CHECK(FL_Previous_No_Check(*head), NULL);
    FL_SetNext(end, *head);
    FL_SetPrevious(*head, end);
  }
  *head = start;
}

// Calculates the size of the list that begins at |head|.
size_t FL_Size(void *head){
  int count = 0;
  if (head) {
    MEMORY_CHECK(FL_Previous_No_Check(head), NULL);
  }
  while (head) {
    count++;
    head = FL_Next(head);
  }
  return count;
}

} // namespace tcmalloc

#else
#include "linked_list.h" // for SLL_SetNext

namespace {

inline void FL_SetNext(void *t, void *n) {
  tcmalloc::SLL_SetNext(t,n);
}

}

#endif // TCMALLOC_USE_DOUBLYLINKED_FREELIST
