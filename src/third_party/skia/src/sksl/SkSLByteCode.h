/*
 * Copyright 2019 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_BYTECODE
#define SKSL_BYTECODE

#include "src/sksl/ir/SkSLFunctionDeclaration.h"

#include <memory>
#include <vector>

namespace SkSL {

struct ByteCode;
class  ExternalValue;
struct FunctionDeclaration;

#define VECTOR(name) name, name ## 2, name ## 3, name ## 4
#define VECTOR_MATRIX(name) name, name ## 2, name ## 3, name ## 4, name ## N

enum class ByteCodeInstruction : uint16_t {
    // B = bool, F = float, I = int, S = signed, U = unsigned
    VECTOR_MATRIX(kAddF),
    VECTOR(kAddI),
    VECTOR(kAndB),
    VECTOR(kAndI),
    kBranch,
    // Followed by a byte indicating the index of the function to call
    kCall,
    // Followed by three bytes indicating: the number of argument slots, the number of return slots,
    // and the index of the external value to call
    kCallExternal,
    VECTOR(kCompareIEQ),
    VECTOR(kCompareINEQ),
    VECTOR_MATRIX(kCompareFEQ),
    VECTOR(kCompareFGT),
    VECTOR(kCompareFGTEQ),
    VECTOR(kCompareFLT),
    VECTOR(kCompareFLTEQ),
    VECTOR_MATRIX(kCompareFNEQ),
    VECTOR(kCompareSGT),
    VECTOR(kCompareSGTEQ),
    VECTOR(kCompareSLT),
    VECTOR(kCompareSLTEQ),
    VECTOR(kCompareUGT),
    VECTOR(kCompareUGTEQ),
    VECTOR(kCompareULT),
    VECTOR(kCompareULTEQ),
    // Followed by a 16 bit address
    kConditionalBranch,
    VECTOR(kConvertFtoI),
    VECTOR(kConvertStoF),
    VECTOR(kConvertUtoF),
    VECTOR(kCos),
    kCross,
    // Pops and prints the top value from the stack
    kDebugPrint,
    VECTOR_MATRIX(kDivideF),
    VECTOR(kDivideS),
    VECTOR(kDivideU),
    // Duplicates the top stack value
    VECTOR_MATRIX(kDup),
    // kLoad/kLoadGlobal are followed by a byte indicating the local/global slot to load
    VECTOR(kLoad),
    VECTOR(kLoadGlobal),
    // As kLoad/kLoadGlobal, then a count byte (1-4), and then one byte per swizzle component (0-3).
    kLoadSwizzle,
    kLoadSwizzleGlobal,
    // kLoadExtended* are fallback load ops when we lack a specialization. They are followed by a
    // count byte, and get the slot to load from the top of the stack.
    kLoadExtended,
    kLoadExtendedGlobal,
    // Followed by four bytes: srcCols, srcRows, dstCols, dstRows. Consumes the src matrix from the
    // stack, and replaces it with the dst matrix. Per GLSL rules, there are no restrictions on
    // dimensions. Any overlapping values are copied, and any other values are filled in with the
    // identity matrix.
    kMatrixToMatrix,
    // Followed by three bytes: leftCols (== rightRows), leftRows, rightCols
    kMatrixMultiply,
    VECTOR_MATRIX(kNegateF),
    VECTOR(kNegateI),
    VECTOR(kMix),
    VECTOR_MATRIX(kMultiplyF),
    VECTOR(kMultiplyI),
    VECTOR(kNot),
    VECTOR(kOrB),
    VECTOR(kOrI),
    VECTOR_MATRIX(kPop),
    // Followed by a 32 bit value containing the value to push
    kPushImmediate,
    // Followed by a byte indicating external value to read
    VECTOR(kReadExternal),
    VECTOR(kRemainderF),
    VECTOR(kRemainderS),
    VECTOR(kRemainderU),
    // Followed by a byte indicating the number of slots being returned
    kReturn,
    // Followed by two bytes indicating columns and rows of matrix (2, 3, or 4 each).
    // Takes a single value from the top of the stack, and converts to a CxR matrix with that value
    // replicated along the diagonal (and zero elsewhere), per the GLSL matrix construction rules.
    kScalarToMatrix,
    VECTOR(kSin),
    VECTOR(kSqrt),
    // kStore/kStoreGlobal are followed by a byte indicating the local/global slot to store
    VECTOR(kStore),
    VECTOR(kStoreGlobal),
    // Fallback stores. Followed by count byte, and get the slot to store from the top of the stack
    kStoreExtended,
    kStoreExtendedGlobal,
    // As kStore/kStoreGlobal, then a count byte (1-4), then one byte per swizzle component (0-3).
    // Expects the stack to look like: ... v1 v2 v3 v4, where the number of 'v's is equal to the
    // number of swizzle components. After the store, all v's are popped from the stack.
    kStoreSwizzle,
    kStoreSwizzleGlobal,
    // As above, but gets the store slot from the top of the stack (before values to be stored)
    kStoreSwizzleIndirect,
    kStoreSwizzleIndirectGlobal,
    // Followed by two count bytes (1-4), and then one byte per swizzle component (0-3). The first
    // count byte provides the current vector size (the vector is the top n stack elements), and the
    // second count byte provides the swizzle component count.
    kSwizzle,
    VECTOR_MATRIX(kSubtractF),
    VECTOR(kSubtractI),
    VECTOR(kTan),
    VECTOR(kXorB),
    VECTOR(kXorI),
    // Followed by a byte indicating external value to write
    VECTOR(kWriteExternal),
};
#undef VECTOR

struct ByteCodeFunction {
    ByteCodeFunction(const FunctionDeclaration* declaration)
        : fDeclaration(*declaration) {}

    const FunctionDeclaration& fDeclaration;
    int fParameterCount = 0;
    int fLocalCount = 0;
    // TODO: Compute this value analytically. For now, just pick an arbitrary value that we probably
    // won't overflow.
    int fStackCount = 128;
    int fReturnCount = 0;
    std::vector<uint8_t> fCode;
};

struct ByteCode {
    int fGlobalCount = 0;
    // one entry per input slot, contains the global slot to which the input slot maps
    std::vector<uint8_t> fInputSlots;
    std::vector<std::unique_ptr<ByteCodeFunction>> fFunctions;

    const ByteCodeFunction* getFunction(const char* name) const {
        for (const auto& f : fFunctions) {
            if (f->fDeclaration.fName == name) {
                return f.get();
            }
        }
        return nullptr;
    }

    std::vector<ExternalValue*> fExternalValues;
};

}

#endif
