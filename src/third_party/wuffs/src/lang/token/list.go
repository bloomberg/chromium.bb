// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package token

// MaxIntBits is the largest size (in bits) of the i8, u8, i16, u16, etc.
// integer types.
const MaxIntBits = 64

// ID is a token type. Every identifier (in the programming language sense),
// keyword, operator and literal has its own ID.
//
// Some IDs are built-in: the "func" keyword always has the same numerical ID
// value. Others are mapped at runtime. For example, the ID value for the
// "foobar" identifier (e.g. a variable name) is looked up in a Map.
type ID uint32

// Str returns a string form of x.
func (x ID) Str(m *Map) string { return m.ByID(x) }

func (x ID) AmbiguousForm() ID {
	if x >= ID(len(ambiguousForms)) {
		return 0
	}
	return ambiguousForms[x]
}

func (x ID) UnaryForm() ID {
	if x >= ID(len(unaryForms)) {
		return 0
	}
	return unaryForms[x]
}

func (x ID) BinaryForm() ID {
	if x >= ID(len(binaryForms)) {
		return 0
	}
	return binaryForms[x]
}

func (x ID) AssociativeForm() ID {
	if x >= ID(len(associativeForms)) {
		return 0
	}
	return associativeForms[x]
}

func (x ID) IsBuiltIn() bool { return x < nBuiltInIDs }

func (x ID) IsUnaryOp() bool       { return minOp <= x && x <= maxOp && unaryForms[x] != 0 }
func (x ID) IsBinaryOp() bool      { return minOp <= x && x <= maxOp && binaryForms[x] != 0 }
func (x ID) IsAssociativeOp() bool { return minOp <= x && x <= maxOp && associativeForms[x] != 0 }

func (x ID) IsLiteral(m *Map) bool {
	if x < nBuiltInIDs {
		return minBuiltInLiteral <= x && x <= maxBuiltInLiteral
	} else if s := m.ByID(x); s != "" {
		return !alpha(s[0])
	}
	return false
}

func (x ID) IsNumLiteral(m *Map) bool {
	if x < nBuiltInIDs {
		return minBuiltInNumLiteral <= x && x <= maxBuiltInNumLiteral
	} else if s := m.ByID(x); s != "" {
		return numeric(s[0])
	}
	return false
}

// IsDQStrLiteral returns whether x is a double-quote string literal.
func (x ID) IsDQStrLiteral(m *Map) bool {
	if x < nBuiltInIDs {
		return false
	} else if s := m.ByID(x); s != "" {
		return s[0] == '"'
	}
	return false
}

// IsSQStrLiteral returns whether x is a single-quote string literal.
func (x ID) IsSQStrLiteral(m *Map) bool {
	if x < nBuiltInIDs {
		return false
	} else if s := m.ByID(x); s != "" {
		return s[0] == '\''
	}
	return false
}

func (x ID) IsIdent(m *Map) bool {
	if x < nBuiltInIDs {
		return minBuiltInIdent <= x && x <= maxBuiltInIdent
	} else if s := m.ByID(x); s != "" {
		return alpha(s[0])
	}
	return false
}

func (x ID) IsTightLeft() bool  { return x < ID(len(isTightLeft)) && isTightLeft[x] }
func (x ID) IsTightRight() bool { return x < ID(len(isTightRight)) && isTightRight[x] }

func (x ID) IsAssign() bool         { return minAssign <= x && x <= maxAssign }
func (x ID) IsCannotAssignTo() bool { return minCannotAssignTo <= x && x <= maxCannotAssignTo }
func (x ID) IsClose() bool          { return minClose <= x && x <= maxClose }
func (x ID) IsKeyword() bool        { return minKeyword <= x && x <= maxKeyword }
func (x ID) IsNumType() bool        { return minNumType <= x && x <= maxNumType }
func (x ID) IsNumTypeOrIdeal() bool { return minNumTypeOrIdeal <= x && x <= maxNumTypeOrIdeal }
func (x ID) IsOpen() bool           { return minOpen <= x && x <= maxOpen }

func (x ID) IsImplicitSemicolon(m *Map) bool {
	return x.IsClose() || x.IsKeyword() || x.IsIdent(m) || x.IsLiteral(m)
}

func (x ID) IsXOp() bool            { return minXOp <= x && x <= maxXOp }
func (x ID) IsXUnaryOp() bool       { return minXOp <= x && x <= maxXOp && unaryForms[x] != 0 }
func (x ID) IsXBinaryOp() bool      { return minXOp <= x && x <= maxXOp && binaryForms[x] != 0 }
func (x ID) IsXAssociativeOp() bool { return minXOp <= x && x <= maxXOp && associativeForms[x] != 0 }

func (x ID) SmallPowerOf2Value() int {
	switch x {
	case ID1:
		return 1
	case ID2:
		return 2
	case ID4:
		return 4
	case ID8:
		return 8
	case ID16:
		return 16
	case ID32:
		return 32
	case ID64:
		return 64
	case ID128:
		return 128
	case ID256:
		return 256
	}
	return 0
}

// QID is a qualified ID, such as "foo.bar". QID[0] is "foo"'s ID and QID[1] is
// "bar"'s. QID[0] may be 0 for a plain "bar".
type QID [2]ID

func (x QID) IsZero() bool { return x == QID{} }

func (x QID) LessThan(y QID) bool {
	if x[0] != y[0] {
		return x[0] < y[0]
	}
	return x[1] < y[1]
}

// Str returns a string form of x.
func (x QID) Str(m *Map) string {
	if x[0] != 0 {
		return m.ByID(x[0]) + "." + m.ByID(x[1])
	}
	return m.ByID(x[1])
}

// QQID is a double-qualified ID, such as "receiverPkg.receiverName.funcName".
type QQID [3]ID

func (x QQID) IsZero() bool { return x == QQID{} }

func (x QQID) LessThan(y QQID) bool {
	if x[0] != y[0] {
		return x[0] < y[0]
	}
	if x[1] != y[1] {
		return x[1] < y[1]
	}
	return x[2] < y[2]
}

