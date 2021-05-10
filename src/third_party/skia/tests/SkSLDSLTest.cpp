/*
 * Copyright 2020 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/GrDirectContextPriv.h"
#include "src/gpu/GrGpu.h"
#include "src/sksl/SkSLIRGenerator.h"
#include "src/sksl/dsl/DSL.h"
#include "src/sksl/dsl/priv/DSLWriter.h"
#include "src/sksl/ir/SkSLIRNode.h"

#include "tests/Test.h"

#include <limits>

using namespace SkSL::dsl;

class AutoDSLContext {
public:
    AutoDSLContext(GrGpu* gpu) {
        Start(gpu->shaderCompiler());
        DSLWriter::Instance().fMangle = false;
    }

    ~AutoDSLContext() {
        End();
    }
};

class ExpectError : public ErrorHandler {
public:
    ExpectError(skiatest::Reporter* reporter, const char* msg)
        : fMsg(msg)
        , fReporter(reporter) {
        SetErrorHandler(this);
    }

    ~ExpectError() override {
        REPORTER_ASSERT(fReporter, !fMsg,
                        "Error mismatch: expected:\n%sbut no error occurred\n", fMsg);
        SetErrorHandler(nullptr);
    }

    void handleError(const char* msg) override {
        REPORTER_ASSERT(fReporter, !strcmp(msg, fMsg),
                        "Error mismatch: expected:\n%sbut received:\n%s", fMsg, msg);
        fMsg = nullptr;
    }

private:
    const char* fMsg;
    skiatest::Reporter* fReporter;
};

static bool whitespace_insensitive_compare(const char* a, const char* b) {
    for (;;) {
        while (isspace(*a)) {
            ++a;
        }
        while (isspace(*b)) {
            ++b;
        }
        if (*a != *b) {
            return false;
        }
        if (*a == 0) {
            return true;
        }
        ++a;
        ++b;
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLStartup, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Expression e1 = 1;
    REPORTER_ASSERT(r, e1.release()->description() == "1");
    Expression e2 = 1.0;
    REPORTER_ASSERT(r, e2.release()->description() == "1.0");
    Expression e3 = true;
    REPORTER_ASSERT(r, e3.release()->description() == "true");
    Var a(kInt, "a");
    Expression e4 = a;
    REPORTER_ASSERT(r, e4.release()->description() == "a");

    REPORTER_ASSERT(r, whitespace_insensitive_compare("", ""));
    REPORTER_ASSERT(r, !whitespace_insensitive_compare("", "a"));
    REPORTER_ASSERT(r, !whitespace_insensitive_compare("a", ""));
    REPORTER_ASSERT(r, whitespace_insensitive_compare("a", "a"));
    REPORTER_ASSERT(r, whitespace_insensitive_compare("abc", "abc"));
    REPORTER_ASSERT(r, whitespace_insensitive_compare("abc", " abc "));
    REPORTER_ASSERT(r, whitespace_insensitive_compare("a b  c  ", "\n\n\nabc"));
    REPORTER_ASSERT(r, !whitespace_insensitive_compare("a b c  d", "\n\n\nabc"));
}

static SkSL::String stringize(DSLStatement& stmt)  { return stmt.release()->description(); }
static SkSL::String stringize(DSLExpression& expr) { return expr.release()->description(); }
static SkSL::String stringize(SkSL::IRNode& node)  { return node.description(); }

template <typename T>
static void expect_equal(skiatest::Reporter* r, int lineNumber, T& input, const char* expected) {
    SkSL::String actual = stringize(input);
    if (!whitespace_insensitive_compare(expected, actual.c_str())) {
        ERRORF(r, "(Failed on line %d)\nExpected: %s\n  Actual: %s\n",
                  lineNumber, expected, actual.c_str());
    }
}

template <typename T>
static void expect_equal(skiatest::Reporter* r, int lineNumber, T&& dsl, const char* expected) {
    // This overload allows temporary values to be passed to expect_equal.
    return expect_equal(r, lineNumber, dsl, expected);
}

#define EXPECT_EQUAL(a, b)  expect_equal(r, __LINE__, (a), (b))

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLFloat, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Expression e1 = Float(std::numeric_limits<float>::max());
    REPORTER_ASSERT(r, atof(e1.release()->description().c_str()) ==
                       std::numeric_limits<float>::max());

    Expression e2 = Float(std::numeric_limits<float>::min());
    REPORTER_ASSERT(r, atof(e2.release()->description().c_str()) ==
                       std::numeric_limits<float>::min());

    EXPECT_EQUAL(Float2(0),
                "float2(0.0)");
    EXPECT_EQUAL(Float2(-0.5, 1),
                "float2(-0.5, 1.0)");
    EXPECT_EQUAL(Float3(0.75),
                "float3(0.75)");
    EXPECT_EQUAL(Float3(Float2(0, 1), -2),
                "float3(float2(0.0, 1.0), -2.0)");
    EXPECT_EQUAL(Float3(0, 1, 2),
                "float3(0.0, 1.0, 2.0)");
    EXPECT_EQUAL(Float4(0),
                "float4(0.0)");
    EXPECT_EQUAL(Float4(Float2(0, 1), Float2(2, 3)),
                "float4(float2(0.0, 1.0), float2(2.0, 3.0))");
    EXPECT_EQUAL(Float4(0, 1, Float2(2, 3)),
                "float4(0.0, 1.0, float2(2.0, 3.0))");
    EXPECT_EQUAL(Float4(0, 1, 2, 3),
                "float4(0.0, 1.0, 2.0, 3.0)");

    {
        ExpectError error(r, "error: floating point value is infinite\n");
        Float(std::numeric_limits<float>::infinity()).release();
    }

    {
        ExpectError error(r, "error: floating point value is NaN\n");
        Float(std::numeric_limits<float>::quiet_NaN()).release();
    }

    {
        ExpectError error(r, "error: invalid arguments to 'float2' constructor (expected 2 scalars,"
                             " but found 4)\n");
        Float2(Float4(1)).release();
    }

    {
        ExpectError error(r, "error: invalid arguments to 'float4' constructor (expected 4 scalars,"
                             " but found 3)\n");
        Float4(Float3(1)).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLHalf, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Expression e1 = Half(std::numeric_limits<float>::max());
    REPORTER_ASSERT(r,
                    atof(e1.release()->description().c_str()) == std::numeric_limits<float>::max());

    Expression e2 = Half(std::numeric_limits<float>::min());
    REPORTER_ASSERT(r,
                    atof(e2.release()->description().c_str()) == std::numeric_limits<float>::min());

    Expression e3 = Half2(0);
    EXPECT_EQUAL(e3, "half2(0.0)");

    Expression e4 = Half2(-0.5, 1);
    EXPECT_EQUAL(e4, "half2(-0.5, 1.0)");

    Expression e5 = Half3(0.75);
    EXPECT_EQUAL(e5, "half3(0.75)");

    Expression e6 = Half3(Half2(0, 1), -2);
    EXPECT_EQUAL(e6, "half3(half2(0.0, 1.0), -2.0)");

    Expression e7 = Half3(0, 1, 2);
    EXPECT_EQUAL(e7, "half3(0.0, 1.0, 2.0)");

    Expression e8 = Half4(0);
    EXPECT_EQUAL(e8, "half4(0.0)");

    Expression e9 = Half4(Half2(0, 1), Half2(2, 3));
    EXPECT_EQUAL(e9, "half4(half2(0.0, 1.0), half2(2.0, 3.0))");

    Expression e10 = Half4(0, 1, Half2(2, 3));
    EXPECT_EQUAL(e10, "half4(0.0, 1.0, half2(2.0, 3.0))");

    Expression e11 = Half4(0, 1, 2, 3);
    EXPECT_EQUAL(e11, "half4(0.0, 1.0, 2.0, 3.0)");

    {
        ExpectError error(r, "error: floating point value is infinite\n");
        Half(std::numeric_limits<float>::infinity()).release();
    }

    {
        ExpectError error(r, "error: floating point value is NaN\n");
        Half(std::numeric_limits<float>::quiet_NaN()).release();
    }

    {
        ExpectError error(r, "error: invalid arguments to 'half2' constructor (expected 2 scalars,"
                             " but found 4)\n");
        Half2(Half4(1)).release();
    }

    {
        ExpectError error(r, "error: invalid arguments to 'half4' constructor (expected 4 scalars,"
                             " but found 3)\n");
        Half4(Half3(1)).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLInt, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Expression e1 = Int(std::numeric_limits<int32_t>::max());
    EXPECT_EQUAL(e1, "2147483647");

    Expression e2 = Int2(std::numeric_limits<int32_t>::min());
    EXPECT_EQUAL(e2, "int2(-2147483648)");

    Expression e3 = Int2(0, 1);
    EXPECT_EQUAL(e3, "int2(0, 1)");

    Expression e4 = Int3(0);
    EXPECT_EQUAL(e4, "int3(0)");

    Expression e5 = Int3(Int2(0, 1), -2);
    EXPECT_EQUAL(e5, "int3(int2(0, 1), -2)");

    Expression e6 = Int3(0, 1, 2);
    EXPECT_EQUAL(e6, "int3(0, 1, 2)");

    Expression e7 = Int4(0);
    EXPECT_EQUAL(e7, "int4(0)");

    Expression e8 = Int4(Int2(0, 1), Int2(2, 3));
    EXPECT_EQUAL(e8, "int4(int2(0, 1), int2(2, 3))");

    Expression e9 = Int4(0, 1, Int2(2, 3));
    EXPECT_EQUAL(e9, "int4(0, 1, int2(2, 3))");

    Expression e10 = Int4(0, 1, 2, 3);
    EXPECT_EQUAL(e10, "int4(0, 1, 2, 3)");

    {
        ExpectError error(r, "error: invalid arguments to 'int2' constructor (expected 2 scalars,"
                             " but found 4)\n");
        Int2(Int4(1)).release();
    }

    {
        ExpectError error(r, "error: invalid arguments to 'int4' constructor (expected 4 scalars,"
                             " but found 3)\n");
        Int4(Int3(1)).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLShort, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Expression e1 = Short(std::numeric_limits<int16_t>::max());
    EXPECT_EQUAL(e1, "32767");

    Expression e2 = Short2(std::numeric_limits<int16_t>::min());
    EXPECT_EQUAL(e2, "short2(-32768)");

    Expression e3 = Short2(0, 1);
    EXPECT_EQUAL(e3, "short2(0, 1)");

    Expression e4 = Short3(0);
    EXPECT_EQUAL(e4, "short3(0)");

    Expression e5 = Short3(Short2(0, 1), -2);
    EXPECT_EQUAL(e5, "short3(short2(0, 1), -2)");

    Expression e6 = Short3(0, 1, 2);
    EXPECT_EQUAL(e6, "short3(0, 1, 2)");

    Expression e7 = Short4(0);
    EXPECT_EQUAL(e7, "short4(0)");

    Expression e8 = Short4(Short2(0, 1), Short2(2, 3));
    EXPECT_EQUAL(e8, "short4(short2(0, 1), short2(2, 3))");

    Expression e9 = Short4(0, 1, Short2(2, 3));
    EXPECT_EQUAL(e9, "short4(0, 1, short2(2, 3))");

    Expression e10 = Short4(0, 1, 2, 3);
    EXPECT_EQUAL(e10, "short4(0, 1, 2, 3)");

    {
        ExpectError error(r, "error: invalid arguments to 'short2' constructor (expected 2 scalars,"
                             " but found 4)\n");
        Short2(Short4(1)).release();
    }

    {
        ExpectError error(r, "error: invalid arguments to 'short4' constructor (expected 4 scalars,"
                             " but found 3)\n");
        Short4(Short3(1)).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLBool, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Expression e1 = Bool2(false);
    EXPECT_EQUAL(e1, "bool2(false)");

    Expression e2 = Bool2(false, true);
    EXPECT_EQUAL(e2, "bool2(false, true)");

    Expression e3 = Bool3(false);
    EXPECT_EQUAL(e3, "bool3(false)");

    Expression e4 = Bool3(Bool2(false, true), false);
    EXPECT_EQUAL(e4, "bool3(bool2(false, true), false)");

    Expression e5 = Bool3(false, true, false);
    EXPECT_EQUAL(e5, "bool3(false, true, false)");

    Expression e6 = Bool4(false);
    EXPECT_EQUAL(e6, "bool4(false)");

    Expression e7 = Bool4(Bool2(false, true), Bool2(false, true));
    EXPECT_EQUAL(e7, "bool4(bool2(false, true), "
                                                      "bool2(false, true))");

    Expression e8 = Bool4(false, true, Bool2(false, true));
    EXPECT_EQUAL(e8, "bool4(false, true, bool2(false, true))");

    Expression e9 = Bool4(false, true, false, true);
    EXPECT_EQUAL(e9, "bool4(false, true, false, true)");

    {
        ExpectError error(r, "error: invalid arguments to 'bool2' constructor (expected 2 scalars,"
                             " but found 4)\n");
        Bool2(Bool4(true)).release();
    }

    {
        ExpectError error(r, "error: invalid arguments to 'bool4' constructor (expected 4 scalars,"
                             " but found 3)\n");
        Bool4(Bool3(true)).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLPlus, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kFloat, "a"), b(kFloat, "b");
    Expression e1 = a + b;
    EXPECT_EQUAL(e1, "(a + b)");

    Expression e2 = a + 1;
    EXPECT_EQUAL(e2, "(a + 1.0)");

    Expression e3 = 0.5 + a + -99;
    EXPECT_EQUAL(e3, "((0.5 + a) + -99.0)");

    Expression e4 = a += b + 1;
    EXPECT_EQUAL(e4, "(a += (b + 1.0))");

    {
        ExpectError error(r, "error: type mismatch: '+' cannot operate on 'bool2', 'float'\n");
        (Bool2(true) + a).release();
    }

    {
        ExpectError error(r, "error: type mismatch: '+=' cannot operate on 'float', 'bool2'\n");
        (a += Bool2(true)).release();
    }

    {
        ExpectError error(r, "error: cannot assign to this expression\n");
        (1.0 += a).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLMinus, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kInt, "b");
    Expression e1 = a - b;
    EXPECT_EQUAL(e1, "(a - b)");

    Expression e2 = a - 1;
    EXPECT_EQUAL(e2, "(a - 1)");

    Expression e3 = 2 - a - b;
    EXPECT_EQUAL(e3, "((2 - a) - b)");

    Expression e4 = a -= b + 1;
    EXPECT_EQUAL(e4, "(a -= (b + 1))");

    {
        ExpectError error(r, "error: type mismatch: '-' cannot operate on 'bool2', 'int'\n");
        (Bool2(true) - a).release();
    }

    {
        ExpectError error(r, "error: type mismatch: '-=' cannot operate on 'int', 'bool2'\n");
        (a -= Bool2(true)).release();
    }

    {
        ExpectError error(r, "error: cannot assign to this expression\n");
        (1.0 -= a).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLMultiply, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kFloat, "a"), b(kFloat, "b");
    Expression e1 = a * b;
    EXPECT_EQUAL(e1, "(a * b)");

    Expression e2 = a * 1;
    EXPECT_EQUAL(e2, "(a * 1.0)");

    Expression e3 = 0.5 * a * -99;
    EXPECT_EQUAL(e3, "((0.5 * a) * -99.0)");

    Expression e4 = a *= b + 1;
    EXPECT_EQUAL(e4, "(a *= (b + 1.0))");

    {
        ExpectError error(r, "error: type mismatch: '*' cannot operate on 'bool2', 'float'\n");
        (Bool2(true) * a).release();
    }

    {
        ExpectError error(r, "error: type mismatch: '*=' cannot operate on 'float', 'bool2'\n");
        (a *= Bool2(true)).release();
    }

    {
        ExpectError error(r, "error: cannot assign to this expression\n");
        (1.0 *= a).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLDivide, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kFloat, "a"), b(kFloat, "b");
    Expression e1 = a / b;
    EXPECT_EQUAL(e1, "(a / b)");

    Expression e2 = a / 1;
    EXPECT_EQUAL(e2, "(a / 1.0)");

    Expression e3 = 0.5 / a / -99;
    EXPECT_EQUAL(e3, "((0.5 / a) / -99.0)");

    Expression e4 = b / (a - 1);
    EXPECT_EQUAL(e4, "(b / (a - 1.0))");

    Expression e5 = a /= b + 1;
    EXPECT_EQUAL(e5, "(a /= (b + 1.0))");

    {
        ExpectError error(r, "error: type mismatch: '/' cannot operate on 'bool2', 'float'\n");
        (Bool2(true) / a).release();
    }

    {
        ExpectError error(r, "error: type mismatch: '/=' cannot operate on 'float', 'bool2'\n");
        (a /= Bool2(true)).release();
    }

    {
        ExpectError error(r, "error: cannot assign to this expression\n");
        (1.0 /= a).release();
    }

    {
        ExpectError error(r, "error: division by zero\n");
        (a /= 0).release();
    }

    {
        Var c(kFloat2, "c");
        ExpectError error(r, "error: division by zero\n");
        (c /= Float2(Float(0), 1)).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLMod, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kInt, "b");
    Expression e1 = a % b;
    EXPECT_EQUAL(e1, "(a % b)");

    Expression e2 = a % 2;
    EXPECT_EQUAL(e2, "(a % 2)");

    Expression e3 = 10 % a % -99;
    EXPECT_EQUAL(e3, "((10 % a) % -99)");

    Expression e4 = a %= b + 1;
    EXPECT_EQUAL(e4, "(a %= (b + 1))");

    {
        ExpectError error(r, "error: type mismatch: '%' cannot operate on 'bool2', 'int'\n");
        (Bool2(true) % a).release();
    }

    {
        ExpectError error(r, "error: type mismatch: '%=' cannot operate on 'int', 'bool2'\n");
        (a %= Bool2(true)).release();
    }

    {
        ExpectError error(r, "error: cannot assign to this expression\n");
        (1 %= a).release();
    }

    {
        ExpectError error(r, "error: division by zero\n");
        (a %= 0).release();
    }

    {
        Var c(kInt2, "c");
        ExpectError error(r, "error: division by zero\n");
        (c %= Int2(Int(0), 1)).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLShl, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kInt, "b");
    Expression e1 = a << b;
    EXPECT_EQUAL(e1, "(a << b)");

    Expression e2 = a << 1;
    EXPECT_EQUAL(e2, "(a << 1)");

    Expression e3 = 1 << a << 2;
    EXPECT_EQUAL(e3, "((1 << a) << 2)");

    Expression e4 = a <<= b + 1;
    EXPECT_EQUAL(e4, "(a <<= (b + 1))");

    {
        ExpectError error(r, "error: type mismatch: '<<' cannot operate on 'bool2', 'int'\n");
        (Bool2(true) << a).release();
    }

    {
        ExpectError error(r, "error: type mismatch: '<<=' cannot operate on 'int', 'bool2'\n");
        (a <<= Bool2(true)).release();
    }

    {
        ExpectError error(r, "error: cannot assign to this expression\n");
        (1 <<= a).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLShr, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kInt, "b");
    Expression e1 = a >> b;
    EXPECT_EQUAL(e1, "(a >> b)");

    Expression e2 = a >> 1;
    EXPECT_EQUAL(e2, "(a >> 1)");

    Expression e3 = 1 >> a >> 2;
    EXPECT_EQUAL(e3, "((1 >> a) >> 2)");

    Expression e4 = a >>= b + 1;
    EXPECT_EQUAL(e4, "(a >>= (b + 1))");

    {
        ExpectError error(r, "error: type mismatch: '>>' cannot operate on 'bool2', 'int'\n");
        (Bool2(true) >> a).release();
    }

    {
        ExpectError error(r, "error: type mismatch: '>>=' cannot operate on 'int', 'bool2'\n");
        (a >>= Bool2(true)).release();
    }

    {
        ExpectError error(r, "error: cannot assign to this expression\n");
        (1 >>= a).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLBitwiseAnd, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kInt, "b");
    Expression e1 = a & b;
    EXPECT_EQUAL(e1, "(a & b)");

    Expression e2 = a & 1;
    EXPECT_EQUAL(e2, "(a & 1)");

    Expression e3 = 1 & a & 2;
    EXPECT_EQUAL(e3, "((1 & a) & 2)");

    Expression e4 = a &= b + 1;
    EXPECT_EQUAL(e4, "(a &= (b + 1))");

    {
        ExpectError error(r, "error: type mismatch: '&' cannot operate on 'bool2', 'int'\n");
        (Bool2(true) & a).release();
    }

    {
        ExpectError error(r, "error: type mismatch: '&=' cannot operate on 'int', 'bool2'\n");
        (a &= Bool2(true)).release();
    }

    {
        ExpectError error(r, "error: cannot assign to this expression\n");
        (1 &= a).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLBitwiseOr, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kInt, "b");
    Expression e1 = a | b;
    EXPECT_EQUAL(e1, "(a | b)");

    Expression e2 = a | 1;
    EXPECT_EQUAL(e2, "(a | 1)");

    Expression e3 = 1 | a | 2;
    EXPECT_EQUAL(e3, "((1 | a) | 2)");

    Expression e4 = a |= b + 1;
    EXPECT_EQUAL(e4, "(a |= (b + 1))");

    {
        ExpectError error(r, "error: type mismatch: '|' cannot operate on 'bool2', 'int'\n");
        (Bool2(true) | a).release();
    }

    {
        ExpectError error(r, "error: type mismatch: '|=' cannot operate on 'int', 'bool2'\n");
        (a |= Bool2(true)).release();
    }

    {
        ExpectError error(r, "error: cannot assign to this expression\n");
        (1 |= a).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLBitwiseXor, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kInt, "b");
    Expression e1 = a ^ b;
    EXPECT_EQUAL(e1, "(a ^ b)");

    Expression e2 = a ^ 1;
    EXPECT_EQUAL(e2, "(a ^ 1)");

    Expression e3 = 1 ^ a ^ 2;
    EXPECT_EQUAL(e3, "((1 ^ a) ^ 2)");

    Expression e4 = a ^= b + 1;
    EXPECT_EQUAL(e4, "(a ^= (b + 1))");

    {
        ExpectError error(r, "error: type mismatch: '^' cannot operate on 'bool2', 'int'\n");
        (Bool2(true) ^ a).release();
    }

    {
        ExpectError error(r, "error: type mismatch: '^=' cannot operate on 'int', 'bool2'\n");
        (a ^= Bool2(true)).release();
    }

    {
        ExpectError error(r, "error: cannot assign to this expression\n");
        (1 ^= a).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLLogicalAnd, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kBool, "a"), b(kBool, "b");
    Expression e1 = a && b;
    EXPECT_EQUAL(e1, "(a && b)");

    Expression e2 = a && true && b;
    EXPECT_EQUAL(e2, "(a && b)");

    Expression e3 = a && false && b;
    EXPECT_EQUAL(e3, "false");

    {
        ExpectError error(r, "error: type mismatch: '&&' cannot operate on 'bool', 'int'\n");
        (a && 5).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLLogicalOr, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kBool, "a"), b(kBool, "b");
    Expression e1 = a || b;
    EXPECT_EQUAL(e1, "(a || b)");

    Expression e2 = a || true || b;
    EXPECT_EQUAL(e2, "true");

    Expression e3 = a || false || b;
    EXPECT_EQUAL(e3, "(a || b)");

    {
        ExpectError error(r, "error: type mismatch: '||' cannot operate on 'bool', 'int'\n");
        (a || 5).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLComma, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kInt, "b");
    Expression e1 = (a += b, b);
    EXPECT_EQUAL(e1, "((a += b) , b)");

    Expression e2 = (a += b, b += b, Int2(a));
    EXPECT_EQUAL(e2, "(((a += b) , (b += b)) , int2(a))");
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLEqual, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kInt, "b");
    Expression e1 = a == b;
    EXPECT_EQUAL(e1, "(a == b)");

    Expression e2 = a == 5;
    EXPECT_EQUAL(e2, "(a == 5)");

    {
        ExpectError error(r, "error: type mismatch: '==' cannot operate on 'int', 'bool2'\n");
        (a == Bool2(true)).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLNotEqual, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kInt, "b");
    Expression e1 = a != b;
    EXPECT_EQUAL(e1, "(a != b)");

    Expression e2 = a != 5;
    EXPECT_EQUAL(e2, "(a != 5)");

    {
        ExpectError error(r, "error: type mismatch: '!=' cannot operate on 'int', 'bool2'\n");
        (a != Bool2(true)).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLGreaterThan, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kInt, "b");
    Expression e1 = a > b;
    EXPECT_EQUAL(e1, "(a > b)");

    Expression e2 = a > 5;
    EXPECT_EQUAL(e2, "(a > 5)");

    {
        ExpectError error(r, "error: type mismatch: '>' cannot operate on 'int', 'bool2'\n");
        (a > Bool2(true)).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLGreaterThanOrEqual, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kInt, "b");
    Expression e1 = a >= b;
    EXPECT_EQUAL(e1, "(a >= b)");

    Expression e2 = a >= 5;
    EXPECT_EQUAL(e2, "(a >= 5)");

    {
        ExpectError error(r, "error: type mismatch: '>=' cannot operate on 'int', 'bool2'\n");
        (a >= Bool2(true)).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLLessThan, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kInt, "b");
    Expression e1 = a < b;
    EXPECT_EQUAL(e1, "(a < b)");

    Expression e2 = a < 5;
    EXPECT_EQUAL(e2, "(a < 5)");

    {
        ExpectError error(r, "error: type mismatch: '<' cannot operate on 'int', 'bool2'\n");
        (a < Bool2(true)).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLLessThanOrEqual, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kInt, "b");
    Expression e1 = a <= b;
    EXPECT_EQUAL(e1, "(a <= b)");

    Expression e2 = a <= 5;
    EXPECT_EQUAL(e2, "(a <= 5)");

    {
        ExpectError error(r, "error: type mismatch: '<=' cannot operate on 'int', 'bool2'\n");
        (a <= Bool2(true)).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLLogicalNot, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kInt, "b");
    Expression e1 = !(a <= b);
    EXPECT_EQUAL(e1, "!(a <= b)");

    {
        ExpectError error(r, "error: '!' cannot operate on 'int'\n");
        (!a).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLBitwiseNot, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kBool, "b");
    Expression e1 = ~a;
    EXPECT_EQUAL(e1, "~a");

    {
        ExpectError error(r, "error: '~' cannot operate on 'bool'\n");
        (~b).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLIncrement, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kBool, "b");
    Expression e1 = ++a;
    EXPECT_EQUAL(e1, "++a");

    Expression e2 = a++;
    EXPECT_EQUAL(e2, "a++");

    {
        ExpectError error(r, "error: '++' cannot operate on 'bool'\n");
        (++b).release();
    }

    {
        ExpectError error(r, "error: '++' cannot operate on 'bool'\n");
        (b++).release();
    }

    {
        ExpectError error(r, "error: cannot assign to this expression\n");
        (++(a + 1)).release();
    }

    {
        ExpectError error(r, "error: cannot assign to this expression\n");
        ((a + 1)++).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLDecrement, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a"), b(kBool, "b");
    Expression e1 = --a;
    EXPECT_EQUAL(e1, "--a");

    Expression e2 = a--;
    EXPECT_EQUAL(e2, "a--");

    {
        ExpectError error(r, "error: '--' cannot operate on 'bool'\n");
        (--b).release();
    }

    {
        ExpectError error(r, "error: '--' cannot operate on 'bool'\n");
        (b--).release();
    }

    {
        ExpectError error(r, "error: cannot assign to this expression\n");
        (--(a + 1)).release();
    }

    {
        ExpectError error(r, "error: cannot assign to this expression\n");
        ((a + 1)--).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLBlock, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Statement x = Block();
    EXPECT_EQUAL(x, "{ }");
    Var a(kInt, "a"), b(kInt, "b");
    Statement y = Block(Declare(a, 1), Declare(b, 2), a = b);
    EXPECT_EQUAL(y, "{ int a = 1; int b = 2; (a = b); }");
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLBreak, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var i(kInt, "i");
    DSLFunction(kVoid, "success").define(
        For(Declare(i, 0), i < 10, ++i, Block(
            If(i > 5, Break())
        ))
    );
    REPORTER_ASSERT(r, DSLWriter::ProgramElements().size() == 1);
    EXPECT_EQUAL(*DSLWriter::ProgramElements()[0],
                 "void success() { for (int i = 0; (i < 10); ++i) { if ((i > 5)) break; } }");

    {
        ExpectError error(r, "error: break statement must be inside a loop or switch\n");
        DSLFunction(kVoid, "fail").define(
            Break()
        );
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLContinue, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var i(kInt, "i");
    DSLFunction(kVoid, "success").define(
        For(Declare(i, 0), i < 10, ++i, Block(
            If(i < 5, Continue())
        ))
    );
    REPORTER_ASSERT(r, DSLWriter::ProgramElements().size() == 1);
    EXPECT_EQUAL(*DSLWriter::ProgramElements()[0],
                 "void success() { for (int i = 0; (i < 10); ++i) { if ((i < 5)) continue; } }");

    {
        ExpectError error(r, "error: continue statement must be inside a loop\n");
        DSLFunction(kVoid, "fail").define(
            Continue()
        );
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLDeclare, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kHalf4, "a"), b(kHalf4, "b");
    Statement x = Declare(a);
    EXPECT_EQUAL(x, "half4 a;");
    Statement y = Declare(b, Half4(1));
    EXPECT_EQUAL(y, "half4 b = half4(1.0);");

    {
        Var c(kHalf4, "c");
        ExpectError error(r, "error: expected 'half4', but found 'int'\n");
        Declare(c, 1).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLDiscard, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Statement x = If(Sqrt(1) > 0, Discard());
    EXPECT_EQUAL(x, "if ((sqrt(1.0) > 0.0)) discard;");
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLDo, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Statement x = Do(Block(), true);
    EXPECT_EQUAL(x, "do {} while (true);");

    Var a(kFloat, "a"), b(kFloat, "b");
    Statement y = Do(Block(a++, --b), a != b);
    EXPECT_EQUAL(y, "do { a++; --b; } while ((a != b));");

    {
        ExpectError error(r, "error: expected 'bool', but found 'int'\n");
        Do(Block(), 7).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLFor, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Statement x = For(Statement(), Expression(), Expression(), Block());
    EXPECT_EQUAL(x, "for (;;) {}");

    Var i(kInt, "i");
    Statement y = For(Declare(i, 0), i < 10, ++i, i += 5);
    EXPECT_EQUAL(y, "for (int i = 0; (i < 10); ++i) (i += 5);");

    {
        ExpectError error(r, "error: expected 'bool', but found 'int'\n");
        For(i = 0, i + 10, ++i, i += 5).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLFunction, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    DSLWriter::ProgramElements().clear();
    Var coords(kHalf2, "coords");
    DSLFunction(kVoid, "main", coords).define(
        sk_FragColor() = Half4(coords, 0, 1)
    );
    REPORTER_ASSERT(r, DSLWriter::ProgramElements().size() == 1);
    EXPECT_EQUAL(*DSLWriter::ProgramElements()[0],
                 "void main(half2 coords) { (sk_FragColor = half4(coords, 0.0, 1.0)); }");

    {
        DSLWriter::ProgramElements().clear();
        Var x(kFloat, "x");
        DSLFunction sqr(kFloat, "sqr", x);
        sqr.define(
            Return(x * x)
        );
        EXPECT_EQUAL(sqr(sk_FragCoord().x()), "sqr(sk_FragCoord.x)");
        REPORTER_ASSERT(r, DSLWriter::ProgramElements().size() == 1);
        EXPECT_EQUAL(*DSLWriter::ProgramElements()[0], "float sqr(float x) { return (x * x); }");
    }

    {
        DSLWriter::ProgramElements().clear();
        Var x(kFloat2, "x");
        Var y(kFloat2, "y");
        DSLFunction dot(kFloat2, "dot", x, y);
        dot.define(
            Return(x * x + y * y)
        );
        EXPECT_EQUAL(dot(Float2(1.0f, 2.0f), Float2(3.0f, 4.0f)),
                     "dot(float2(1.0, 2.0), float2(3.0, 4.0))");
        REPORTER_ASSERT(r, DSLWriter::ProgramElements().size() == 1);
        EXPECT_EQUAL(*DSLWriter::ProgramElements()[0],
                "float2 dot(float2 x, float2 y) { return ((x * x) + (y * y)); }");
    }

    {
        ExpectError error(r, "error: expected 'float', but found 'bool'\n");
        DSLWriter::ProgramElements().clear();
        DSLFunction(kFloat, "broken").define(
            Return(true)
        );
    }

    {
        ExpectError error(r, "error: expected function to return 'float'\n");
        DSLWriter::ProgramElements().clear();
        DSLFunction(kFloat, "broken").define(
            Return()
        );
    }

    {
        ExpectError error(r, "error: may not return a value from a void function\n");
        DSLWriter::ProgramElements().clear();
        DSLFunction(kVoid, "broken").define(
            Return(0)
        );
    }

/* TODO: detect this case
    {
        ExpectError error(r, "error: expected function to return 'float'\n");
        DSLWriter::ProgramElements().clear();
        DSLFunction(kFloat, "broken").define(
        );
    }
*/
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLIf, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kFloat, "a"), b(kFloat, "b");
    Statement x = If(a > b, a -= b);
    EXPECT_EQUAL(x, "if ((a > b)) (a -= b);");

    Statement y = If(a > b, a -= b, b -= a);
    EXPECT_EQUAL(y, "if ((a > b)) (a -= b); else (b -= a);");

    {
        ExpectError error(r, "error: expected 'bool', but found 'float'\n");
        If(a + b, a -= b).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLReturn, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());

    Statement x = Return();
    EXPECT_EQUAL(x, "return;");

    Statement y = Return(true);
    EXPECT_EQUAL(y, "return true;");
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLSelect, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kInt, "a");
    Expression x = Select(a > 0, 1, -1);
    EXPECT_EQUAL(x, "((a > 0) ? 1 : -1)");

    {
        ExpectError error(r, "error: expected 'bool', but found 'int'\n");
        Select(a, 1, -1).release();
    }

    {
        ExpectError error(r, "error: ternary operator result mismatch: 'float2', 'float3'\n");
        Select(a > 0, Float2(1), Float3(1)).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLSwitch, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());

    Var a(kFloat, "a"), b(kInt, "b");

    Statement x = Switch(5,
        Case(0, a = 0, Break()),
        Case(1, a = 1, Continue()),
        Case(2, a = 2  /*Fallthrough*/),
        Default(Discard())
    );
    EXPECT_EQUAL(x, R"(
        switch (5) {
            case 0: (a = 0.0); break;
            case 1: (a = 1.0); continue;
            case 2: (a = 2.0);
            default: discard;
        }
    )");

    EXPECT_EQUAL(Switch(b),
                "switch (b) {}");

    EXPECT_EQUAL(Switch(b, Default(), Case(0), Case(1)),
                "switch (b) { default: case 0: case 1: }");

    {
        ExpectError error(r, "error: duplicate case value '0'\n");
        Switch(0, Case(0), Case(0)).release();
    }

    {
        ExpectError error(r, "error: duplicate default case\n");
        Switch(0, Default(a = 0), Default(a = 1)).release();
    }

    {
        ExpectError error(r, "error: case value must be a constant integer\n");
        Var b(kInt);
        Switch(0, Case(b)).release();
    }

    {
        ExpectError error(r, "error: continue statement must be inside a loop\n");
        DSLFunction(kVoid, "fail").define(
            Switch(5,
                Case(0, a = 0, Break()),
                Case(1, a = 1, Continue()),
                Default(Discard())
            )
        );
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLSwizzle, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(kFloat4, "a");

    Expression e1 = a.x();
    EXPECT_EQUAL(e1, "a.x");

    Expression e2 = a.y();
    EXPECT_EQUAL(e2, "a.y");

    Expression e3 = a.z();
    EXPECT_EQUAL(e3, "a.z");

    Expression e4 = a.w();
    EXPECT_EQUAL(e4, "a.w");

    Expression e5 = a.r();
    EXPECT_EQUAL(e5, "a.x");

    Expression e6 = a.g();
    EXPECT_EQUAL(e6, "a.y");

    Expression e7 = a.b();
    EXPECT_EQUAL(e7, "a.z");

    Expression e8 = a.a();
    EXPECT_EQUAL(e8, "a.w");

    Expression e9 = Swizzle(a, R);
    EXPECT_EQUAL(e9, "a.x");

    Expression e10 = Swizzle(a, ZERO, G);
    EXPECT_EQUAL(e10, "float2(a.y, 0.0).yx");

    Expression e11 = Swizzle(a, B, G, G);
    EXPECT_EQUAL(e11, "a.zyy");

    Expression e12 = Swizzle(a, R, G, B, ONE);
    EXPECT_EQUAL(e12, "float4(a.xyz, 1.0)");

    Expression e13 = Swizzle(a, R, G, B, ONE).r();
    EXPECT_EQUAL(e13, "float4(a.xyz, 1.0).x");
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLWhile, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Statement x = While(true, Block());
    EXPECT_EQUAL(x, "for (; true;) {}");

    Var a(kFloat, "a"), b(kFloat, "b");
    Statement y = While(a != b, Block(a++, --b));
    EXPECT_EQUAL(y, "for (; (a != b);) { a++; --b; }");

    {
        ExpectError error(r, "error: expected 'bool', but found 'int'\n");
        While(7, Block()).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLIndex, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    Var a(Array(kInt, 5), "a"), b(kInt, "b");

    EXPECT_EQUAL(a[0], "a[0]");
    EXPECT_EQUAL(a[b], "a[b]");

    {
        ExpectError error(r, "error: expected 'int', but found 'bool'\n");
        a[true].release();
    }

    {
        ExpectError error(r, "error: expected array, but found 'int'\n");
        b[0].release();
    }

    {
        ExpectError error(r, "error: index -1 out of range for 'int[5]'\n");
        a[-1].release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLBuiltins, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());
    // There is a Fract type on Mac which can conflict with our Fract builtin
    using SkSL::dsl::Fract;
    Var a(kHalf4, "a"), b(kHalf4, "b"), c(kHalf4, "c");
    Var h3(kHalf3, "h3");
    Var b4(kBool4, "b4");
    EXPECT_EQUAL(Abs(a),                 "abs(a)");
    EXPECT_EQUAL(All(b4),                "all(b4)");
    EXPECT_EQUAL(Any(b4),                "any(b4)");
    EXPECT_EQUAL(Ceil(a),                "ceil(a)");
    EXPECT_EQUAL(Clamp(a, 0, 1),         "clamp(a, 0.0, 1.0)");
    EXPECT_EQUAL(Cos(a),                 "cos(a)");
    EXPECT_EQUAL(Cross(h3, h3),          "cross(h3, h3)");
    EXPECT_EQUAL(Degrees(a),             "degrees(a)");
    EXPECT_EQUAL(Distance(a, b),         "distance(a, b)");
    EXPECT_EQUAL(Dot(a, b),              "dot(a, b)");
    EXPECT_EQUAL(Equal(a, b),            "equal(a, b)");
    EXPECT_EQUAL(Exp(a),                 "exp(a)");
    EXPECT_EQUAL(Exp2(a),                "exp2(a)");
    EXPECT_EQUAL(Faceforward(a, b, c),   "faceforward(a, b, c)");
    EXPECT_EQUAL(Floor(a),               "floor(a)");
    EXPECT_EQUAL(Fract(a),               "fract(a)");
    EXPECT_EQUAL(GreaterThan(a, b),      "greaterThan(a, b)");
    EXPECT_EQUAL(GreaterThanEqual(a, b), "greaterThanEqual(a, b)");
    EXPECT_EQUAL(Inversesqrt(a),         "inversesqrt(a)");
    EXPECT_EQUAL(LessThan(a, b),         "lessThan(a, b)");
    EXPECT_EQUAL(LessThanEqual(a, b),    "lessThanEqual(a, b)");
    EXPECT_EQUAL(Length(a),              "length(a)");
    EXPECT_EQUAL(Log(a),                 "log(a)");
    EXPECT_EQUAL(Log2(a),                "log2(a)");
    EXPECT_EQUAL(Max(a, b),              "max(a, b)");
    EXPECT_EQUAL(Min(a, b),              "min(a, b)");
    EXPECT_EQUAL(Mix(a, b, c),           "mix(a, b, c)");
    EXPECT_EQUAL(Mod(a, b),              "mod(a, b)");
    EXPECT_EQUAL(Normalize(a),           "normalize(a)");
    EXPECT_EQUAL(NotEqual(a, b),         "notEqual(a, b)");
    EXPECT_EQUAL(Pow(a, b),              "pow(a, b)");
    EXPECT_EQUAL(Radians(a),             "radians(a)");
    EXPECT_EQUAL(Reflect(a, b),          "reflect(a, b)");
    EXPECT_EQUAL(Refract(a, b, 1),       "refract(a, b, 1.0)");
    EXPECT_EQUAL(Saturate(a),            "saturate(a)");
    EXPECT_EQUAL(Sign(a),                "sign(a)");
    EXPECT_EQUAL(Sin(a),                 "sin(a)");
    EXPECT_EQUAL(Smoothstep(a, b, c),    "smoothstep(a, b, c)");
    EXPECT_EQUAL(Sqrt(a),                "sqrt(a)");
    EXPECT_EQUAL(Step(a, b),             "step(a, b)");
    EXPECT_EQUAL(Tan(a),                 "tan(a)");
    EXPECT_EQUAL(Unpremul(a),            "unpremul(a)");

    // these calls all go through the normal channels, so it ought to be sufficient to prove that
    // one of them reports errors correctly
    {
        ExpectError error(r, "error: no match for ceil(bool)\n");
        Ceil(a == b).release();
    }
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLModifiers, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());

    Var v1(kConst_Modifier, kInt, "v1");
    Statement d1 = Declare(v1, 0);
    EXPECT_EQUAL(d1, "const int v1 = 0;");

    // Most modifiers require an appropriate context to be legal. We can't yet give them that
    // context, so we can't as yet Declare() variables with these modifiers.
    // TODO: better tests when able
    Var v2(kIn_Modifier, kInt, "v2");
    REPORTER_ASSERT(r, DSLWriter::Var(v2).modifiers().fFlags == SkSL::Modifiers::kIn_Flag);

    Var v3(kOut_Modifier, kInt, "v3");
    REPORTER_ASSERT(r, DSLWriter::Var(v3).modifiers().fFlags == SkSL::Modifiers::kOut_Flag);

    Var v4(kFlat_Modifier, kInt, "v4");
    REPORTER_ASSERT(r, DSLWriter::Var(v4).modifiers().fFlags == SkSL::Modifiers::kFlat_Flag);

    Var v5(kNoPerspective_Modifier, kInt, "v5");
    REPORTER_ASSERT(r, DSLWriter::Var(v5).modifiers().fFlags ==
                       SkSL::Modifiers::kNoPerspective_Flag);

    Var v6(kIn_Modifier | kOut_Modifier, kInt, "v6");
    REPORTER_ASSERT(r, DSLWriter::Var(v6).modifiers().fFlags ==
                       (SkSL::Modifiers::kIn_Flag | SkSL::Modifiers::kOut_Flag));

    Var v7(kInOut_Modifier, kInt, "v7");
    REPORTER_ASSERT(r, DSLWriter::Var(v7).modifiers().fFlags ==
                       (SkSL::Modifiers::kIn_Flag | SkSL::Modifiers::kOut_Flag));

    Var v8(kUniform_Modifier, kInt, "v8");
    REPORTER_ASSERT(r, DSLWriter::Var(v8).modifiers().fFlags == SkSL::Modifiers::kUniform_Flag);
}

