/*
 * FNV1A64 Hash
 * http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-param
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Langley, Google Inc.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* $Id: fnv1a64.c,v 1.0 2010/08/09 13:00:00 agl%google.com Exp $ */

#include "prtypes.h"
#include "prnetdb.h"

/* Older versions of Visual C++ don't support the 'ull' suffix. */
#ifdef _MSC_VER
static const PRUint64 FNV1A64_OFFSET_BASIS = 14695981039346656037ui64;
static const PRUint64 FNV1A64_PRIME = 1099511628211ui64;
#else
static const PRUint64 FNV1A64_OFFSET_BASIS = 14695981039346656037ull;
static const PRUint64 FNV1A64_PRIME = 1099511628211ull;
#endif

void FNV1A64_Init(PRUint64* digest) {
    *digest = FNV1A64_OFFSET_BASIS;
}

void FNV1A64_Update(PRUint64* digest, const unsigned char *data,
                    unsigned int length) {
    unsigned int i;

    for (i = 0; i < length; i++) {
        *digest ^= data[i];
        *digest *= FNV1A64_PRIME;
    }
}

void FNV1A64_Final(PRUint64 *digest) {
    *digest = PR_htonll(*digest);
}
