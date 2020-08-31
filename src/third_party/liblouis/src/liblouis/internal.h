/* liblouis Braille Translation and Back-Translation Library

   Based on the Linux screenreader BRLTTY, copyright (C) 1999-2006 by The
   BRLTTY Team

   Copyright (C) 2004, 2005, 2006 ViewPlus Technologies, Inc. www.viewplus.com
   Copyright (C) 2004, 2005, 2006 JJB Software, Inc. www.jjb-software.com
   Copyright (C) 2016 Mike Gray, American Printing House for the Blind
   Copyright (C) 2016 Davy Kager, Dedicon

   This file is part of liblouis.

   liblouis is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published
   by the Free Software Foundation, either version 2.1 of the License, or
   (at your option) any later version.

   liblouis is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with liblouis. If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file
 * @brief Internal API of liblouis
 */

#ifndef __LOUIS_H_
#define __LOUIS_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdio.h>
#include "liblouis.h"

#ifdef _WIN32
#define PATH_SEP ';'
#define DIR_SEP '\\'
#else
#define PATH_SEP ':'
#define DIR_SEP '/'
#endif

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

#define NUMSWAPS 50
#define NUMVAR 50
#define LETSIGNSIZE 128
#define SEQPATTERNSIZE 128
#define CHARSIZE sizeof(widechar)
#define DEFAULTRULESIZE 50

typedef struct intCharTupple {
	int key;
	char value;
} intCharTupple;

/* HASHNUM must be prime */
#define HASHNUM 1123

#define MAXPASS 4
#define MAXSTRING 2048

#define MAX_EMPH_CLASSES 10  // {emph_1...emph_10} in typeforms enum (liblouis.h)

typedef unsigned int TranslationTableOffset;
#define OFFSETSIZE sizeof(TranslationTableOffset)

typedef enum {
	CTC_Space = 0x1,
	CTC_Letter = 0x2,
	CTC_Digit = 0x4,
	CTC_Punctuation = 0x8,
	CTC_UpperCase = 0x10,
	CTC_LowerCase = 0x20,
	CTC_Math = 0x40,
	CTC_Sign = 0x80,
	CTC_LitDigit = 0x100,
	CTC_Class1 = 0x200,
	CTC_Class2 = 0x400,
	CTC_Class3 = 0x800,
	CTC_Class4 = 0x1000,
	CTC_SeqDelimiter = 0x2000,
	CTC_SeqBefore = 0x4000,
	CTC_SeqAfter = 0x8000,
	CTC_UserDefined0 = 0x10000,  // class 5
	CTC_UserDefined1 = 0x20000,
	CTC_UserDefined2 = 0x40000,
	CTC_UserDefined3 = 0x80000,
	CTC_UserDefined4 = 0x100000,
	CTC_UserDefined5 = 0x200000,
	CTC_UserDefined6 = 0x400000,
	CTC_UserDefined7 = 0x800000,  // class 12
	CTC_CapsMode = 0x1000000,
	CTC_EmphMode = 0x2000000,
	CTC_NumericMode = 0x4000000,
	CTC_NumericNoContract = 0x8000000,
	CTC_EndOfInput = 0x10000000,  // only used by pattern matcher
	CTC_EmpMatch = 0x20000000,	// only used in TranslationTableRule->before and
								  // TranslationTableRule->after
	CTC_MidEndNumericMode = 0x40000000,
	// 33 more bits available in a unsigned long long (at least 64 bits)
	// currently used for classes 13 to 45
	CTC_Class13 = 0x80000000,
} TranslationTableCharacterAttribute;