DEF_GPUTEST_FOR_MOCK_CONTEXT(DSLStruct, r, ctxInfo) {
    AutoDSLContext context(ctxInfo.directContext()->priv().getGpu());

    DSLType simpleStruct = Struct("SimpleStruct",
        Field(kFloat, "x"),
        Field(kBool, "b"),
        Field(Array(kFloat, 3), "a")
    );
    DSLVar result(simpleStruct, "result");
    DSLFunction(simpleStruct, "returnStruct").define(
        Declare(result),
        result.field("x") = 123,
        result.field("b") = result.field("x") > 0,
        result.field("a")[0] = result.field("x"),
        Return(result)
    );
    REPORTER_ASSERT(r, DSLWriter::ProgramElements().size() == 2);
    EXPECT_EQUAL(*DSLWriter::ProgramElements()[0],
                 "struct SimpleStruct { float x; bool b; float[3] a; };");
    EXPECT_EQUAL(*DSLWriter::ProgramElements()[1],
                 "SimpleStruct returnStruct() { SimpleStruct result; (result.x = 123.0);"
                 "(result.b = (result.x > 0.0)); (result.a[0] = result.x); return result; }");

    DSLWriter::ProgramElements().clear();
    Struct("NestedStruct",
        Field(kInt, "x"),
        Field(simpleStruct, "simple")
    );
    REPORTER_ASSERT(r, DSLWriter::ProgramElements().size() == 1);
    EXPECT_EQUAL(*DSLWriter::ProgramElements()[0],
                 "struct NestedStruct { int x; SimpleStruct simple; };");
}