// Str returns a string form of x.
func (x QQID) Str(m *Map) string {
	if x[0] != 0 {
		return m.ByID(x[0]) + "." + m.ByID(x[1]) + "." + m.ByID(x[2])
	}
	if x[1] != 0 {
		return m.ByID(x[1]) + "." + m.ByID(x[2])
	}
	return m.ByID(x[2])
}

// Token combines an ID and the line number it was seen.
type Token struct {
	ID   ID
	Line uint32
}

// nBuiltInIDs is the number of built-in IDs. The packing is:
//  -            0x00 is invalid.
//  -  0x01 ..=  0x0F are squiggly punctuation, such as ";", "." and "?".
//  -  0x10 ..=  0x1F are squiggly bookends, such as "(", ")" and "]".
//  -  0x20 ..=  0x3F are squiggly assignments, such as "=" and "+=".
//  -  0x40 ..=  0x6F are operators, such as "+", "==" and "not".
//  -  0x70 ..=  0xAF are x-ops (disambiguation forms): unary vs binary "+".
//  -  0xB0 ..=  0xCF are keywords, such as "if" and "return".
//  -  0xD0 ..=  0xDF are type modifiers, such as "ptr" and "slice".
//  -  0xE0 ..=  0xFF are literals, such as "ok" and "true".
//  - 0x100 ..= 0x3FF are identifiers, such as "bool", "u32" and "read_u8".
//
// Squiggly means a sequence of non-alpha-numeric bytes, such as "+" and "&=".
const (
	nBuiltInSymbolicIDs = ID(0xB0)  // 176
	nBuiltInIDs         = ID(0x400) // 1024
)

const (
	IDInvalid = ID(0x00)

	IDSemicolon = ID(0x01)
	IDDot       = ID(0x02)
	IDDotDot    = ID(0x03)
	IDDotDotEq  = ID(0x04)
	IDComma     = ID(0x05)
	IDExclam    = ID(0x06)
	IDQuestion  = ID(0x07)
	IDColon     = ID(0x08)
)

const (
	minOpen = 0x10
	maxOpen = 0x17

	IDOpenParen       = ID(0x10)
	IDOpenBracket     = ID(0x11)
	IDOpenCurly       = ID(0x12)
	IDOpenDoubleCurly = ID(0x13)

	minClose = 0x18
	maxClose = 0x1F

	IDCloseParen       = ID(0x18)
	IDCloseBracket     = ID(0x19)
	IDCloseCurly       = ID(0x1A)
	IDCloseDoubleCurly = ID(0x1B)
)

const (
	minAssign = 0x20
	maxAssign = 0x3F

	IDPlusEq    = ID(0x20)
	IDMinusEq   = ID(0x21)
	IDStarEq    = ID(0x22)
	IDSlashEq   = ID(0x23)
	IDShiftLEq  = ID(0x24)
	IDShiftREq  = ID(0x25)
	IDAmpEq     = ID(0x26)
	IDPipeEq    = ID(0x27)
	IDHatEq     = ID(0x28)
	IDPercentEq = ID(0x29)

	IDTildeModPlusEq   = ID(0x30)
	IDTildeModMinusEq  = ID(0x31)
	IDTildeModStarEq   = ID(0x32)
	IDTildeModShiftLEq = ID(0x34)

	IDTildeSatPlusEq  = ID(0x38)
	IDTildeSatMinusEq = ID(0x39)

	IDEq         = ID(0x3E)
	IDEqQuestion = ID(0x3F)
)

const (
	minOp          = 0x40
	minAmbiguousOp = 0x40
	maxAmbiguousOp = 0x6F
	minXOp         = 0x70
	maxXOp         = 0xAF
	maxOp          = 0xAF

	IDPlus    = ID(0x40)
	IDMinus   = ID(0x41)
	IDStar    = ID(0x42)
	IDSlash   = ID(0x43)
	IDShiftL  = ID(0x44)
	IDShiftR  = ID(0x45)
	IDAmp     = ID(0x46)
	IDPipe    = ID(0x47)
	IDHat     = ID(0x48)
	IDPercent = ID(0x49)

	IDTildeModPlus   = ID(0x50)
	IDTildeModMinus  = ID(0x51)
	IDTildeModStar   = ID(0x52)
	IDTildeModShiftL = ID(0x54)

	IDTildeSatPlus  = ID(0x58)
	IDTildeSatMinus = ID(0x59)

	IDNotEq       = ID(0x60)
	IDLessThan    = ID(0x61)
	IDLessEq      = ID(0x62)
	IDEqEq        = ID(0x63)
	IDGreaterEq   = ID(0x64)
	IDGreaterThan = ID(0x65)

	IDAnd = ID(0x68)
	IDOr  = ID(0x69)
	IDAs  = ID(0x6A)

	IDNot = ID(0x6F)

	// The IDXFoo IDs are not returned by the tokenizer. They are used by the
	// ast.Node ID-typed fields to disambiguate e.g. unary vs binary plus.

	IDXBinaryPlus    = ID(0x70)
	IDXBinaryMinus   = ID(0x71)
	IDXBinaryStar    = ID(0x72)
	IDXBinarySlash   = ID(0x73)
	IDXBinaryShiftL  = ID(0x74)
	IDXBinaryShiftR  = ID(0x75)
	IDXBinaryAmp     = ID(0x76)
	IDXBinaryPipe    = ID(0x77)
	IDXBinaryHat     = ID(0x78)
	IDXBinaryPercent = ID(0x79)

	IDXBinaryTildeModPlus   = ID(0x80)
	IDXBinaryTildeModMinus  = ID(0x81)
	IDXBinaryTildeModStar   = ID(0x82)
	IDXBinaryTildeModShiftL = ID(0x84)

	IDXBinaryTildeSatPlus  = ID(0x88)
	IDXBinaryTildeSatMinus = ID(0x89)

	IDXBinaryNotEq       = ID(0x90)
	IDXBinaryLessThan    = ID(0x91)
	IDXBinaryLessEq      = ID(0x92)
	IDXBinaryEqEq        = ID(0x93)
	IDXBinaryGreaterEq   = ID(0x94)
	IDXBinaryGreaterThan = ID(0x95)

	IDXBinaryAnd = ID(0x98)
	IDXBinaryOr  = ID(0x99)
	IDXBinaryAs  = ID(0x9A)

	IDXAssociativePlus = ID(0xA0)
	IDXAssociativeStar = ID(0xA1)
	IDXAssociativeAmp  = ID(0xA2)
	IDXAssociativePipe = ID(0xA3)
	IDXAssociativeHat  = ID(0xA4)
	IDXAssociativeAnd  = ID(0xA5)
	IDXAssociativeOr   = ID(0xA6)

	IDXUnaryPlus  = ID(0xAC)
	IDXUnaryMinus = ID(0xAD)
	IDXUnaryNot   = ID(0xAF)
)

