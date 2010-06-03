//  (C) Copyright Greg Colvin and Beman Dawes 1998, 1999.
//  Copyright (c) 2001, 2002 Peter Dimov
//
//  Permission to copy, use, modify, sell and distribute this software
//  is granted provided this copyright notice appears in all copies.
//  This software is provided "as is" without express or implied
//  warranty, and with no claim as to its suitability for any purpose.
//
//  See http://www.boost.org/libs/smart_ptr/scoped_ptr.htm for documentation.
//

//  scoped_ptr mimics a built-in pointer except that it guarantees deletion
//  of the object pointed to, either on destruction of the scoped_ptr or via
//  an explicit reset(). scoped_ptr is a simple solution for simple needs;
//  use shared_ptr or std::auto_ptr if your needs are more complex.

//  scoped_ptr_malloc added in by Google.  When one of
//  these goes out of scope, instead of doing a delete or delete[], it
//  calls free().  scoped_ptr_malloc<char> is likely to see much more
//  use than any other specializations.

//  release() added in by Google. Use this to conditionally
//  transfer ownership of a heap-allocated object to the caller, usually on
//  method success.
#ifndef TALK_BASE_SCOPED_PTR_H__
#define TALK_BASE_SCOPED_PTR_H__

#include <cstddef>            // for std::ptrdiff_t
#include <assert.h>           // for assert
#include <stdlib.h>           // for free() decl

#ifdef _WIN32
namespace std { using ::ptrdiff_t; };
#endif // _WIN32

#include "base/scoped_ptr.h"

#endif  // #ifndef TALK_BASE_SCOPED_PTR_H__
