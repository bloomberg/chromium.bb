/*
 * $RCSId: xc/lib/fontconfig/src/fcint.h,v 1.27 2002/08/31 22:17:32 keithp Exp $
 *
 * Copyright © 2000 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _FCINT_H_
#define _FCINT_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#elif defined(HAVE_STDINT_H)
#include <stdint.h>
#else
#error missing C99 integer data types
#endif
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcprivate.h>
#include <fontconfig/fcfreetype.h>

#ifndef FC_CONFIG_PATH
#define FC_CONFIG_PATH "fonts.conf"
#endif

#define FC_FONT_FILE_INVALID	((FcChar8 *) ".")
#define FC_FONT_FILE_DIR	((FcChar8 *) ".dir")
#define FC_GLOBAL_MAGIC_COOKIE	"GLOBAL"

#ifdef _WIN32
#define FC_SEARCH_PATH_SEPARATOR ';'
#else
#define FC_SEARCH_PATH_SEPARATOR ':'
#endif

#define FC_DBG_MATCH	1
#define FC_DBG_MATCHV	2
#define FC_DBG_EDIT	4
#define FC_DBG_FONTSET	8
#define FC_DBG_CACHE	16
#define FC_DBG_CACHEV	32
#define FC_DBG_PARSE	64
#define FC_DBG_SCAN	128
#define FC_DBG_SCANV	256
#define FC_DBG_MEMORY	512
#define FC_DBG_CONFIG	1024
#define FC_DBG_LANGSET	2048

#define FC_MEM_CHARSET	    0
#define FC_MEM_CHARLEAF	    1
#define FC_MEM_FONTSET	    2
#define FC_MEM_FONTPTR	    3
#define FC_MEM_OBJECTSET    4
#define FC_MEM_OBJECTPTR    5
#define FC_MEM_MATRIX	    6
#define FC_MEM_PATTERN	    7
#define FC_MEM_PATELT	    8
#define FC_MEM_VALLIST	    9
#define FC_MEM_SUBSTATE	    10
#define FC_MEM_STRING	    11
#define FC_MEM_LISTBUCK	    12
#define FC_MEM_STRSET	    13
#define FC_MEM_STRLIST	    14
#define FC_MEM_CONFIG	    15
#define FC_MEM_LANGSET	    16
#define FC_MEM_ATOMIC	    17
#define FC_MEM_BLANKS	    18
#define FC_MEM_CACHE	    19
#define FC_MEM_STRBUF	    20
#define FC_MEM_SUBST	    21
#define FC_MEM_OBJECTTYPE   22
#define FC_MEM_CONSTANT	    23
#define FC_MEM_TEST	    24
#define FC_MEM_EXPR	    25
#define FC_MEM_VSTACK	    26
#define FC_MEM_ATTR	    27
#define FC_MEM_PSTACK	    28
#define FC_MEM_STATICSTR    29

#define FC_MEM_NUM	    30

#define FC_BANK_DYNAMIC 0
#define FC_BANK_FIRST 1
#define FC_BANK_LANGS	    0xfcfcfcfc

typedef enum _FcValueBinding {
    FcValueBindingWeak, FcValueBindingStrong, FcValueBindingSame
} FcValueBinding;

/*
 * Serialized data structures use only offsets instead of pointers
 * A low bit of 1 indicates an offset.
 */
 
/* Is the provided pointer actually an offset? */
#define FcIsEncodedOffset(p)	((((intptr_t) (p)) & 1) != 0)

/* Encode offset in a pointer of type t */
#define FcOffsetEncode(o,t)	((t *) ((o) | 1))

/* Decode a pointer into an offset */
#define FcOffsetDecode(p)	(((intptr_t) (p)) & ~1)

/* Compute pointer offset */
#define FcPtrToOffset(b,p)	((intptr_t) (p) - (intptr_t) (b))

/* Given base address, offset and type, return a pointer */
#define FcOffsetToPtr(b,o,t)	((t *) ((intptr_t) (b) + (o)))