const (
	minKeyword = 0xB0
	maxKeyword = 0xCF

	IDAssert     = ID(0xB0)
	IDBreak      = ID(0xB1)
	IDConst      = ID(0xB2)
	IDContinue   = ID(0xB3)
	IDElse       = ID(0xB4)
	IDEndwhile   = ID(0xB5)
	IDFunc       = ID(0xB6)
	IDIOBind     = ID(0xB7)
	IDIOLimit    = ID(0xB8)
	IDIf         = ID(0xB9)
	IDImplements = ID(0xBA)
	IDInv        = ID(0xBB)
	IDIterate    = ID(0xBC)
	IDPost       = ID(0xBD)
	IDPre        = ID(0xBE)
	IDPri        = ID(0xBF)
	IDPub        = ID(0xC0)
	IDReturn     = ID(0xC1)
	IDStruct     = ID(0xC2)
	IDUse        = ID(0xC3)
	IDVar        = ID(0xC4)
	IDVia        = ID(0xC5)
	IDWhile      = ID(0xC6)
	IDYield      = ID(0xC7)
)

const (
	minTypeModifier = 0xD0
	maxTypeModifier = 0xDF

	IDArray = ID(0xD0)
	IDNptr  = ID(0xD1)
	IDPtr   = ID(0xD2)
	IDSlice = ID(0xD3)
	IDTable = ID(0xD4)
)

const (
	minBuiltInLiteral    = 0xE0
	minBuiltInNumLiteral = 0xF0
	maxBuiltInNumLiteral = 0xFF
	maxBuiltInLiteral    = 0xFF

	IDFalse   = ID(0xE0)
	IDTrue    = ID(0xE1)
	IDNothing = ID(0xE2)
	IDNullptr = ID(0xE3)
	IDOk      = ID(0xE4)

	ID0   = ID(0xF0)
	ID1   = ID(0xF1)
	ID2   = ID(0xF2)
	ID4   = ID(0xF3)
	ID8   = ID(0xF4)
	ID16  = ID(0xF5)
	ID32  = ID(0xF6)
	ID64  = ID(0xF7)
	ID128 = ID(0xF8)
	ID256 = ID(0xF9)
)

