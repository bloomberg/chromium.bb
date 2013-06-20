/*
 *  Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef ExceptionCode_h
#define ExceptionCode_h

// FIXME: Move this header into the files that actually need it.
#include "DOMException.h"

namespace WebCore {

    // The DOM standards use unsigned short for exception codes.
    // In our DOM implementation we use int instead, and use different
    // numerical ranges for different types of DOM exception, so that
    // an exception of any type can be expressed with a single integer.
    typedef int ExceptionCode;


    // Some of these are considered historical since they have been
    // changed or removed from the specifications.
    enum {
        INDEX_SIZE_ERR = 1,
        HIERARCHY_REQUEST_ERR,
        WRONG_DOCUMENT_ERR,
        INVALID_CHARACTER_ERR,
        NO_MODIFICATION_ALLOWED_ERR,
        NOT_FOUND_ERR,
        NOT_SUPPORTED_ERR,
        INUSE_ATTRIBUTE_ERR, // Historical. Only used in setAttributeNode etc which have been removed from the DOM specs.

        // Introduced in DOM Level 2:
        INVALID_STATE_ERR,
        SYNTAX_ERR,
        INVALID_MODIFICATION_ERR,
        NAMESPACE_ERR,
        INVALID_ACCESS_ERR,

        // Introduced in DOM Level 3:
        TYPE_MISMATCH_ERR, // Historical; use TypeError instead

        // XMLHttpRequest extension:
        SECURITY_ERR,

        // Others introduced in HTML5:
        NETWORK_ERR,
        ABORT_ERR,
        URL_MISMATCH_ERR,
        QUOTA_EXCEEDED_ERR,
        TIMEOUT_ERR,
        INVALID_NODE_TYPE_ERR,
        DATA_CLONE_ERR,

        // WebIDL exception types, handled by the binding layer.
        // FIXME: Add GeneralError, EvalError, etc. when implemented in the bindings.
        TypeError = 105,
    };

} // namespace WebCore

#endif // ExceptionCode_h