/* Given base address, encoded offset and type, return a pointer */
#define FcEncodedOffsetToPtr(b,p,t) FcOffsetToPtr(b,FcOffsetDecode(p),t)

/* Given base address, pointer and type, return an encoded offset */
#define FcPtrToEncodedOffset(b,p,t) FcOffsetEncode(FcPtrToOffset(b,p),t)

/* Given a structure, offset member and type, return pointer */
#define FcOffsetMember(s,m,t)	    FcOffsetToPtr(s,(s)->m,t)

/* Given a structure, encoded offset member and type, return pointer to member */
#define FcEncodedOffsetMember(s,m,t) FcOffsetToPtr(s,FcOffsetDecode((s)->m), t)

/* Given a structure, member and type, convert the member to a pointer */
#define FcPointerMember(s,m,t)	(FcIsEncodedOffset((s)->m) ? \
				 FcEncodedOffsetMember (s,m,t) : \
				 (s)->m)

/*
 * Serialized values may hold strings, charsets and langsets as pointers,
 * unfortunately FcValue is an exposed type so we can't just always use
 * offsets
 */
#define FcValueString(v)	FcPointerMember(v,u.s,FcChar8)
#define FcValueCharSet(v)	FcPointerMember(v,u.c,const FcCharSet)
#define FcValueLangSet(v)	FcPointerMember(v,u.l,const FcLangSet)

typedef struct _FcValueList *FcValueListPtr;

typedef struct _FcValueList {
    struct _FcValueList	*next;
    FcValue		value;
    FcValueBinding	binding;
} FcValueList;

#define FcValueListNext(vl)	FcPointerMember(vl,next,FcValueList)
			     
typedef int FcObject;

typedef struct _FcPatternElt *FcPatternEltPtr;

/*
 * Pattern elts are stuck in a structure connected to the pattern, 
 * so they get moved around when the pattern is resized. Hence, the
 * values field must be a pointer/offset instead of just an offset
 */
typedef struct _FcPatternElt {
    FcObject		object;
    FcValueList		*values;
} FcPatternElt;

#define FcPatternEltValues(pe)	FcPointerMember(pe,values,FcValueList)

struct _FcPattern {
    int		    num;
    int		    size;
    intptr_t	    elts_offset;
    int		    ref;
};

#define FcPatternElts(p)	FcOffsetMember(p,elts_offset,FcPatternElt)

#define FcFontSetFonts(fs)	FcPointerMember(fs,fonts,FcPattern *)

#define FcFontSetFont(fs,i)	(FcIsEncodedOffset((fs)->fonts) ? \
				 FcEncodedOffsetToPtr(FcFontSetFonts(fs), \
						      FcFontSetFonts(fs)[i], \
						      FcPattern) : \
				 fs->fonts[i])
						
typedef enum _FcOp {
    FcOpInteger, FcOpDouble, FcOpString, FcOpMatrix, FcOpBool, FcOpCharSet, 
    FcOpNil,
    FcOpField, FcOpConst,
    FcOpAssign, FcOpAssignReplace, 
    FcOpPrependFirst, FcOpPrepend, FcOpAppend, FcOpAppendLast,
    FcOpQuest,
    FcOpOr, FcOpAnd, FcOpEqual, FcOpNotEqual, 
    FcOpContains, FcOpListing, FcOpNotContains,
    FcOpLess, FcOpLessEqual, FcOpMore, FcOpMoreEqual,
    FcOpPlus, FcOpMinus, FcOpTimes, FcOpDivide,
    FcOpNot, FcOpComma, FcOpFloor, FcOpCeil, FcOpRound, FcOpTrunc,
    FcOpInvalid
} FcOp;

typedef struct _FcExpr {
    FcOp   op;
    union {
	int	    ival;
	double	    dval;
	FcChar8	    *sval;
	FcMatrix    *mval;
	FcBool	    bval;
	FcCharSet   *cval;
	FcObject    object;
	FcChar8	    *constant;
	struct {
	    struct _FcExpr *left, *right;
	} tree;
    } u;
} FcExpr;