const (
	minBuiltInIdent   = 0x100
	minCannotAssignTo = 0x100
	maxCannotAssignTo = 0x102
	minNumTypeOrIdeal = 0x10F
	minNumType        = 0x110
	maxNumType        = 0x117
	maxNumTypeOrIdeal = 0x117
	maxBuiltInIdent   = 0x3FF

	// -------- 0x100 block.

	IDArgs             = ID(0x100)
	IDCoroutineResumed = ID(0x101)
	IDThis             = ID(0x102)

	IDT1      = ID(0x104)
	IDT2      = ID(0x105)
	IDDagger1 = ID(0x106)
	IDDagger2 = ID(0x107)

	IDQNullptr     = ID(0x10B)
	IDQPackage     = ID(0x10C)
	IDQPlaceholder = ID(0x10D)
	IDQTypeExpr    = ID(0x10E)

	// It is important that IDQIdeal is right next to the IDI8..IDU64 block.
	// See the ID.IsNumTypeOrIdeal method.
	IDQIdeal = ID(0x10F)

	IDI8  = ID(0x110)
	IDI16 = ID(0x111)
	IDI32 = ID(0x112)
	IDI64 = ID(0x113)
	IDU8  = ID(0x114)
	IDU16 = ID(0x115)
	IDU32 = ID(0x116)
	IDU64 = ID(0x117)

	IDBase            = ID(0x120)
	IDBool            = ID(0x121)
	IDEmptyIOReader   = ID(0x122)
	IDEmptyIOWriter   = ID(0x123)
	IDEmptyStruct     = ID(0x124)
	IDMoreInformation = ID(0x125)
	IDIOReader        = ID(0x126)
	IDIOWriter        = ID(0x127)
	IDStatus          = ID(0x128)
	IDTokenReader     = ID(0x129)
	IDTokenWriter     = ID(0x12A)
	IDUtility         = ID(0x12B)

	IDRangeIEU32 = ID(0x130)
	IDRangeIIU32 = ID(0x131)
	IDRangeIEU64 = ID(0x132)
	IDRangeIIU64 = ID(0x133)
	IDRectIEU32  = ID(0x134)
	IDRectIIU32  = ID(0x135)

	IDFrameConfig   = ID(0x150)
	IDImageConfig   = ID(0x151)
	IDPixelBlend    = ID(0x152)
	IDPixelBuffer   = ID(0x153)
	IDPixelConfig   = ID(0x154)
	IDPixelFormat   = ID(0x155)
	IDPixelSwizzler = ID(0x156)

	IDDecodeFrameOptions = ID(0x158)

	IDCanUndoByte   = ID(0x160)
	IDCountSince    = ID(0x161)
	IDHistoryLength = ID(0x162)
	IDIsClosed      = ID(0x163)
	IDMark          = ID(0x164)
	IDMatch15       = ID(0x165)
	IDMatch31       = ID(0x166)
	IDMatch7        = ID(0x167)
	IDPosition      = ID(0x168)
	IDSince         = ID(0x169)
	IDSkip          = ID(0x16A)
	IDSkipU32       = ID(0x16B)
	IDSkipU32Fast   = ID(0x16C)

	IDCopyFromSlice                 = ID(0x170)
	IDLimitedCopyU32FromHistory     = ID(0x171)
	IDLimitedCopyU32FromHistoryFast = ID(0x172)
	IDLimitedCopyU32FromReader      = ID(0x173)
	IDLimitedCopyU32FromSlice       = ID(0x174)
	IDLimitedCopyU32ToSlice         = ID(0x175)

	// -------- 0x180 block.

	IDUndoByte = ID(0x180)
	IDReadU8   = ID(0x181)

	IDReadU16BE = ID(0x182)
	IDReadU16LE = ID(0x183)

	IDReadU8AsU32    = ID(0x189)
	IDReadU16BEAsU32 = ID(0x18A)
	IDReadU16LEAsU32 = ID(0x18B)
	IDReadU24BEAsU32 = ID(0x18C)
	IDReadU24LEAsU32 = ID(0x18D)
	IDReadU32BE      = ID(0x18E)
	IDReadU32LE      = ID(0x18F)

	IDReadU8AsU64    = ID(0x191)
	IDReadU16BEAsU64 = ID(0x192)
	IDReadU16LEAsU64 = ID(0x193)
	IDReadU24BEAsU64 = ID(0x194)
	IDReadU24LEAsU64 = ID(0x195)
	IDReadU32BEAsU64 = ID(0x196)
	IDReadU32LEAsU64 = ID(0x197)
	IDReadU40BEAsU64 = ID(0x198)
	IDReadU40LEAsU64 = ID(0x199)
	IDReadU48BEAsU64 = ID(0x19A)
	IDReadU48LEAsU64 = ID(0x19B)
	IDReadU56BEAsU64 = ID(0x19C)
	IDReadU56LEAsU64 = ID(0x19D)
	IDReadU64BE      = ID(0x19E)
	IDReadU64LE      = ID(0x19F)

	// --------

	IDPeekU64LEAt = ID(0x1A0)

	IDPeekU8 = ID(0x1A1)

	IDPeekU16BE = ID(0x1A2)
	IDPeekU16LE = ID(0x1A3)

	IDPeekU8AsU32    = ID(0x1A9)
	IDPeekU16BEAsU32 = ID(0x1AA)
	IDPeekU16LEAsU32 = ID(0x1AB)
	IDPeekU24BEAsU32 = ID(0x1AC)
	IDPeekU24LEAsU32 = ID(0x1AD)
	IDPeekU32BE      = ID(0x1AE)
	IDPeekU32LE      = ID(0x1AF)

	IDPeekU8AsU64    = ID(0x1B1)
	IDPeekU16BEAsU64 = ID(0x1B2)
	IDPeekU16LEAsU64 = ID(0x1B3)
	IDPeekU24BEAsU64 = ID(0x1B4)
	IDPeekU24LEAsU64 = ID(0x1B5)
	IDPeekU32BEAsU64 = ID(0x1B6)
	IDPeekU32LEAsU64 = ID(0x1B7)
	IDPeekU40BEAsU64 = ID(0x1B8)
	IDPeekU40LEAsU64 = ID(0x1B9)
	IDPeekU48BEAsU64 = ID(0x1BA)
	IDPeekU48LEAsU64 = ID(0x1BB)
	IDPeekU56BEAsU64 = ID(0x1BC)
	IDPeekU56LEAsU64 = ID(0x1BD)
	IDPeekU64BE      = ID(0x1BE)
	IDPeekU64LE      = ID(0x1BF)

	// --------

	// TODO: IDUnwriteU8?

	IDWriteU8    = ID(0x1C1)
	IDWriteU16BE = ID(0x1C2)
	IDWriteU16LE = ID(0x1C3)
	IDWriteU24BE = ID(0x1C4)
	IDWriteU24LE = ID(0x1C5)
	IDWriteU32BE = ID(0x1C6)
	IDWriteU32LE = ID(0x1C7)
	IDWriteU40BE = ID(0x1C8)
	IDWriteU40LE = ID(0x1C9)
	IDWriteU48BE = ID(0x1CA)
	IDWriteU48LE = ID(0x1CB)
	IDWriteU56BE = ID(0x1CC)
	IDWriteU56LE = ID(0x1CD)
	IDWriteU64BE = ID(0x1CE)
	IDWriteU64LE = ID(0x1CF)

	// --------

	IDWriteU8Fast    = ID(0x1E1)
	IDWriteU16BEFast = ID(0x1E2)
	IDWriteU16LEFast = ID(0x1E3)
	IDWriteU24BEFast = ID(0x1E4)
	IDWriteU24LEFast = ID(0x1E5)
	IDWriteU32BEFast = ID(0x1E6)
	IDWriteU32LEFast = ID(0x1E7)
	IDWriteU40BEFast = ID(0x1E8)
	IDWriteU40LEFast = ID(0x1E9)
	IDWriteU48BEFast = ID(0x1EA)
	IDWriteU48LEFast = ID(0x1EB)
	IDWriteU56BEFast = ID(0x1EC)
	IDWriteU56LEFast = ID(0x1ED)
	IDWriteU64BEFast = ID(0x1EE)
	IDWriteU64LEFast = ID(0x1EF)

	// --------

	IDWriteSimpleTokenFast   = ID(0x1F1)
	IDWriteExtendedTokenFast = ID(0x1F2)

	// -------- 0x200 block.

	IDInitialize = ID(0x200)
	IDReset      = ID(0x201)
	IDSet        = ID(0x202)
	IDUnroll     = ID(0x203)
	IDUpdate     = ID(0x204)

	// TODO: range/rect methods like intersection and contains?

	IDHighBits = ID(0x220)
	IDLowBits  = ID(0x221)
	IDMax      = ID(0x222)
	IDMin      = ID(0x223)

	IDIsError      = ID(0x230)
	IDIsOK         = ID(0x231)
	IDIsSuspension = ID(0x232)

	IDData            = ID(0x240)
	IDHeight          = ID(0x241)
	IDIO              = ID(0x242)
	IDLength          = ID(0x243)
	IDLimit           = ID(0x244)
	IDPrefix          = ID(0x245)
	IDRow             = ID(0x246)
	IDStride          = ID(0x247)
	IDSuffix          = ID(0x248)
	IDValidUTF8Length = ID(0x249)
	IDWidth           = ID(0x24A)

	IDSwizzleInterleavedFromReader = ID(0x280)
)

