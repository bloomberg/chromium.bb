/*
 * Copyright 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
 */

#ifndef WebCoreSystemInterface_h
#define WebCoreSystemInterface_h

#include <objc/objc.h>

typedef const struct __CFDictionary * CFDictionaryRef;
typedef struct CGSize CGSize;
typedef struct CGRect CGRect;
typedef struct CGAffineTransform CGAffineTransform;
typedef struct CGContext *CGContextRef;
typedef struct CGFont *CGFontRef;
typedef unsigned short CGGlyph;

typedef const struct __CTFont * CTFontRef;
typedef const struct __CTLine * CTLineRef;
typedef struct _NSRange NSRange;

typedef UInt16 ATSGlyphRef;

#ifdef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGPoint NSPoint;
typedef struct CGRect NSRect;
#else
typedef struct _NSPoint NSPoint;
typedef struct _NSRect NSRect;
#endif

OBJC_CLASS NSFont;
OBJC_CLASS NSString;

extern "C" {

extern void WKDrawBezeledTextFieldCell(NSRect, BOOL enabled);
extern void WKDrawCapsLockIndicator(CGContextRef, CGRect);
extern void WKDrawBezeledTextArea(NSRect, BOOL enabled);
extern NSFont* WKGetFontInLanguageForRange(NSFont*, NSString*, NSRange);
extern NSFont* WKGetFontInLanguageForCharacter(NSFont*, UniChar);
extern BOOL WKGetGlyphTransformedAdvances(CGFontRef, NSFont*, CGAffineTransform*, ATSGlyphRef*, CGSize* advance);
extern void WKSetUpFontCache();
extern void WKGetGlyphsForCharacters(CGFontRef, const UniChar[], CGGlyph[], size_t);
extern bool WKGetVerticalGlyphsForCharacters(CTFontRef, const UniChar[], CGGlyph[], size_t);
extern CTLineRef WKCreateCTLineWithUniCharProvider(const UniChar* (*provide)(CFIndex stringIndex, CFIndex* charCount, CFDictionaryRef* attributes, void*), void (*dispose)(const UniChar* chars, void*), void*);

}  // extern "C"

#endif