typedef enum _FcQual {
    FcQualAny, FcQualAll, FcQualFirst, FcQualNotFirst
} FcQual;

#define FcMatchDefault	((FcMatchKind) -1)

typedef struct _FcTest {
    struct _FcTest	*next;
    FcMatchKind		kind;
    FcQual		qual;
    FcObject		object;
    FcOp		op;
    FcExpr		*expr;
} FcTest;

typedef struct _FcEdit {
    struct _FcEdit *next;
    FcObject	    object;
    FcOp	    op;
    FcExpr	    *expr;
    FcValueBinding  binding;
} FcEdit;

typedef struct _FcSubst {
    struct _FcSubst	*next;
    FcTest		*test;
    FcEdit		*edit;
} FcSubst;

typedef struct _FcCharLeaf {
    FcChar32	map[256/32];
} FcCharLeaf;

#define FC_REF_CONSTANT	    -1

struct _FcCharSet {
    int		    ref;	/* reference count */
    int		    num;	/* size of leaves and numbers arrays */
    intptr_t	    leaves_offset;
    intptr_t	    numbers_offset;
};

#define FcCharSetLeaves(c)	FcOffsetMember(c,leaves_offset,intptr_t)
#define FcCharSetLeaf(c,i)	(FcOffsetToPtr(FcCharSetLeaves(c), \
					       FcCharSetLeaves(c)[i], \
					       FcCharLeaf))
#define FcCharSetNumbers(c)	FcOffsetMember(c,numbers_offset,FcChar16)

struct _FcStrSet {
    int		    ref;	/* reference count */
    int		    num;
    int		    size;
    FcChar8	    **strs;
};

struct _FcStrList {
    FcStrSet	    *set;
    int		    n;
};

typedef struct _FcStrBuf {
    FcChar8 *buf;
    FcBool  allocated;
    FcBool  failed;
    int	    len;
    int	    size;
} FcStrBuf;

typedef struct _FcCache {
    int		magic;              /* FC_CACHE_MAGIC */
    intptr_t	size;		    /* size of file */
    intptr_t	dir;		    /* offset to dir name */
    intptr_t	dirs;		    /* offset to subdirs */
    int		dirs_count;	    /* number of subdir strings */
    intptr_t	set;		    /* offset to font set */
} FcCache;

#define FcCacheDir(c)	FcOffsetMember(c,dir,FcChar8)
#define FcCacheDirs(c)	FcOffsetMember(c,dirs,intptr_t)
#define FcCacheSet(c)	FcOffsetMember(c,set,FcFontSet)

/*
 * Used while constructing a directory cache object
 */

#define FC_SERIALIZE_HASH_SIZE	8191

typedef struct _FcSerializeBucket {
    struct _FcSerializeBucket *next;
    const void	*object;
    intptr_t	offset;
} FcSerializeBucket;

typedef struct _FcSerialize {
    intptr_t		size;
    void		*linear;
    FcSerializeBucket	*buckets[FC_SERIALIZE_HASH_SIZE];
} FcSerialize;
    
/*
 * To map adobe glyph names to unicode values, a precomputed hash
 * table is used
 */

typedef struct _FcGlyphName {
    FcChar32	ucs;		/* unicode value */
    FcChar8	name[1];	/* name extends beyond struct */
} FcGlyphName;

/*
 * To perform case-insensitive string comparisons, a table
 * is used which holds three different kinds of folding data.
 * 
 * The first is a range of upper case values mapping to a range
 * of their lower case equivalents.  Within each range, the offset
 * between upper and lower case is constant.
 *
 * The second is a range of upper case values which are interleaved
 * with their lower case equivalents.
 * 
 * The third is a set of raw unicode values mapping to a list
 * of unicode values for comparison purposes.  This allows conversion
 * of ß to "ss" so that SS, ss and ß all match.  A separate array
 * holds the list of unicode values for each entry.
 *
 * These are packed into a single table.  Using a binary search,
 * the appropriate entry can be located.
 */