var builtInsByID = [nBuiltInIDs]string{
	IDSemicolon: ";",
	IDDot:       ".",
	IDDotDot:    "..",
	IDDotDotEq:  "..=",
	IDComma:     ",",
	IDExclam:    "!",
	IDQuestion:  "?",
	IDColon:     ":",

	IDOpenParen:       "(",
	IDOpenBracket:     "[",
	IDOpenCurly:       "{",
	IDOpenDoubleCurly: "{{",

	IDCloseParen:       ")",
	IDCloseBracket:     "]",
	IDCloseCurly:       "}",
	IDCloseDoubleCurly: "}}",

	IDPlusEq:    "+=",
	IDMinusEq:   "-=",
	IDStarEq:    "*=",
	IDSlashEq:   "/=",
	IDShiftLEq:  "<<=",
	IDShiftREq:  ">>=",
	IDAmpEq:     "&=",
	IDPipeEq:    "|=",
	IDHatEq:     "^=",
	IDPercentEq: "%=",

	IDTildeModPlusEq:   "~mod+=",
	IDTildeModMinusEq:  "~mod-=",
	IDTildeModStarEq:   "~mod*=",
	IDTildeModShiftLEq: "~mod<<=",

	IDTildeSatPlusEq:  "~sat+=",
	IDTildeSatMinusEq: "~sat-=",

	IDEq:         "=",
	IDEqQuestion: "=?",

	IDPlus:    "+",
	IDMinus:   "-",
	IDStar:    "*",
	IDSlash:   "/",
	IDShiftL:  "<<",
	IDShiftR:  ">>",
	IDAmp:     "&",
	IDPipe:    "|",
	IDHat:     "^",
	IDPercent: "%",

	IDTildeModPlus:   "~mod+",
	IDTildeModMinus:  "~mod-",
	IDTildeModStar:   "~mod*",
	IDTildeModShiftL: "~mod<<",

	IDTildeSatPlus:  "~sat+",
	IDTildeSatMinus: "~sat-",

	IDNotEq:       "<>",
	IDLessThan:    "<",
	IDLessEq:      "<=",
	IDEqEq:        "==",
	IDGreaterEq:   ">=",
	IDGreaterThan: ">",

	IDAnd: "and",
	IDOr:  "or",
	IDAs:  "as",

	IDNot: "not",

	IDAssert:     "assert",
	IDBreak:      "break",
	IDConst:      "const",
	IDContinue:   "continue",
	IDElse:       "else",
	IDEndwhile:   "endwhile",
	IDFunc:       "func",
	IDIOBind:     "io_bind",
	IDIOLimit:    "io_limit",
	IDIf:         "if",
	IDImplements: "implements",
	IDInv:        "inv",
	IDIterate:    "iterate",
	IDPost:       "post",
	IDPre:        "pre",
	IDPri:        "pri",
	IDPub:        "pub",
	IDReturn:     "return",
	IDStruct:     "struct",
	IDUse:        "use",
	IDVar:        "var",
	IDVia:        "via",
	IDWhile:      "while",
	IDYield:      "yield",

	IDArray: "array",
	IDNptr:  "nptr",
	IDPtr:   "ptr",
	IDSlice: "slice",
	IDTable: "table",

	IDFalse:   "false",
	IDTrue:    "true",
	IDNothing: "nothing",
	IDNullptr: "nullptr",
	IDOk:      "ok",

	ID0:   "0",
	ID1:   "1",
	ID2:   "2",
	ID4:   "4",
	ID8:   "8",
	ID16:  "16",
	ID32:  "32",
	ID64:  "64",
	ID128: "128",
	ID256: "256",

	// -------- 0x100 block.

	IDArgs:             "args",
	IDCoroutineResumed: "coroutine_resumed",
	IDThis:             "this",

	// Some of the next few IDs are never returned by the tokenizer, as it
	// rejects non-ASCII input. The string representations "¶", "ℤ" etc. are
	// specifically non-ASCII so that no user-defined (non built-in) identifier
	// will conflict with them.

	// IDDaggerN is used by the type checker as a dummy-valued built-in ID to
	// represent a generic type.
	IDT1:      "T1",
	IDT2:      "T2",
	IDDagger1: "†", // U+2020 DAGGER
	IDDagger2: "‡", // U+2021 DOUBLE DAGGER

	// IDQNullptr is used by the type checker to build an artificial MType for
	// the nullptr literal.
	IDQNullptr: "«Nullptr»",

	// IDQPackage is used by the type checker to build an artificial MType for
	// used (imported) packages.
	IDQPackage: "«Package»",

	// IDQPlaceholder is used by the type checker to build an artificial MType
	// for AST nodes that aren't expression nodes or type expression nodes,
	// such as struct definition nodes and statement nodes. Its presence means
	// that the node is type checked.
	IDQPlaceholder: "«Placeholder»",

	// IDQTypeExpr is used by the type checker to build an artificial MType for
	// type expression AST nodes.
	IDQTypeExpr: "«TypeExpr»",

	// IDQIdeal is used by the type checker to build an artificial MType for
	// ideal integers (in mathematical terms, the integer ring ℤ), as opposed
	// to a realized integer type whose range is restricted. For example, the
	// base.u16 type is restricted to [0x0000, 0xFFFF].
	IDQIdeal: "«Ideal»",

	// Change MaxIntBits if a future update adds an i128 or u128 type.
	IDI8:  "i8",
	IDI16: "i16",
	IDI32: "i32",
	IDI64: "i64",
	IDU8:  "u8",
	IDU16: "u16",
	IDU32: "u32",
	IDU64: "u64",

	IDBase:            "base",
	IDBool:            "bool",
	IDEmptyIOReader:   "empty_io_reader",
	IDEmptyIOWriter:   "empty_io_writer",
	IDEmptyStruct:     "empty_struct",
	IDMoreInformation: "more_information",
	IDIOReader:        "io_reader",
	IDIOWriter:        "io_writer",
	IDStatus:          "status",
	IDTokenReader:     "token_reader",
	IDTokenWriter:     "token_writer",
	IDUtility:         "utility",

	IDRangeIEU32: "range_ie_u32",
	IDRangeIIU32: "range_ii_u32",
	IDRangeIEU64: "range_ie_u64",
	IDRangeIIU64: "range_ii_u64",
	IDRectIEU32:  "rect_ie_u32",
	IDRectIIU32:  "rect_ii_u32",

	IDFrameConfig:   "frame_config",
	IDImageConfig:   "image_config",
	IDPixelBlend:    "pixel_blend",
	IDPixelBuffer:   "pixel_buffer",
	IDPixelConfig:   "pixel_config",
	IDPixelFormat:   "pixel_format",
	IDPixelSwizzler: "pixel_swizzler",

	IDDecodeFrameOptions: "decode_frame_options",

	IDCanUndoByte:   "can_undo_byte",
	IDCountSince:    "count_since",
	IDHistoryLength: "history_length",
	IDIsClosed:      "is_closed",
	IDMark:          "mark",
	IDMatch15:       "match15",
	IDMatch31:       "match31",
	IDMatch7:        "match7",
	IDPosition:      "position",
	IDSince:         "since",
	IDSkip:          "skip",
	IDSkipU32:       "skip_u32",
	IDSkipU32Fast:   "skip_u32_fast",

	IDCopyFromSlice:                 "copy_from_slice",
	IDLimitedCopyU32FromHistory:     "limited_copy_u32_from_history",
	IDLimitedCopyU32FromHistoryFast: "limited_copy_u32_from_history_fast",
	IDLimitedCopyU32FromReader:      "limited_copy_u32_from_reader",
	IDLimitedCopyU32FromSlice:       "limited_copy_u32_from_slice",
	IDLimitedCopyU32ToSlice:         "limited_copy_u32_to_slice",

	// -------- 0x180 block.

	IDUndoByte: "undo_byte",
	IDReadU8:   "read_u8",

	IDReadU16BE: "read_u16be",
	IDReadU16LE: "read_u16le",

	IDReadU8AsU32:    "read_u8_as_u32",
	IDReadU16BEAsU32: "read_u16be_as_u32",
	IDReadU16LEAsU32: "read_u16le_as_u32",
	IDReadU24BEAsU32: "read_u24be_as_u32",
	IDReadU24LEAsU32: "read_u24le_as_u32",
	IDReadU32BE:      "read_u32be",
	IDReadU32LE:      "read_u32le",

	IDReadU8AsU64:    "read_u8_as_u64",
	IDReadU16BEAsU64: "read_u16be_as_u64",
	IDReadU16LEAsU64: "read_u16le_as_u64",
	IDReadU24BEAsU64: "read_u24be_as_u64",
	IDReadU24LEAsU64: "read_u24le_as_u64",
	IDReadU32BEAsU64: "read_u32be_as_u64",
	IDReadU32LEAsU64: "read_u32le_as_u64",
	IDReadU40BEAsU64: "read_u40be_as_u64",
	IDReadU40LEAsU64: "read_u40le_as_u64",
	IDReadU48BEAsU64: "read_u48be_as_u64",
	IDReadU48LEAsU64: "read_u48le_as_u64",
	IDReadU56BEAsU64: "read_u56be_as_u64",
	IDReadU56LEAsU64: "read_u56le_as_u64",
	IDReadU64BE:      "read_u64be",
	IDReadU64LE:      "read_u64le",

	// --------

	IDPeekU64LEAt: "peek_u64le_at",

	IDPeekU8: "peek_u8",

	IDPeekU16BE: "peek_u16be",
	IDPeekU16LE: "peek_u16le",

	IDPeekU8AsU32:    "peek_u8_as_u32",
	IDPeekU16BEAsU32: "peek_u16be_as_u32",
	IDPeekU16LEAsU32: "peek_u16le_as_u32",
	IDPeekU24BEAsU32: "peek_u24be_as_u32",
	IDPeekU24LEAsU32: "peek_u24le_as_u32",
	IDPeekU32BE:      "peek_u32be",
	IDPeekU32LE:      "peek_u32le",

	IDPeekU8AsU64:    "peek_u8_as_u64",
	IDPeekU16BEAsU64: "peek_u16be_as_u64",
	IDPeekU16LEAsU64: "peek_u16le_as_u64",
	IDPeekU24BEAsU64: "peek_u24be_as_u64",
	IDPeekU24LEAsU64: "peek_u24le_as_u64",
	IDPeekU32BEAsU64: "peek_u32be_as_u64",
	IDPeekU32LEAsU64: "peek_u32le_as_u64",
	IDPeekU40BEAsU64: "peek_u40be_as_u64",
	IDPeekU40LEAsU64: "peek_u40le_as_u64",
	IDPeekU48BEAsU64: "peek_u48be_as_u64",
	IDPeekU48LEAsU64: "peek_u48le_as_u64",
	IDPeekU56BEAsU64: "peek_u56be_as_u64",
	IDPeekU56LEAsU64: "peek_u56le_as_u64",
	IDPeekU64BE:      "peek_u64be",
	IDPeekU64LE:      "peek_u64le",

	// --------

	IDWriteU8:    "write_u8",
	IDWriteU16BE: "write_u16be",
	IDWriteU16LE: "write_u16le",
	IDWriteU24BE: "write_u24be",
	IDWriteU24LE: "write_u24le",
	IDWriteU32BE: "write_u32be",
	IDWriteU32LE: "write_u32le",
	IDWriteU40BE: "write_u40be",
	IDWriteU40LE: "write_u40le",
	IDWriteU48BE: "write_u48be",
	IDWriteU48LE: "write_u48le",
	IDWriteU56BE: "write_u56be",
	IDWriteU56LE: "write_u56le",
	IDWriteU64BE: "write_u64be",
	IDWriteU64LE: "write_u64le",

	// --------

	IDWriteU8Fast:    "write_u8_fast",
	IDWriteU16BEFast: "write_u16be_fast",
	IDWriteU16LEFast: "write_u16le_fast",
	IDWriteU24BEFast: "write_u24be_fast",
	IDWriteU24LEFast: "write_u24le_fast",
	IDWriteU32BEFast: "write_u32be_fast",
	IDWriteU32LEFast: "write_u32le_fast",
	IDWriteU40BEFast: "write_u40be_fast",
	IDWriteU40LEFast: "write_u40le_fast",
	IDWriteU48BEFast: "write_u48be_fast",
	IDWriteU48LEFast: "write_u48le_fast",
	IDWriteU56BEFast: "write_u56be_fast",
	IDWriteU56LEFast: "write_u56le_fast",
	IDWriteU64BEFast: "write_u64be_fast",
	IDWriteU64LEFast: "write_u64le_fast",

	// --------

	IDWriteSimpleTokenFast:   "write_simple_token_fast",
	IDWriteExtendedTokenFast: "write_extended_token_fast",

	// -------- 0x200 block.

	IDInitialize: "initialize",
	IDReset:      "reset",
	IDSet:        "set",
	IDUnroll:     "unroll",
	IDUpdate:     "update",

	IDHighBits: "high_bits",
	IDLowBits:  "low_bits",
	IDMax:      "max",
	IDMin:      "min",

	IDIsError:      "is_error",
	IDIsOK:         "is_ok",
	IDIsSuspension: "is_suspension",

	IDData:            "data",
	IDHeight:          "height",
	IDIO:              "io",
	IDLength:          "length",
	IDLimit:           "limit",
	IDPrefix:          "prefix",
	IDRow:             "row",
	IDStride:          "stride",
	IDSuffix:          "suffix",
	IDValidUTF8Length: "valid_utf_8_length",
	IDWidth:           "width",

	IDSwizzleInterleavedFromReader: "swizzle_interleaved_from_reader",
}

