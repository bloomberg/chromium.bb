// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// This list must be in sync with the enum in ExceptionCode.h.  The order matters.
var domExceptions = [
    "IndexSizeError",
    "HierarchyRequestError",
    "WrongDocumentError",
    "InvalidCharacterError",
    "NoModificationAllowedError",
    "NotFoundError",
    "NotSupportedError",
    "InUseAttributeError", // Historical. Only used in setAttributeNode etc which have been removed from the DOM specs.

    // Introduced in DOM Level 2:
    "InvalidStateError",
    "SyntaxError",
    "InvalidModificationError",
    "NamespaceError",
    "InvalidAccessError",

    // Introduced in DOM Level 3:
    "TypeMismatchError", // Historical; use TypeError instead

    // XMLHttpRequest extension:
    "SecurityError",

    // Others introduced in HTML5:
    "NetworkError",
    "AbortError",
    "URLMismatchError",
    "QuotaExceededError",
    "TimeoutError",
    "InvalidNodeTypeError",
    "DataCloneError",

    // These are IDB-specific.
    "UnknownError",
    "ConstraintError",
    "DataError",
    "TransactionInactiveError",
    "ReadOnlyError",
    "VersionError",

    // File system
    "NotReadableError",
    "EncodingError",
    "PathExistsError",

    // SQL
    "SQLDatabaseError", // Naming conflict with DatabaseError class.

    // Web Crypto
    "OperationError",

    // WebIDL exception types, handled by the binding layer.
    // FIXME: Add GeneralError, EvalError, etc. when implemented in the bindings.
    "TypeError",
    "RangeError",
];

var domExceptionCode = {};
var installedClasses = {};

function init()
{
    var code = 1;
    domExceptions.forEach(function (exception) {
        domExceptionCode[exception] = code;
        code++;
    });
}

function DOMExceptionInPrivateScript(code, message)
{
    this.code = domExceptionCode[code] || 0;
    this.message = message;
    this.name = "DOMExceptionInPrivateScript";
}

function privateScriptClass()
{
}

function installClass(className, implementation)
{
    installedClasses[className] = new privateScriptClass();
    implementation(window, installedClasses[className]);
}

init();

// This line must be the last statement of this JS file.
// A parenthesis is needed, because the caller of this script (PrivateScriptRunner.cpp)
// is depending on the completion value of this script.
(installedClasses);