#define FC_CASE_FOLD_RANGE	    0
#define FC_CASE_FOLD_EVEN_ODD	    1
#define FC_CASE_FOLD_FULL	    2

typedef struct _FcCaseFold {
    FcChar32	upper;
    FcChar16	method : 2;
    FcChar16	count : 14;
    short    	offset;	    /* lower - upper for RANGE, table id for FULL */
} FcCaseFold;

#define FC_MAX_FILE_LEN	    4096

/* XXX remove these when we're ready */

#define fc_value_string(v)	FcValueString(v)
#define fc_value_charset(v)	FcValueCharSet(v)
#define fc_value_langset(v)	FcValueLangSet(v)
#define fc_storage_type(v)	((v)->type)

#define fc_alignof(type) offsetof (struct { char c; type member; }, member)

#define FC_CACHE_MAGIC	    0xFC02FC04
#define FC_CACHE_MAGIC_COPY 0xFC02FC05

struct _FcAtomic {
    FcChar8	*file;		/* original file name */
    FcChar8	*new;		/* temp file name -- write data here */
    FcChar8	*lck;		/* lockfile name (used for locking) */
    FcChar8	*tmp;		/* tmpfile name (used for locking) */
};

struct _FcBlanks {
    int		nblank;
    int		sblank;
    FcChar32	*blanks;
};

typedef struct _FcCacheList {
    struct _FcCacheList *next;
    FcCache		*cache;
} FcCacheList;

struct _FcConfig {
    /*
     * File names loaded from the configuration -- saved here as the
     * cache file must be consulted before the directories are scanned,
     * and those directives may occur in any order
     */
    FcStrSet	*configDirs;	    /* directories to scan for fonts */
    /*
     * Set of allowed blank chars -- used to
     * trim fonts of bogus glyphs
     */
    FcBlanks	*blanks;
    /*
     * List of directories containing fonts,
     * built by recursively scanning the set 
     * of configured directories
     */
    FcStrSet	*fontDirs;
    /*
     * List of directories containing cache files.
     */
    FcStrSet	*cacheDirs;
    /*
     * Names of all of the configuration files used
     * to create this configuration
     */
    FcStrSet	*configFiles;	    /* config files loaded */
    /*
     * Substitution instructions for patterns and fonts;
     * maxObjects is used to allocate appropriate intermediate storage
     * while performing a whole set of substitutions
     */
    FcSubst	*substPattern;	    /* substitutions for patterns */
    FcSubst	*substFont;	    /* substitutions for fonts */
    int		maxObjects;	    /* maximum number of tests in all substs */
    /*
     * List of patterns used to control font file selection
     */
    FcStrSet	*acceptGlobs;
    FcStrSet	*rejectGlobs;
    FcFontSet	*acceptPatterns;
    FcFontSet	*rejectPatterns;
    /*
     * The set of fonts loaded from the listed directories; the
     * order within the set does not determine the font selection,
     * except in the case of identical matches in which case earlier fonts
     * match preferrentially
     */
    FcFontSet	*fonts[FcSetApplication + 1];
    /*
     * Font cache information is mapped from cache files
     * the configuration is destroyed, the files need to be unmapped
     */
    FcCacheList	*caches;
    /*
     * Fontconfig can periodically rescan the system configuration
     * and font directories.  This rescanning occurs when font
     * listing requests are made, but no more often than rescanInterval
     * seconds apart.
     */
    time_t	rescanTime;	    /* last time information was scanned */
    int		rescanInterval;	    /* interval between scans */
};
 
extern FcConfig	*_fcConfig;

typedef struct _FcFileTime {
    time_t  time;
    FcBool  set;
} FcFileTime;

typedef struct _FcCharMap FcCharMap;