typedef enum {
	pass_first = '`',
	pass_last = '~',
	pass_lookback = '_',
	pass_string = '\"',
	pass_dots = '@',
	pass_omit = '?',
	pass_startReplace = '[',
	pass_endReplace = ']',
	pass_startGroup = '{',
	pass_endGroup = '}',
	pass_variable = '#',
	pass_not = '!',
	pass_search = '/',
	pass_any = 'a',
	pass_digit = 'd',
	pass_litDigit = 'D',
	pass_letter = 'l',
	pass_math = 'm',
	pass_punctuation = 'p',
	pass_sign = 'S',
	pass_space = 's',
	pass_uppercase = 'U',
	pass_lowercase = 'u',
	pass_class1 = 'w',
	pass_class2 = 'x',
	pass_class3 = 'y',
	pass_class4 = 'z',
	pass_attributes = '$',
	pass_groupstart = '{',
	pass_groupend = '}',
	pass_groupreplace = ';',
	pass_swap = '%',
	pass_hyphen = '-',
	pass_until = '.',
	pass_eq = '=',
	pass_lt = '<',
	pass_gt = '>',
	pass_endTest = 32,
	pass_plus = '+',
	pass_copy = '*',
	pass_leftParen = '(',
	pass_rightParen = ')',
	pass_comma = ',',
	pass_lteq = 130,
	pass_gteq = 131,
	pass_invalidToken = 132,
	pass_noteq = 133,
	pass_and = 134,
	pass_or = 135,
	pass_nameFound = 136,
	pass_numberFound = 137,
	pass_boolean = 138,
	pass_class = 139,
	pass_define = 140,
	pass_emphasis = 141,
	pass_group = 142,
	pass_mark = 143,
	pass_repGroup = 143,
	pass_script = 144,
	pass_noMoreTokens = 145,
	pass_replace = 146,
	pass_if = 147,
	pass_then = 148,
	pass_all = 255
} pass_Codes;

typedef unsigned long long TranslationTableCharacterAttributes;

typedef struct {
	TranslationTableOffset next;
	widechar lookFor;
	widechar found;
} CharOrDots;

typedef struct {
	TranslationTableOffset next;
	TranslationTableOffset definitionRule;
	TranslationTableOffset otherRules;
	TranslationTableCharacterAttributes attributes;
	widechar realchar;
	widechar uppercase;
	widechar lowercase;
} TranslationTableCharacter;

