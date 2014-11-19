//
//   Copyright (C) 2012 International Business Machines Corporation
//   and others. All rights reserved.
//
//   file:  regeximp.cpp
//
//           ICU Regular Expressions,
//             miscellaneous implementation functions.
//

#include "unicode/utypes.h"

#if !UCONFIG_NO_REGULAR_EXPRESSIONS
#include "regeximp.h"

#include "uassert.h"
#include "unicode/utf16.h"

U_NAMESPACE_BEGIN

CaseFoldingUTextIterator::CaseFoldingUTextIterator(UText &text) :
   fUText(text), fcsp(NULL), fFoldChars(NULL), fFoldLength(0) {
   fcsp = ucase_getSingleton();
}

CaseFoldingUTextIterator::~CaseFoldingUTextIterator() {}

UChar32 CaseFoldingUTextIterator::next() {
    UChar32  foldedC;
    UChar32  originalC;
    if (fFoldChars == NULL) {
        // We are not in a string folding of an earlier character.
        // Start handling the next char from the input UText.
        originalC = UTEXT_NEXT32(&fUText);
        if (originalC == U_SENTINEL) {
            return originalC;
        }
        fFoldLength = ucase_toFullFolding(fcsp, originalC, &fFoldChars, U_FOLD_CASE_DEFAULT);
        if (fFoldLength >= UCASE_MAX_STRING_LENGTH || fFoldLength < 0) {
            // input code point folds to a single code point, possibly itself.
            // See comment in ucase.h for explanation of return values from ucase_toFullFoldings.
            if (fFoldLength < 0) {
                fFoldLength = ~fFoldLength;
            }
            foldedC = (UChar32)fFoldLength;
            fFoldChars = NULL;
            return foldedC;
        }
        // String foldings fall through here.
        fFoldIndex = 0;
    }

    U16_NEXT(fFoldChars, fFoldIndex, fFoldLength, foldedC);
    if (fFoldIndex >= fFoldLength) {
        fFoldChars = NULL;
    }
    return foldedC;
}
    

UBool CaseFoldingUTextIterator::inExpansion() {
    return fFoldChars != NULL;
}



CaseFoldingUCharIterator::CaseFoldingUCharIterator(const UChar *chars, int64_t start, int64_t limit) :
   fChars(chars), fIndex(start), fLimit(limit), fcsp(NULL), fFoldChars(NULL), fFoldLength(0) {
   fcsp = ucase_getSingleton();
}


CaseFoldingUCharIterator::~CaseFoldingUCharIterator() {}


UChar32 CaseFoldingUCharIterator::next() {
    UChar32  foldedC;
    UChar32  originalC;
    if (fFoldChars == NULL) {
        // We are not in a string folding of an earlier character.
        // Start handling the next char from the input UText.
        if (fIndex >= fLimit) {
            return U_SENTINEL;
        }
        U16_NEXT(fChars, fIndex, fLimit, originalC);

        fFoldLength = ucase_toFullFolding(fcsp, originalC, &fFoldChars, U_FOLD_CASE_DEFAULT);
        if (fFoldLength >= UCASE_MAX_STRING_LENGTH || fFoldLength < 0) {
            // input code point folds to a single code point, possibly itself.
            // See comment in ucase.h for explanation of return values from ucase_toFullFoldings.
            if (fFoldLength < 0) {
                fFoldLength = ~fFoldLength;
            }
            foldedC = (UChar32)fFoldLength;
            fFoldChars = NULL;
            return foldedC;
        }
        // String foldings fall through here.
        fFoldIndex = 0;
    }

    U16_NEXT(fFoldChars, fFoldIndex, fFoldLength, foldedC);
    if (fFoldIndex >= fFoldLength) {
        fFoldChars = NULL;
    }
    return foldedC;
}
    

UBool CaseFoldingUCharIterator::inExpansion() {
    return fFoldChars != NULL;
}

int64_t CaseFoldingUCharIterator::getIndex() {
    return fIndex;
}

// Assemble a pcode instruction from the opcode and operand values.
// Out-of-range values should not occur - if they do it is from an internal
// error in the regex compiler. 

// TODO: move into regexcmp, where it has access to fStatus.
//       NOP cleanly if U_FAILURE.
//       Set U_REGEX_INTERNAL_ERROR on bad operands.

int32_t URX_BUILD(int32_t type, int32_t val) {
    if (type < 0 || type > 255) {
        U_ASSERT(FALSE);
        type = URX_RESERVED_OP;
    }
    if (val > 0x00ffffff) {
        U_ASSERT(FALSE);
        val = 0;
    }
    return (type << 24) | val;
}


U_NAMESPACE_END

#endif