var builtInsByName = map[string]ID{}

func init() {
	for i, name := range builtInsByID {
		if name != "" {
			builtInsByName[name] = ID(i)
		}
	}
}

// squiggles are built-in IDs that aren't alpha-numeric.
var squiggles = [256]ID{
	'(': IDOpenParen,
	')': IDCloseParen,
	'[': IDOpenBracket,
	']': IDCloseBracket,

	',': IDComma,
	'!': IDExclam,
	'?': IDQuestion,
	':': IDColon,
	';': IDSemicolon,
}

type suffixLexer struct {
	suffix string
	id     ID
}

// lexers lex ambiguous 1-byte squiggles. For example, "&" might be the start
// of "&^" or "&=".
//
// The order of the []suffixLexer elements matters. The first match wins. Since
// we want to lex greedily, longer suffixes should be earlier in the slice.
var lexers = [256][]suffixLexer{
	'.': {
		{".=", IDDotDotEq},
		{".", IDDotDot},
		{"", IDDot},
	},
	'&': {
		{"=", IDAmpEq},
		{"", IDAmp},
	},
	'|': {
		{"=", IDPipeEq},
		{"", IDPipe},
	},
	'^': {
		{"=", IDHatEq},
		{"", IDHat},
	},
	'+': {
		{"=", IDPlusEq},
		{"", IDPlus},
	},
	'-': {
		{"=", IDMinusEq},
		{"", IDMinus},
	},
	'*': {
		{"=", IDStarEq},
		{"", IDStar},
	},
	'/': {
		{"=", IDSlashEq},
		{"", IDSlash},
	},
	'%': {
		{"=", IDPercentEq},
		{"", IDPercent},
	},
	'=': {
		{"=", IDEqEq},
		{"?", IDEqQuestion},
		{"", IDEq},
	},
	'<': {
		{"<=", IDShiftLEq},
		{"<", IDShiftL},
		{"=", IDLessEq},
		{">", IDNotEq},
		{"", IDLessThan},
	},
	'>': {
		{">=", IDShiftREq},
		{">", IDShiftR},
		{"=", IDGreaterEq},
		{"", IDGreaterThan},
	},
	'{': {
		{"{", IDOpenDoubleCurly},
		{"", IDOpenCurly},
	},
	'}': {
		{"}", IDCloseDoubleCurly},
		{"", IDCloseCurly},
	},
	'~': {
		{"mod<<=", IDTildeModShiftLEq},
		{"mod<<", IDTildeModShiftL},
		{"mod+=", IDTildeModPlusEq},
		{"mod+", IDTildeModPlus},
		{"mod-=", IDTildeModMinusEq},
		{"mod-", IDTildeModMinus},
		{"mod*=", IDTildeModStarEq},
		{"mod*", IDTildeModStar},
		{"sat+=", IDTildeSatPlusEq},
		{"sat+", IDTildeSatPlus},
		{"sat-=", IDTildeSatMinusEq},
		{"sat-", IDTildeSatMinus},
	},
}