typedef enum { /* Op codes */
	CTO_IncludeFile,
	CTO_Locale, /* Deprecated, do not use */
	CTO_Undefined,
	/* Do not change the order of the following opcodes! */
	CTO_CapsLetter,
	CTO_BegCapsWord,
	CTO_EndCapsWord,
	CTO_BegCaps,
	CTO_EndCaps,
	CTO_BegCapsPhrase,
	CTO_EndCapsPhrase,
	CTO_LenCapsPhrase,
	/* End of ordered opcodes */
	CTO_LetterSign,
	CTO_NoLetsignBefore,
	CTO_NoLetsign,
	CTO_NoLetsignAfter,
	CTO_NumberSign,
	CTO_NumericModeChars,
	CTO_MidEndNumericModeChars,
	CTO_NumericNoContractChars,
	CTO_SeqDelimiter,
	CTO_SeqBeforeChars,
	CTO_SeqAfterChars,
	CTO_SeqAfterPattern,
	CTO_SeqAfterExpression,
	CTO_EmphClass,

	/* Do not change the order of the following opcodes! */
	CTO_EmphLetter,
	CTO_BegEmphWord,
	CTO_EndEmphWord,
	CTO_BegEmph,
	CTO_EndEmph,
	CTO_BegEmphPhrase,
	CTO_EndEmphPhrase,
	CTO_LenEmphPhrase,
	/* End of ordered opcodes */

	CTO_CapsModeChars,
	CTO_EmphModeChars,
	CTO_BegComp,
	CTO_CompBegEmph1,
	CTO_CompEndEmph1,
	CTO_CompBegEmph2,
	CTO_CompEndEmph2,
	CTO_CompBegEmph3,
	CTO_CompEndEmph3,
	CTO_CompCapSign,
	CTO_CompBegCaps,
	CTO_CompEndCaps,
	CTO_EndComp,
	CTO_NoContractSign,
	CTO_MultInd,
	CTO_CompDots,
	CTO_Comp6,
	CTO_Class,  /* define a character class */
	CTO_After,  /* only match if after character in class */
	CTO_Before, /* only match if before character in class 30 */
	CTO_NoBack,
	CTO_NoFor,
	CTO_EmpMatchBefore,
	CTO_EmpMatchAfter,
	CTO_SwapCc,
	CTO_SwapCd,
	CTO_SwapDd,
	CTO_Space,
	CTO_Digit,
	CTO_Punctuation,
	CTO_Math,
	CTO_Sign,
	CTO_Letter,
	CTO_UpperCase,
	CTO_LowerCase,
	CTO_Grouping,
	CTO_UpLow,
	CTO_LitDigit,
	CTO_Display,
	CTO_Replace,
	CTO_Context,
	CTO_Correct,
	CTO_Pass2,
	CTO_Pass3,
	CTO_Pass4,
	CTO_Repeated,
	CTO_RepWord,
	CTO_CapsNoCont,
	CTO_Always,
	CTO_ExactDots,
	CTO_NoCross,
	CTO_Syllable,
	CTO_NoCont,
	CTO_CompBrl,
	CTO_Literal,
	CTO_LargeSign,
	CTO_WholeWord,
	CTO_PartWord,
	CTO_JoinNum,
	CTO_JoinableWord,
	CTO_LowWord,
	CTO_Contraction,
	CTO_SuffixableWord, /** whole word or beginning of word */
	CTO_PrefixableWord, /** whole word or end of word */
	CTO_BegWord,		/** beginning of word only */
	CTO_BegMidWord,		/** beginning or middle of word */
	CTO_MidWord,		/** middle of word only 20 */
	CTO_MidEndWord,		/** middle or end of word */
	CTO_EndWord,		/** end of word only */
	CTO_PrePunc,		/** punctuation in string at beginning of word */
	CTO_PostPunc,		/** punctuation in string at end of word */
	CTO_BegNum,			/** beginning of number */
	CTO_MidNum,			/** middle of number, e.g., decimal point */
	CTO_EndNum,			/** end of number */
	CTO_DecPoint,
	CTO_Hyphen,
	// CTO_Apostrophe,
	// CTO_Initial,
	CTO_NoBreak,
	CTO_Match,
	CTO_BackMatch,
	CTO_Attribute,
	CTO_None,

	/* More internal opcodes */
	CTO_LetterRule,
	CTO_NumberRule,
	CTO_NoContractRule,

	/* Start of (11 x 9) internal opcodes values that match
	 * {"singlelettercaps"..."lenemphphrase"}
	 * Do not change the order of the following opcodes! */
	CTO_CapsLetterRule,
	CTO_BegCapsWordRule,
	CTO_EndCapsWordRule,
	CTO_BegCapsRule,
	CTO_EndCapsRule,
	CTO_BegCapsPhraseRule,
	CTO_EndCapsPhraseBeforeRule,
	CTO_EndCapsPhraseAfterRule,
	CTO_Emph1LetterRule,
	CTO_BegEmph1WordRule,
	CTO_EndEmph1WordRule,
	CTO_BegEmph1Rule,
	CTO_EndEmph1Rule,
	CTO_BegEmph1PhraseRule,
	CTO_EndEmph1PhraseBeforeRule,
	CTO_EndEmph1PhraseAfterRule,
	CTO_Emph2LetterRule,
	CTO_BegEmph2WordRule,
	CTO_EndEmph2WordRule,
	CTO_BegEmph2Rule,
	CTO_EndEmph2Rule,
	CTO_BegEmph2PhraseRule,
	CTO_EndEmph2PhraseBeforeRule,
	CTO_EndEmph2PhraseAfterRule,
	CTO_Emph3LetterRule,
	CTO_BegEmph3WordRule,
	CTO_EndEmph3WordRule,
	CTO_BegEmph3Rule,
	CTO_EndEmph3Rule,
	CTO_BegEmph3PhraseRule,
	CTO_EndEmph3PhraseBeforeRule,
	CTO_EndEmph3PhraseAfterRule,
	CTO_Emph4LetterRule,
	CTO_BegEmph4WordRule,
	CTO_EndEmph4WordRule,
	CTO_BegEmph4Rule,
	CTO_EndEmph4Rule,
	CTO_BegEmph4PhraseRule,
	CTO_EndEmph4PhraseBeforeRule,
	CTO_EndEmph4PhraseAfterRule,
	CTO_Emph5LetterRule,
	CTO_BegEmph5WordRule,
	CTO_EndEmph5WordRule,
	CTO_BegEmph5Rule,
	CTO_EndEmph5Rule,
	CTO_BegEmph5PhraseRule,
	CTO_EndEmph5PhraseBeforeRule,
	CTO_EndEmph5PhraseAfterRule,
	CTO_Emph6LetterRule,
	CTO_BegEmph6WordRule,
	CTO_EndEmph6WordRule,
	CTO_BegEmph6Rule,
	CTO_EndEmph6Rule,
	CTO_BegEmph6PhraseRule,
	CTO_EndEmph6PhraseBeforeRule,
	CTO_EndEmph6PhraseAfterRule,
	CTO_Emph7LetterRule,
	CTO_BegEmph7WordRule,
	CTO_EndEmph7WordRule,
	CTO_BegEmph7Rule,
	CTO_EndEmph7Rule,
	CTO_BegEmph7PhraseRule,
	CTO_EndEmph7PhraseBeforeRule,
	CTO_EndEmph7PhraseAfterRule,
	CTO_Emph8LetterRule,
	CTO_BegEmph8WordRule,
	CTO_EndEmph8WordRule,
	CTO_BegEmph8Rule,
	CTO_EndEmph8Rule,
	CTO_BegEmph8PhraseRule,
	CTO_EndEmph8PhraseBeforeRule,
	CTO_EndEmph8PhraseAfterRule,
	CTO_Emph9LetterRule,
	CTO_BegEmph9WordRule,
	CTO_EndEmph9WordRule,
	CTO_BegEmph9Rule,
	CTO_EndEmph9Rule,
	CTO_BegEmph9PhraseRule,
	CTO_EndEmph9PhraseBeforeRule,
	CTO_EndEmph9PhraseAfterRule,
	CTO_Emph10LetterRule,
	CTO_BegEmph10WordRule,
	CTO_EndEmph10WordRule,
	CTO_BegEmph10Rule,
	CTO_EndEmph10Rule,
	CTO_BegEmph10PhraseRule,
	CTO_EndEmph10PhraseBeforeRule,
	CTO_EndEmph10PhraseAfterRule,
	/* End of ordered (10 x 9) internal opcodes */

	CTO_BegCompRule,
	CTO_CompBegEmph1Rule,
	CTO_CompEndEmph1Rule,
	CTO_CompBegEmph2Rule,
	CTO_CompEndEmrh2Rule,
	CTO_CompBegEmph3Rule,
	CTO_CompEndEmph3Rule,
	CTO_CompCapSignRule,
	CTO_CompBegCapsRule,
	CTO_CompEndCapsRule,
	CTO_EndCompRule,
	CTO_CapsNoContRule,
	CTO_All
} TranslationTableOpcode;