/* watch out; assumes that v is void * -PL */
#define ALIGN(v,type) ((void *)(((uintptr_t)(v) + fc_alignof(type) - 1) & ~(fc_alignof(type) - 1)))

/* fcblanks.c */

/* fccache.c */

FcFontSet *
FcCacheRead (FcConfig *config);

FcBool
FcDirCacheWrite (FcFontSet *set, FcStrSet * dirs, const FcChar8 *dir, FcConfig *config);

FcBool
FcDirCacheConsume (FILE *file, FcFontSet *set, FcStrSet *dirs,
		   const FcChar8 *dir, char *dirname);
    
void
FcDirCacheUnmap (FcCache *cache);

FcBool
FcDirCacheRead (FcFontSet * set, FcStrSet * dirs, const FcChar8 *dir, FcConfig *config);
 
FcCache *
FcDirCacheMap (const FcChar8 *dir, FcConfig *config);
    
FcBool
FcDirCacheLoad (int fd, off_t size, void *closure);
    
/* fccfg.c */

FcBool
FcConfigAddConfigDir (FcConfig	    *config,
		      const FcChar8 *d);

FcBool
FcConfigAddFontDir (FcConfig	    *config,
		    const FcChar8   *d);

FcBool
FcConfigAddDir (FcConfig	*config,
		const FcChar8	*d);

FcBool
FcConfigAddCacheDir (FcConfig	    *config,
		     const FcChar8  *d);

FcStrList *
FcConfigGetCacheDirs (FcConfig	*config);

FcBool
FcConfigAddConfigFile (FcConfig		*config,
		       const FcChar8	*f);

FcBool
FcConfigAddBlank (FcConfig	*config,
		  FcChar32    	blank);

FcBool
FcConfigAddEdit (FcConfig	*config,
		 FcTest		*test,
		 FcEdit		*edit,
		 FcMatchKind	kind);

void
FcConfigSetFonts (FcConfig	*config,
		  FcFontSet	*fonts,
		  FcSetName	set);

FcBool
FcConfigCompareValue (const FcValue *m,
		      FcOp	    op,
		      const FcValue *v);

FcBool
FcConfigGlobAdd (FcConfig	*config,
		 const FcChar8	*glob,
		 FcBool		accept);

FcBool
FcConfigAcceptFilename (FcConfig	*config,
			const FcChar8	*filename);

FcBool
FcConfigPatternsAdd (FcConfig	*config,
		     FcPattern	*pattern,
		     FcBool	accept);

FcBool
FcConfigAcceptFont (FcConfig	    *config,
		    const FcPattern *font);

FcFileTime
FcConfigModifiedTime (FcConfig *config);

FcBool
FcConfigAddCache (FcConfig *config, FcCache *cache);

/* fcserialize.c */
intptr_t
FcAlignSize (intptr_t size);
    
FcSerialize *
FcSerializeCreate (void);

void
FcSerializeDestroy (FcSerialize *serialize);

FcBool
FcSerializeAlloc (FcSerialize *serialize, const void *object, int size);

intptr_t
FcSerializeReserve (FcSerialize *serialize, int size);

intptr_t
FcSerializeOffset (FcSerialize *serialize, const void *object);

void *
FcSerializePtr (FcSerialize *serialize, const void *object);

FcBool
FcLangSetSerializeAlloc (FcSerialize *serialize, const FcLangSet *l);

FcLangSet *
FcLangSetSerialize(FcSerialize *serialize, const FcLangSet *l);

/* fccharset.c */
void
FcLangCharSetPopulate (void);

FcCharSet *
FcCharSetFreeze (FcCharSet *cs);

void
FcCharSetThawAll (void);

FcBool
FcNameUnparseCharSet (FcStrBuf *buf, const FcCharSet *c);

FcCharSet *
FcNameParseCharSet (FcChar8 *string);

FcCharLeaf *
FcCharSetFindLeafCreate (FcCharSet *fcs, FcChar32 ucs4);