var ambiguousForms = [nBuiltInSymbolicIDs]ID{
	IDXBinaryPlus:           IDPlus,
	IDXBinaryMinus:          IDMinus,
	IDXBinaryStar:           IDStar,
	IDXBinarySlash:          IDSlash,
	IDXBinaryShiftL:         IDShiftL,
	IDXBinaryShiftR:         IDShiftR,
	IDXBinaryAmp:            IDAmp,
	IDXBinaryPipe:           IDPipe,
	IDXBinaryHat:            IDHat,
	IDXBinaryPercent:        IDPercent,
	IDXBinaryTildeModPlus:   IDTildeModPlus,
	IDXBinaryTildeModMinus:  IDTildeModMinus,
	IDXBinaryTildeModStar:   IDTildeModStar,
	IDXBinaryTildeModShiftL: IDTildeModShiftL,
	IDXBinaryTildeSatPlus:   IDTildeSatPlus,
	IDXBinaryTildeSatMinus:  IDTildeSatMinus,
	IDXBinaryNotEq:          IDNotEq,
	IDXBinaryLessThan:       IDLessThan,
	IDXBinaryLessEq:         IDLessEq,
	IDXBinaryEqEq:           IDEqEq,
	IDXBinaryGreaterEq:      IDGreaterEq,
	IDXBinaryGreaterThan:    IDGreaterThan,
	IDXBinaryAnd:            IDAnd,
	IDXBinaryOr:             IDOr,
	IDXBinaryAs:             IDAs,

	IDXAssociativePlus: IDPlus,
	IDXAssociativeStar: IDStar,
	IDXAssociativeAmp:  IDAmp,
	IDXAssociativePipe: IDPipe,
	IDXAssociativeHat:  IDHat,
	IDXAssociativeAnd:  IDAnd,
	IDXAssociativeOr:   IDOr,

	IDXUnaryPlus:  IDPlus,
	IDXUnaryMinus: IDMinus,
	IDXUnaryNot:   IDNot,
}