typedef struct {
	TranslationTableOffset charsnext;			/** next chars entry */
	TranslationTableOffset dotsnext;			/** next dots entry */
	TranslationTableCharacterAttributes after;  /** character types which must follow */
	TranslationTableCharacterAttributes before; /** character types which must precede */
	TranslationTableOffset patterns;			/** before and after patterns */
	TranslationTableOpcode opcode;		 /** rule for testing validity of replacement */
	short charslen;						 /** length of string to be replaced */
	short dotslen;						 /** length of replacement string */
	widechar charsdots[DEFAULTRULESIZE]; /** find and replacement strings */
} TranslationTableRule;

typedef struct /* state transition */
{
	widechar ch;
	widechar newState;
} HyphenationTrans;

typedef union {
	HyphenationTrans *pointer;
	TranslationTableOffset offset;
} PointOff;

typedef struct /* one state */
{
	PointOff trans;
	TranslationTableOffset hyphenPattern;
	widechar fallbackState;
	widechar numTrans;
} HyphenationState;

typedef struct CharacterClass {
	struct CharacterClass *next;
	TranslationTableCharacterAttributes attribute;
	widechar length;
	widechar name[1];
} CharacterClass;

typedef struct RuleName {
	struct RuleName *next;
	TranslationTableOffset ruleOffset;
	widechar length;
	widechar name[1];
} RuleName;