FcBool
FcCharSetSerializeAlloc(FcSerialize *serialize, const FcCharSet *cs);

FcCharSet *
FcCharSetSerialize(FcSerialize *serialize, const FcCharSet *cs);

FcChar16 *
FcCharSetGetNumbers(const FcCharSet *c);

/* fcdbg.c */
void
FcValueListPrint (const FcValueListPtr l);

void
FcLangSetPrint (const FcLangSet *ls);

void
FcOpPrint (FcOp op);

void
FcTestPrint (const FcTest *test);

void
FcExprPrint (const FcExpr *expr);

void
FcEditPrint (const FcEdit *edit);

void
FcSubstPrint (const FcSubst *subst);

void
FcCharSetPrint (const FcCharSet *c);
    
extern int FcDebugVal;

static inline int
FcDebug (void) { return FcDebugVal; }

void
FcInitDebug (void);

/* fcdefault.c */
FcChar8 *
FcGetDefaultLang (void);

/* fcdir.c */

FcBool
FcFileIsDir (const FcChar8 *file);

FcBool
FcFileScanConfig (FcFontSet	*set,
		  FcStrSet	*dirs,
		  FcBlanks	*blanks,
		  const FcChar8 *file,
		  FcBool	force,
		  FcConfig	*config);

FcBool
FcDirScanConfig (FcFontSet	*set,
		 FcStrSet	*dirs,
		 FcBlanks	*blanks,
		 const FcChar8  *dir,
		 FcBool		force,
		 FcConfig	*config);

/* fcfont.c */
int
FcFontDebug (void);
    
/* fcfreetype.c */
FcBool
FcFreeTypeIsExclusiveLang (const FcChar8  *lang);

FcBool
FcFreeTypeHasLang (FcPattern *pattern, const FcChar8 *lang);

FcChar32
FcFreeTypeUcs4ToPrivate (FcChar32 ucs4, const FcCharMap *map);

FcChar32
FcFreeTypePrivateToUcs4 (FcChar32 private, const FcCharMap *map);

const FcCharMap *
FcFreeTypeGetPrivateMap (FT_Encoding encoding);
    
/* fcfs.c */

FcBool
FcFontSetSerializeAlloc (FcSerialize *serialize, const FcFontSet *s);

FcFontSet *
FcFontSetSerialize (FcSerialize *serialize, const FcFontSet * s);
    
/* fcgram.y */
int
FcConfigparse (void);

int
FcConfigwrap (void);
    
void
FcConfigerror (char *fmt, ...);
    
char *
FcConfigSaveField (const char *field);

void
FcTestDestroy (FcTest *test);

FcExpr *
FcExprCreateInteger (int i);

FcExpr *
FcExprCreateDouble (double d);

FcExpr *
FcExprCreateString (const FcChar8 *s);

FcExpr *
FcExprCreateMatrix (const FcMatrix *m);

FcExpr *
FcExprCreateBool (FcBool b);

FcExpr *
FcExprCreateNil (void);

FcExpr *
FcExprCreateField (const char *field);

FcExpr *
FcExprCreateConst (const FcChar8 *constant);

FcExpr *
FcExprCreateOp (FcExpr *left, FcOp op, FcExpr *right);

void
FcExprDestroy (FcExpr *e);

void
FcEditDestroy (FcEdit *e);

/* fcinit.c */

void
FcMemReport (void);

void
FcMemAlloc (int kind, int size);

void
FcMemFree (int kind, int size);

/* fclang.c */
FcLangSet *
FcFreeTypeLangSet (const FcCharSet  *charset, 
		   const FcChar8    *exclusiveLang);

FcLangResult
FcLangCompare (const FcChar8 *s1, const FcChar8 *s2);
    
const FcCharSet *
FcCharSetForLang (const FcChar8 *lang);

FcLangSet *
FcLangSetPromote (const FcChar8 *lang);