func init() {
	addXForms(&unaryForms)
	addXForms(&binaryForms)
	addXForms(&associativeForms)
}

// addXForms modifies table so that, if table[x] == y, then table[y] = y.
//
// For example, for the unaryForms table, the explicit entries are like:
//  IDPlus:        IDXUnaryPlus,
// and this function implicitly addes entries like:
//  IDXUnaryPlus:  IDXUnaryPlus,
func addXForms(table *[nBuiltInSymbolicIDs]ID) {
	implicitEntries := [nBuiltInSymbolicIDs]bool{}
	for _, y := range table {
		if y != 0 {
			implicitEntries[y] = true
		}
	}
	for y, implicit := range implicitEntries {
		if implicit {
			table[y] = ID(y)
		}
	}
}

var binaryForms = [nBuiltInSymbolicIDs]ID{
	IDPlusEq:           IDXBinaryPlus,
	IDMinusEq:          IDXBinaryMinus,
	IDStarEq:           IDXBinaryStar,
	IDSlashEq:          IDXBinarySlash,
	IDShiftLEq:         IDXBinaryShiftL,
	IDShiftREq:         IDXBinaryShiftR,
	IDAmpEq:            IDXBinaryAmp,
	IDPipeEq:           IDXBinaryPipe,
	IDHatEq:            IDXBinaryHat,
	IDPercentEq:        IDXBinaryPercent,
	IDTildeModPlusEq:   IDXBinaryTildeModPlus,
	IDTildeModMinusEq:  IDXBinaryTildeModMinus,
	IDTildeModStarEq:   IDXBinaryTildeModStar,
	IDTildeModShiftLEq: IDXBinaryTildeModShiftL,
	IDTildeSatPlusEq:   IDXBinaryTildeSatPlus,
	IDTildeSatMinusEq:  IDXBinaryTildeSatMinus,

	IDPlus:           IDXBinaryPlus,
	IDMinus:          IDXBinaryMinus,
	IDStar:           IDXBinaryStar,
	IDSlash:          IDXBinarySlash,
	IDShiftL:         IDXBinaryShiftL,
	IDShiftR:         IDXBinaryShiftR,
	IDAmp:            IDXBinaryAmp,
	IDPipe:           IDXBinaryPipe,
	IDHat:            IDXBinaryHat,
	IDPercent:        IDXBinaryPercent,
	IDTildeModPlus:   IDXBinaryTildeModPlus,
	IDTildeModMinus:  IDXBinaryTildeModMinus,
	IDTildeModStar:   IDXBinaryTildeModStar,
	IDTildeModShiftL: IDXBinaryTildeModShiftL,
	IDTildeSatPlus:   IDXBinaryTildeSatPlus,
	IDTildeSatMinus:  IDXBinaryTildeSatMinus,

	IDNotEq:       IDXBinaryNotEq,
	IDLessThan:    IDXBinaryLessThan,
	IDLessEq:      IDXBinaryLessEq,
	IDEqEq:        IDXBinaryEqEq,
	IDGreaterEq:   IDXBinaryGreaterEq,
	IDGreaterThan: IDXBinaryGreaterThan,
	IDAnd:         IDXBinaryAnd,
	IDOr:          IDXBinaryOr,
	IDAs:          IDXBinaryAs,
}

var associativeForms = [nBuiltInSymbolicIDs]ID{
	IDPlus: IDXAssociativePlus,
	IDStar: IDXAssociativeStar,
	IDAmp:  IDXAssociativeAmp,
	IDPipe: IDXAssociativePipe,
	IDHat:  IDXAssociativeHat,
	// TODO: IDTildeModPlus, IDTildeSatPlus?
	IDAnd: IDXAssociativeAnd,
	IDOr:  IDXAssociativeOr,
}

var unaryForms = [nBuiltInSymbolicIDs]ID{
	IDPlus:  IDXUnaryPlus,
	IDMinus: IDXUnaryMinus,
	IDNot:   IDXUnaryNot,
}

var isTightLeft = [...]bool{
	IDSemicolon: true,

	IDCloseParen:   true,
	IDOpenBracket:  true,
	IDCloseBracket: true,

	IDDot:      true,
	IDComma:    true,
	IDExclam:   true,
	IDQuestion: true,
	IDColon:    true,
}

var isTightRight = [...]bool{
	IDOpenParen:   true,
	IDOpenBracket: true,

	IDDot:    true,
	IDExclam: true,
}