typedef struct {
	TranslationTableOffset tableSize;
	TranslationTableOffset bytesUsed;
	TranslationTableOffset charToDots[HASHNUM];
	TranslationTableOffset dotsToChar[HASHNUM];
	TranslationTableOffset ruleArea[1]; /** Space for storing all rules and values */
} DisplayTableHeader;

/**
 * Translation table header
 */
typedef struct { /* translation table */
	int capsNoCont;
	int numPasses;
	int corrections;
	int syllables;
	int usesSequences;
	int usesNumericMode;
	int usesEmphMode;
	TranslationTableOffset tableSize;
	TranslationTableOffset bytesUsed;
	TranslationTableOffset undefined;
	TranslationTableOffset letterSign;
	TranslationTableOffset numberSign;
	TranslationTableOffset noContractSign;
	widechar seqPatterns[SEQPATTERNSIZE];
	char *emphClasses[MAX_EMPH_CLASSES + 1];
	int seqPatternsCount;
	widechar seqAfterExpression[SEQPATTERNSIZE];
	int seqAfterExpressionLength;

	/* emphRules, including caps. */
	TranslationTableOffset emphRules[MAX_EMPH_CLASSES + 1][9];

	/* state needed during compilation */
	CharacterClass *characterClasses;
	TranslationTableCharacterAttributes nextCharacterClassAttribute;
	RuleName *ruleNames;

	TranslationTableOffset begComp;
	TranslationTableOffset compBegEmph1;
	TranslationTableOffset compEndEmph1;
	TranslationTableOffset compBegEmph2;
	TranslationTableOffset compEndEmph2;
	TranslationTableOffset compBegEmph3;
	TranslationTableOffset compEndEmph3;
	TranslationTableOffset compCapSign;
	TranslationTableOffset compBegCaps;
	TranslationTableOffset compEndCaps;
	TranslationTableOffset endComp;
	TranslationTableOffset hyphenStatesArray;
	widechar noLetsignBefore[LETSIGNSIZE];
	int noLetsignBeforeCount;
	widechar noLetsign[LETSIGNSIZE];
	int noLetsignCount;
	widechar noLetsignAfter[LETSIGNSIZE];
	int noLetsignAfterCount;
	TranslationTableOffset characters[HASHNUM]; /** Character definitions */
	TranslationTableOffset dots[HASHNUM];		/** Dot definitions */
	TranslationTableOffset compdotsPattern[256];
	TranslationTableOffset swapDefinitions[NUMSWAPS];
	TranslationTableOffset forPassRules[MAXPASS + 1];
	TranslationTableOffset backPassRules[MAXPASS + 1];
	TranslationTableOffset forRules[HASHNUM];  /** chains of forward rules */
	TranslationTableOffset backRules[HASHNUM]; /** Chains of backward rules */
	TranslationTableOffset ruleArea[1]; /** Space for storing all rules and values */
} TranslationTableHeader;

typedef enum {
	alloc_typebuf,
	alloc_wordBuffer,
	alloc_emphasisBuffer,
	alloc_destSpacing,
	alloc_passbuf,
	alloc_posMapping1,
	alloc_posMapping2,
	alloc_posMapping3
} AllocBuf;

#define MAXPASSBUF 3

typedef enum {
	capsRule = 0,
	emph1Rule = 1,
	emph2Rule = 2,
	emph3Rule = 3,
	emph4Rule = 4,
	emph5Rule = 5,
	emph6Rule = 6,
	emph7Rule = 7,
	emph8Rule = 8,
	emph9Rule = 9,
	emph10Rule = 10
} EmphRuleNumber;