FcLangSet *
FcNameParseLangSet (const FcChar8 *string);

FcBool
FcNameUnparseLangSet (FcStrBuf *buf, const FcLangSet *ls);

/* fclist.c */

FcBool
FcListPatternMatchAny (const FcPattern *p,
		       const FcPattern *font);

/* fcmatch.c */

/* fcname.c */

/*
 * NOTE -- this ordering is part of the cache file format.
 * It must also match the ordering in fcname.c
 */

#define FC_FAMILY_OBJECT	1
#define FC_FAMILYLANG_OBJECT	2
#define FC_STYLE_OBJECT		3
#define FC_STYLELANG_OBJECT	4
#define FC_FULLNAME_OBJECT	5
#define FC_FULLNAMELANG_OBJECT	6
#define FC_SLANT_OBJECT		7
#define FC_WEIGHT_OBJECT	8
#define FC_WIDTH_OBJECT		9
#define FC_SIZE_OBJECT		10
#define FC_ASPECT_OBJECT	11
#define FC_PIXEL_SIZE_OBJECT	12
#define FC_SPACING_OBJECT	13
#define FC_FOUNDRY_OBJECT	14
#define FC_ANTIALIAS_OBJECT	15
#define FC_HINT_STYLE_OBJECT	16
#define FC_HINTING_OBJECT	17
#define FC_VERTICAL_LAYOUT_OBJECT	18
#define FC_AUTOHINT_OBJECT	19
#define FC_GLOBAL_ADVANCE_OBJECT	20
#define FC_FILE_OBJECT		21
#define FC_INDEX_OBJECT		22
#define FC_RASTERIZER_OBJECT	23
#define FC_OUTLINE_OBJECT	24
#define FC_SCALABLE_OBJECT	25
#define FC_DPI_OBJECT		26
#define FC_RGBA_OBJECT		27
#define FC_SCALE_OBJECT		28
#define FC_MINSPACE_OBJECT	29
#define FC_CHAR_WIDTH_OBJECT	30
#define FC_CHAR_HEIGHT_OBJECT	31
#define FC_MATRIX_OBJECT	32
#define FC_CHARSET_OBJECT	33
#define FC_LANG_OBJECT		34
#define FC_FONTVERSION_OBJECT	35
#define FC_CAPABILITY_OBJECT	36
#define FC_FONTFORMAT_OBJECT	37
#define FC_EMBOLDEN_OBJECT	38
#define FC_EMBEDDED_BITMAP_OBJECT	39

FcBool
FcNameBool (const FcChar8 *v, FcBool *result);

FcBool
FcObjectValidType (FcObject object, FcType type);

FcObject
FcObjectFromName (const char * name);

const char *
FcObjectName (FcObject object);

FcBool
FcObjectInit (void);

void
FcObjectFini (void);

#define FcObjectCompare(a, b)	((int) a - (int) b)

/* fcpat.c */

FcValue
FcValueCanonicalize (const FcValue *v);

void
FcValueListDestroy (FcValueListPtr l);

FcPatternElt *
FcPatternObjectFindElt (const FcPattern *p, FcObject object);

FcPatternElt *
FcPatternObjectInsertElt (FcPattern *p, FcObject object);

FcBool
FcPatternObjectAddWithBinding  (FcPattern	*p,
				FcObject	object,
				FcValue		value,
				FcValueBinding  binding,
				FcBool		append);

FcBool
FcPatternObjectAdd (FcPattern *p, FcObject object, FcValue value, FcBool append);
    
FcBool
FcPatternObjectAddWeak (FcPattern *p, FcObject object, FcValue value, FcBool append);
    
FcResult
FcPatternObjectGet (const FcPattern *p, FcObject object, int id, FcValue *v);
    
FcBool
FcPatternObjectDel (FcPattern *p, FcObject object);

FcBool
FcPatternObjectRemove (FcPattern *p, FcObject object, int id);