typedef enum {
	begPhraseOffset = 0,
	endPhraseBeforeOffset = 1,
	endPhraseAfterOffset = 2,
	begOffset = 3,
	endOffset = 4,
	letterOffset = 5,
	begWordOffset = 6,
	endWordOffset = 7,
	lenPhraseOffset = 8
} EmphCodeOffset;

/* Grouping the begin, end, word and symbol bits and using the type of
 * a single bit group for representing the emphasis classes allows us
 * to do simple bit operations. */

typedef struct {
	unsigned int begin : 16;
	unsigned int end : 16;
	unsigned int word : 16;
	unsigned int symbol : 16;
} EmphasisInfo;

/* An emphasis class is a bit field that contains a single "1" */
typedef unsigned int EmphasisClass;

typedef enum { noEncoding, bigEndian, littleEndian, ascii8 } EncodingType;

typedef struct {
	const char *fileName;
	FILE *in;
	int lineNumber;
	EncodingType encoding;
	int status;
	int linelen;
	int linepos;
	int checkencoding[2];
	widechar line[MAXSTRING];
} FileInfo;

/* The following function definitions are hooks into
 * compileTranslationTable.c. Some are used by other library modules.
 * Others are used by tools like lou_allround.c and lou_debug.c. */

/**
 * Comma separated list of directories to search for tables.
 */
char *EXPORT_CALL
_lou_getTablePath(void);

/**
 * Resolve tableList against base.
 */
char **EXPORT_CALL
_lou_resolveTable(const char *tableList, const char *base);

/**
 * The default table resolver
 */
char **EXPORT_CALL
_lou_defaultTableResolver(const char *tableList, const char *base);

/**
 * Return single-cell dot pattern corresponding to a character.
 * TODO: move to commonTranslationFunctions.c
 */
widechar EXPORT_CALL
_lou_getDotsForChar(widechar c, const DisplayTableHeader *table);

/**
 * Return character corresponding to a single-cell dot pattern.
 * TODO: move to commonTranslationFunctions.c
 */
widechar EXPORT_CALL
_lou_getCharFromDots(widechar d, const DisplayTableHeader *table);

void EXPORT_CALL
_lou_getTable(const char *tableList, const char *displayTableList,
		const TranslationTableHeader **translationTable,
		const DisplayTableHeader **displayTable);

const TranslationTableHeader *EXPORT_CALL
_lou_getTranslationTable(const char *tableList);

const DisplayTableHeader *EXPORT_CALL
_lou_getDisplayTable(const char *tableList);

int EXPORT_CALL
_lou_compileTranslationRule(const char *tableList, const char *inString);

int EXPORT_CALL
_lou_compileDisplayRule(const char *tableList, const char *inString);

/**
 * Allocate memory for internal buffers
 *
 * Used by lou_translateString.c and lou_backTranslateString.c ONLY
 * to allocate memory for internal buffers.
 * TODO: move to utils.c
 */
void *EXPORT_CALL
_lou_allocMem(AllocBuf buffer, int index, int srcmax, int destmax);

/**
 * Hash function for character strings
 *
 * @param lowercase Whether to convert the string to lowercase because
 *                  making the hash of it.
 */
unsigned long int EXPORT_CALL
_lou_stringHash(const widechar *c, int lowercase, const TranslationTableHeader *table);

/**
 * Hash function for single characters
 */
unsigned long int EXPORT_CALL
_lou_charHash(widechar c);

/**
 * Return a string in the same format as the characters operand in opcodes
 */
const char *EXPORT_CALL
_lou_showString(widechar const *chars, int length, int forceHex);

/**
 * Print out dot numbers
 *
 * @return a string containing the dot numbers. The longest possible
 * output is "\123456789ABCDEF0/"
 */
const char *EXPORT_CALL
_lou_unknownDots(widechar dots);

/**
 * Return a character string in the format of the dots operand
 */
const char *EXPORT_CALL
_lou_showDots(widechar const *dots, int length);

/**
 * Return a character string where the attributes are indicated
 * by the attribute letters used in multipass opcodes
 */
char *EXPORT_CALL
_lou_showAttributes(TranslationTableCharacterAttributes a);

/**
 * Return number of the opcode
 *
 * @param toFind the opcodes
 */
TranslationTableOpcode EXPORT_CALL
_lou_findOpcodeNumber(const char *tofind);

/**
 * Return the name of the opcode associated with an opcode number
 *
 * @param opcode an opcode
 */
const char *EXPORT_CALL
_lou_findOpcodeName(TranslationTableOpcode opcode);

/**
 * Convert string to wide characters
 *
 * Takes a character string and produces a sequence of wide characters.
 * Opposite of _lou_showString.
 *
 * @param inString the input string
 * @param outString the output wide char sequence
 * @return length of the widechar sequence.
 */
int EXPORT_CALL
_lou_extParseChars(const char *inString, widechar *outString);

/**
 * Convert string to wide characters containing dot patterns
 *
 * Takes a character string and produces a sequence of wide characters
 * containing dot patterns. Opposite of _lou_showDots.
 * @param inString the input string
 * @param outString the output wide char sequence
 * @return length of the widechar sequence.
 */
int EXPORT_CALL
_lou_extParseDots(const char *inString, widechar *outString);

int EXPORT_CALL
_lou_translate(const char *tableList, const char *displayTableList, const widechar *inbuf,
		int *inlen, widechar *outbuf, int *outlen, formtype *typeform, char *spacing,
		int *outputPos, int *inputPos, int *cursorPos, int mode,
		const TranslationTableRule **rules, int *rulesLen);

int EXPORT_CALL
_lou_backTranslate(const char *tableList, const char *displayTableList,
		const widechar *inbuf, int *inlen, widechar *outbuf, int *outlen,
		formtype *typeform, char *spacing, int *outputPos, int *inputPos, int *cursorPos,
		int mode, const TranslationTableRule **rules, int *rulesLen);

void EXPORT_CALL
_lou_resetPassVariables(void);

int EXPORT_CALL
_lou_handlePassVariableTest(const widechar *instructions, int *IC, int *itsTrue);

int EXPORT_CALL
_lou_handlePassVariableAction(const widechar *instructions, int *IC);

int EXPORT_CALL
_lou_pattern_compile(const widechar *input, const int input_max, widechar *expr_data,
		const int expr_max, const TranslationTableHeader *t);

void EXPORT_CALL
_lou_pattern_reverse(widechar *expr_data);

int EXPORT_CALL
_lou_pattern_check(const widechar *input, const int input_start, const int input_minmax,
		const int input_dir, const widechar *expr_data, const TranslationTableHeader *t);

/**
 * Read a line of widechar's from an input file
 */
int EXPORT_CALL
_lou_getALine(FileInfo *info);

#ifdef DEBUG
/* Can be inserted in code to be used as a breakpoint in gdb */
void EXPORT_CALL
_lou_debugHook(void);
#endif

/**
 * Print an out-of-memory message and exit
 */
void EXPORT_CALL
_lou_outOfMemory(void);

/**
 * Helper for logging a widechar buffer
 */
void EXPORT_CALL
_lou_logWidecharBuf(logLevels level, const char *msg, const widechar *wbuf, int wlen);

void EXPORT_CALL
_lou_logMessage(logLevels level, const char *format, ...);

extern int translation_direction;

/**
 * Return 1 if given translation mode is valid. Return 0 otherwise.
 */
int EXPORT_CALL
_lou_isValidMode(int mode);

/**
 * Return the default braille representation for a character.
 */
widechar EXPORT_CALL
_lou_charToFallbackDots(widechar c);

static inline int
isASCII(widechar c) {
	return (c >= 0X20) && (c < 0X7F);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __LOUIS_H_ */