FcBool
FcPatternObjectAddInteger (FcPattern *p, FcObject object, int i);

FcBool
FcPatternObjectAddDouble (FcPattern *p, FcObject object, double d);

FcBool
FcPatternObjectAddString (FcPattern *p, FcObject object, const FcChar8 *s);

FcBool
FcPatternObjectAddMatrix (FcPattern *p, FcObject object, const FcMatrix *s);

FcBool
FcPatternObjectAddCharSet (FcPattern *p, FcObject object, const FcCharSet *c);

FcBool
FcPatternObjectAddBool (FcPattern *p, FcObject object, FcBool b);

FcBool
FcPatternObjectAddLangSet (FcPattern *p, FcObject object, const FcLangSet *ls);

FcResult
FcPatternObjectGetInteger (const FcPattern *p, FcObject object, int n, int *i);

FcResult
FcPatternObjectGetDouble (const FcPattern *p, FcObject object, int n, double *d);

FcResult
FcPatternObjectGetString (const FcPattern *p, FcObject object, int n, FcChar8 ** s);

FcResult
FcPatternObjectGetMatrix (const FcPattern *p, FcObject object, int n, FcMatrix **s);

FcResult
FcPatternObjectGetCharSet (const FcPattern *p, FcObject object, int n, FcCharSet **c);

FcResult
FcPatternObjectGetBool (const FcPattern *p, FcObject object, int n, FcBool *b);

FcResult
FcPatternObjectGetLangSet (const FcPattern *p, FcObject object, int n, FcLangSet **ls);

void
FcPatternFini (void);

FcBool
FcPatternAppend (FcPattern *p, FcPattern *s);

const FcChar8 *
FcStrStaticName (const FcChar8 *name);

FcChar32
FcStringHash (const FcChar8 *s);

FcBool
FcPatternSerializeAlloc (FcSerialize *serialize, const FcPattern *pat);

FcPattern *
FcPatternSerialize (FcSerialize *serialize, const FcPattern *pat);

FcBool
FcValueListSerializeAlloc (FcSerialize *serialize, const FcValueList *pat);

FcValueList *
FcValueListSerialize (FcSerialize *serialize, const FcValueList *pat);

/* fcrender.c */

/* fcmatrix.c */

extern const FcMatrix    FcIdentityMatrix;

void
FcMatrixFree (FcMatrix *mat);

/* fcstr.c */
void
FcStrSetSort (FcStrSet * set);

FcChar8 *
FcStrPlus (const FcChar8 *s1, const FcChar8 *s2);
    
void
FcStrFree (FcChar8 *s);

void
FcStrBufInit (FcStrBuf *buf, FcChar8 *init, int size);

void
FcStrBufDestroy (FcStrBuf *buf);

FcChar8 *
FcStrBufDone (FcStrBuf *buf);

FcBool
FcStrBufChar (FcStrBuf *buf, FcChar8 c);

FcBool
FcStrBufString (FcStrBuf *buf, const FcChar8 *s);

FcBool
FcStrBufData (FcStrBuf *buf, const FcChar8 *s, int len);

int
FcStrCmpIgnoreBlanksAndCase (const FcChar8 *s1, const FcChar8 *s2);

const FcChar8 *
FcStrContainsIgnoreBlanksAndCase (const FcChar8 *s1, const FcChar8 *s2);

const FcChar8 *
FcStrContainsIgnoreCase (const FcChar8 *s1, const FcChar8 *s2);

FcBool
FcStrUsesHome (const FcChar8 *s);

FcChar8 *
FcStrLastSlash (const FcChar8  *path);

FcChar32
FcStrHashIgnoreCase (const FcChar8 *s);

FcChar8 *
FcStrCanonFilename (const FcChar8 *s);

FcBool
FcStrSerializeAlloc (FcSerialize *serialize, const FcChar8 *str);

FcChar8 *
FcStrSerialize (FcSerialize *serialize, const FcChar8 *str);

#endif /* _FC_INT_H_ */
