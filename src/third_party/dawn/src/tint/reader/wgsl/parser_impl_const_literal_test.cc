// Copyright 2020 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/tint/reader/wgsl/parser_impl_test_helper.h"

#include <cstring>

#include "gmock/gmock.h"

namespace tint::reader::wgsl {
namespace {

// Makes an IEEE 754 binary64 floating point number with
// - 0 sign if sign is 0, 1 otherwise
// - 'exponent_bits' is placed in the exponent space.
//   So, the exponent bias must already be included.
double MakeDouble(uint64_t sign, uint64_t biased_exponent, uint64_t mantissa) {
    const uint64_t sign_bit = sign ? 0x8000000000000000u : 0u;
    // The binary64 exponent is 11 bits, just below the sign.
    const uint64_t exponent_bits = (biased_exponent & 0x7FFull) << 52;
    // The mantissa is the bottom 52 bits.
    const uint64_t mantissa_bits = (mantissa & 0xFFFFFFFFFFFFFull);

    uint64_t bits = sign_bit | exponent_bits | mantissa_bits;
    double result = 0.0;
    static_assert(sizeof(result) == sizeof(bits),
                  "expected double and uint64_t to be the same size");
    std::memcpy(&result, &bits, sizeof(bits));
    return result;
}

TEST_F(ParserImplTest, ConstLiteral_Int) {
    {
        auto p = parser("234");
        auto c = p->const_literal();
        EXPECT_TRUE(c.matched);
        EXPECT_FALSE(c.errored);
        EXPECT_FALSE(p->has_error()) << p->error();
        ASSERT_NE(c.value, nullptr);
        ASSERT_TRUE(c->Is<ast::IntLiteralExpression>());
        EXPECT_EQ(c->As<ast::IntLiteralExpression>()->value, 234);
        EXPECT_EQ(c->As<ast::IntLiteralExpression>()->suffix,
                  ast::IntLiteralExpression::Suffix::kNone);
        EXPECT_EQ(c->source.range, (Source::Range{{1u, 1u}, {1u, 4u}}));
    }
    {
        auto p = parser("234i");
        auto c = p->const_literal();
        EXPECT_TRUE(c.matched);
        EXPECT_FALSE(c.errored);
        EXPECT_FALSE(p->has_error()) << p->error();
        ASSERT_NE(c.value, nullptr);
        ASSERT_TRUE(c->Is<ast::IntLiteralExpression>());
        EXPECT_EQ(c->As<ast::IntLiteralExpression>()->value, 234);
        EXPECT_EQ(c->As<ast::IntLiteralExpression>()->suffix,
                  ast::IntLiteralExpression::Suffix::kI);
        EXPECT_EQ(c->source.range, (Source::Range{{1u, 1u}, {1u, 5u}}));
    }
    {
        auto p = parser("-234");
        auto c = p->const_literal();
        EXPECT_TRUE(c.matched);
        EXPECT_FALSE(c.errored);
        EXPECT_FALSE(p->has_error()) << p->error();
        ASSERT_NE(c.value, nullptr);
        ASSERT_TRUE(c->Is<ast::IntLiteralExpression>());
        EXPECT_EQ(c->As<ast::IntLiteralExpression>()->value, -234);
        EXPECT_EQ(c->As<ast::IntLiteralExpression>()->suffix,
                  ast::IntLiteralExpression::Suffix::kNone);
        EXPECT_EQ(c->source.range, (Source::Range{{1u, 1u}, {1u, 5u}}));
    }
    {
        auto p = parser("-234i");
        auto c = p->const_literal();
        EXPECT_TRUE(c.matched);
        EXPECT_FALSE(c.errored);
        EXPECT_FALSE(p->has_error()) << p->error();
        ASSERT_NE(c.value, nullptr);
        ASSERT_TRUE(c->Is<ast::IntLiteralExpression>());
        EXPECT_EQ(c->As<ast::IntLiteralExpression>()->value, -234);
        EXPECT_EQ(c->As<ast::IntLiteralExpression>()->suffix,
                  ast::IntLiteralExpression::Suffix::kI);
        EXPECT_EQ(c->source.range, (Source::Range{{1u, 1u}, {1u, 6u}}));
    }
}

TEST_F(ParserImplTest, ConstLiteral_Uint) {
    auto p = parser("234u");
    auto c = p->const_literal();
    EXPECT_TRUE(c.matched);
    EXPECT_FALSE(c.errored);
    EXPECT_FALSE(p->has_error()) << p->error();
    ASSERT_NE(c.value, nullptr);
    ASSERT_TRUE(c->Is<ast::IntLiteralExpression>());
    EXPECT_EQ(c->As<ast::IntLiteralExpression>()->value, 234);
    EXPECT_EQ(c->As<ast::IntLiteralExpression>()->suffix, ast::IntLiteralExpression::Suffix::kU);
    EXPECT_EQ(c->source.range, (Source::Range{{1u, 1u}, {1u, 5u}}));
}

TEST_F(ParserImplTest, ConstLiteral_Uint_Negative) {
    auto p = parser("-234u");
    auto c = p->const_literal();
    EXPECT_FALSE(c.matched);
    EXPECT_TRUE(c.errored);
    EXPECT_EQ(p->error(), "1:1: value cannot be represented as 'u32'");
    ASSERT_EQ(c.value, nullptr);
}

TEST_F(ParserImplTest, ConstLiteral_InvalidFloat_IncompleteExponent) {
    auto p = parser("1.0e+");
    auto c = p->const_literal();
    EXPECT_FALSE(c.matched);
    EXPECT_TRUE(c.errored);
    EXPECT_EQ(p->error(), "1:1: incomplete exponent for floating point literal: 1.0e+");
    ASSERT_EQ(c.value, nullptr);
}

struct FloatLiteralTestCase {
    std::string input;
    double expected;
    bool operator==(const FloatLiteralTestCase& other) const {
        return (input == other.input) && std::equal_to<double>()(expected, other.expected);
    }
};

inline std::ostream& operator<<(std::ostream& out, FloatLiteralTestCase data) {
    out << data.input;
    return out;
}

class ParserImplFloatLiteralTest : public ParserImplTestWithParam<FloatLiteralTestCase> {};
TEST_P(ParserImplFloatLiteralTest, Parse) {
    auto params = GetParam();
    SCOPED_TRACE(params.input);
    auto p = parser(params.input);
    auto c = p->const_literal();
    EXPECT_TRUE(c.matched);
    EXPECT_FALSE(c.errored);
    EXPECT_FALSE(p->has_error()) << p->error();
    ASSERT_NE(c.value, nullptr);
    auto* literal = c->As<ast::FloatLiteralExpression>();
    ASSERT_NE(literal, nullptr);
    EXPECT_DOUBLE_EQ(literal->value, params.expected)
        << "\n"
        << "got:      " << std::hexfloat << literal->value << "\n"
        << "expected: " << std::hexfloat << params.expected;
    if (params.input.back() == 'f') {
        EXPECT_EQ(c->As<ast::FloatLiteralExpression>()->suffix,
                  ast::FloatLiteralExpression::Suffix::kF);
    } else {
        EXPECT_EQ(c->As<ast::FloatLiteralExpression>()->suffix,
                  ast::FloatLiteralExpression::Suffix::kNone);
    }
    EXPECT_EQ(c->source.range, (Source::Range{{1u, 1u}, {1u, 1u + params.input.size()}}));
}
using FloatLiteralTestCaseList = std::vector<FloatLiteralTestCase>;

INSTANTIATE_TEST_SUITE_P(ParserImplFloatLiteralTest_Float,
                         ParserImplFloatLiteralTest,
                         testing::ValuesIn(FloatLiteralTestCaseList{
                             {"0.0", 0.0},                        // Zero
                             {"1.0", 1.0},                        // One
                             {"-1.0", -1.0},                      // MinusOne
                             {"1000000000.0", 1e9},               // Billion
                             {"-0.0", std::copysign(0.0, -5.0)},  // NegativeZero
                             {"0.0", MakeDouble(0, 0, 0)},        // Zero
                             {"-0.0", MakeDouble(1, 0, 0)},       // NegativeZero
                             {"1.0", MakeDouble(0, 1023, 0)},     // One
                             {"-1.0", MakeDouble(1, 1023, 0)},    // NegativeOne

                             {"234.e12", 234.e12},
                             {"234.e12f", static_cast<double>(234.e12f)},

                             // Tiny cases
                             {"1e-5000", 0.0},
                             {"-1e-5000", 0.0},
                             {"1e-5000f", 0.0},
                             {"-1e-5000f", 0.0},
                             {"1e-50f", 0.0},
                             {"-1e-50f", 0.0},

                             // Nearly overflow
                             {"1.e308", 1.e308},
                             {"-1.e308", -1.e308},
                             {"1.8e307", 1.8e307},
                             {"-1.8e307", -1.8e307},
                             {"1.798e307", 1.798e307},
                             {"-1.798e307", -1.798e307},
                             {"1.7977e307", 1.7977e307},
                             {"-1.7977e307", -1.7977e307},

                             // Nearly overflow
                             {"1e38f", static_cast<double>(1e38f)},
                             {"-1e38f", static_cast<double>(-1e38f)},
                             {"4.0e37f", static_cast<double>(4.0e37f)},
                             {"-4.0e37f", static_cast<double>(-4.0e37f)},
                             {"3.5e37f", static_cast<double>(3.5e37f)},
                             {"-3.5e37f", static_cast<double>(-3.5e37f)},
                             {"3.403e37f", static_cast<double>(3.403e37f)},
                             {"-3.403e37f", static_cast<double>(-3.403e37f)},
                         }));

const double NegInf = MakeDouble(1, 0x7FF, 0);
const double PosInf = MakeDouble(0, 0x7FF, 0);
FloatLiteralTestCaseList HexFloatCases() {
    return FloatLiteralTestCaseList{
        // Regular numbers
        {"0x0p+0", 0x0p+0},
        {"0x1p+0", 0x1p+0},
        {"0x1p+1", 0x1p+1},
        {"0x1.8p+1", 0x1.8p+1},
        {"0x1.99999ap-4", 0x1.99999ap-4},
        {"0x1p-1", 0x1p-1},
        {"0x1p-2", 0x1p-2},
        {"0x1.8p-1", 0x1.8p-1},
        {"-0x0p+0", -0x0p+0},
        {"-0x1p+0", -0x1p+0},
        {"-0x1p-1", -0x1p-1},
        {"-0x1p-2", -0x1p-2},
        {"-0x1.8p-1", -0x1.8p-1},

        // Large numbers
        {"0x1p+9", 0x1p+9},
        {"0x1p+10", 0x1p+10},
        {"0x1.02p+10", 0x1.02p+10},
        {"-0x1p+9", -0x1p+9},
        {"-0x1p+10", -0x1p+10},
        {"-0x1.02p+10", -0x1.02p+10},

        // Small numbers
        {"0x1p-9", 0x1p-9},
        {"0x1p-10", 0x1p-10},
        {"0x1.02p-3", 0x1.02p-3},
        {"-0x1p-9", -0x1p-9},
        {"-0x1p-10", -0x1p-10},
        {"-0x1.02p-3", -0x1.02p-3},

        // Near lowest non-denorm
        {"0x1p-1020", 0x1p-1020},
        {"0x1p-1021", 0x1p-1021},
        {"-0x1p-1020", -0x1p-1020},
        {"-0x1p-1021", -0x1p-1021},

        {"0x1p-124f", 0x1p-124},
        {"0x1p-125f", 0x1p-125},
        {"-0x1p-124f", -0x1p-124},
        {"-0x1p-125f", -0x1p-125},

        // Lowest non-denorm
        {"0x1p-1022", 0x1p-1022},
        {"-0x1p-1022", -0x1p-1022},

        {"0x1p-126f", 0x1p-126},
        {"-0x1p-126f", -0x1p-126},

        // Denormalized values
        {"0x1p-1023", 0x1p-1023},
        {"0x1p-1024", 0x1p-1024},
        {"0x1p-1025", 0x1p-1025},
        {"0x1p-1026", 0x1p-1026},
        {"-0x1p-1023", -0x1p-1023},
        {"-0x1p-1024", -0x1p-1024},
        {"-0x1p-1025", -0x1p-1025},
        {"-0x1p-1026", -0x1p-1026},
        {"0x1.8p-1023", 0x1.8p-1023},
        {"0x1.8p-1024", 0x1.8p-1024},

        {"0x1p-127f", 0x1p-127},
        {"0x1p-128f", 0x1p-128},
        {"0x1p-129f", 0x1p-129},
        {"0x1p-130f", 0x1p-130},
        {"-0x1p-127f", -0x1p-127},
        {"-0x1p-128f", -0x1p-128},
        {"-0x1p-129f", -0x1p-129},
        {"-0x1p-130f", -0x1p-130},
        {"0x1.8p-127f", 0x1.8p-127},
        {"0x1.8p-128f", 0x1.8p-128},

        // F64 extremities
        {"0x1p-1074", 0x1p-1074},                              // +SmallestDenormal
        {"0x1p-1073", 0x1p-1073},                              // +BiggerDenormal
        {"0x1.ffffffffffffp-1027", 0x1.ffffffffffffp-1027},    // +LargestDenormal
        {"-0x1p-1074", -0x1p-1074},                            // -SmallestDenormal
        {"-0x1p-1073", -0x1p-1073},                            // -BiggerDenormal
        {"-0x1.ffffffffffffp-1027", -0x1.ffffffffffffp-1027},  // -LargestDenormal

        {"0x0.cafebeeff000dp-1022", 0x0.cafebeeff000dp-1022},    // +Subnormal
        {"-0x0.cafebeeff000dp-1022", -0x0.cafebeeff000dp-1022},  // -Subnormal
        {"0x1.2bfaf8p-1052", 0x1.2bfaf8p-1052},                  // +Subnormal
        {"-0x1.2bfaf8p-1052", -0x1.2bfaf8p-1052},                // +Subnormal
        {"0x1.55554p-1055", 0x1.55554p-1055},                    // +Subnormal
        {"-0x1.55554p-1055", -0x1.55554p-1055},                  // -Subnormal

        // F32 extremities
        {"0x1p-149", 0x1p-149},                  // +SmallestDenormal
        {"0x1p-148", 0x1p-148},                  // +BiggerDenormal
        {"0x1.fffffcp-127", 0x1.fffffcp-127},    // +LargestDenormal
        {"-0x1p-149", -0x1p-149},                // -SmallestDenormal
        {"-0x1p-148", -0x1p-148},                // -BiggerDenormal
        {"-0x1.fffffcp-127", -0x1.fffffcp-127},  // -LargestDenormal

        {"0x0.cafebp-129", 0x0.cafebp-129},      // +Subnormal
        {"-0x0.cafebp-129", -0x0.cafebp-129},    // -Subnormal
        {"0x1.2bfaf8p-127", 0x1.2bfaf8p-127},    // +Subnormal
        {"-0x1.2bfaf8p-127", -0x1.2bfaf8p-127},  // -Subnormal
        {"0x1.55554p-130", 0x1.55554p-130},      // +Subnormal
        {"-0x1.55554p-130", -0x1.55554p-130},    // -Subnormal

        // F32 exactly representable
        {"0x1.000002p+0f", 0x1.000002p+0},
        {"0x8.0000fp+0f", 0x8.0000fp+0},
        {"0x8.fffffp+0f", 0x8.fffffp+0},
        {"0x8.00003p+0f", 0x8.00003p+0},
        {"0x2.123p+0f", 0x2.123p+0},
        {"0x2.cafefp+0f", 0x2.cafefp+0},

        // Underflow -> Zero
        {"0x1p-1074", 0.0},  // Exponent underflows
        {"-0x1p-1074", 0.0},
        {"0x1p-5000", 0.0},
        {"-0x1p-5000", 0.0},
        {"0x0.00000000000000000000001p-1022", 0.0},  // Fraction causes underflow
        {"-0x0.0000000000000000000001p-1023", -0.0},
        {"0x0.01p-1073", -0.0},
        {"-0x0.01p-1073", -0.0},  // Fraction causes additional underflow

        {"0x1.0p-9223372036854774784", 0},  // -(INT64_MAX - 1023) (smallest valid exponent)

        // Zero with non-zero exponent -> Zero
        {"0x0p+0", 0.0},
        {"0x0p+1", 0.0},
        {"0x0p-1", 0.0},
        {"0x0p+9999999999", 0.0},
        {"0x0p-9999999999", 0.0},
        // Same, but with very large positive exponents that would cause overflow
        // if the mantissa were non-zero.
        {"0x0p+10000000000000000000", 0.0},    // 10 quintillion   (10,000,000,000,000,000,000)
        {"0x0p+100000000000000000000", 0.0},   // 100 quintillion (100,000,000,000,000,000,000)
        {"-0x0p+100000000000000000000", 0.0},  // As above 2, but negative mantissa
        {"-0x0p+1000000000000000000000", 0.0},
        {"0x0.00p+10000000000000000000", 0.0},  // As above 4, but with fractional part
        {"0x0.00p+100000000000000000000", 0.0},
        {"-0x0.00p+100000000000000000000", 0.0},
        {"-0x0.00p+1000000000000000000000", 0.0},
        {"0x0p-10000000000000000000", 0.0},  // As above 8, but with negative exponents
        {"0x0p-100000000000000000000", 0.0},
        {"-0x0p-100000000000000000000", 0.0},
        {"-0x0p-1000000000000000000000", 0.0},
        {"0x0.00p-10000000000000000000", 0.0},
        {"0x0.00p-100000000000000000000", 0.0},
        {"-0x0.00p-100000000000000000000", 0.0},
        {"-0x0.00p-1000000000000000000000", 0.0},

        // Test parsing
        {"0x0p0", 0.0},
        {"0x0p-0", 0.0},
        {"0x0p+000", 0.0},
        {"0x00000000000000p+000000000000000", 0.0},
        {"0x00000000000000p-000000000000000", 0.0},
        {"0x00000000000001p+000000000000000", 1.0},
        {"0x00000000000001p-000000000000000", 1.0},
        {"0x0000000000000000000001.99999ap-000000000000000004", 0.10000000149011612},
        {"0x2p+0", 2.0},
        {"0xFFp+0", 255.0},
        {"0x0.8p+0", 0.5},
        {"0x0.4p+0", 0.25},
        {"0x0.4p+1", 2 * 0.25},
        {"0x0.4p+2", 4 * 0.25},
        {"0x123Ep+1", 9340.0},
        {"-0x123Ep+1", -9340.0},
        {"0x1a2b3cP12", 7.024656384e+09},
        {"-0x1a2b3cP12", -7.024656384e+09},

        // Examples without a binary exponent part.
        {"0x1.", 1.0},
        {"0x.8", 0.5},
        {"0x1.8", 1.5},
        {"-0x1.", -1.0},
        {"-0x.8", -0.5},
        {"-0x1.8", -1.5},

        // Examples with a binary exponent and a 'f' suffix.
        {"0x1.p0f", 1.0},
        {"0x.8p2f", 2.0},
        {"0x1.8p-1f", 0.75},
        {"0x2p-2f", 0.5},  // No binary point
        {"-0x1.p0f", -1.0},
        {"-0x.8p2f", -2.0},
        {"-0x1.8p-1f", -0.75},
        {"-0x2p-2f", -0.5},  // No binary point
    };
}
INSTANTIATE_TEST_SUITE_P(ParserImplFloatLiteralTest_HexFloat,
                         ParserImplFloatLiteralTest,
                         testing::ValuesIn(HexFloatCases()));

// Now test all the same hex float cases, but with 0X instead of 0x
template <typename ARR>
std::vector<FloatLiteralTestCase> UpperCase0X(const ARR& cases) {
    std::vector<FloatLiteralTestCase> result;
    result.reserve(cases.size());
    for (const auto& c : cases) {
        result.emplace_back(c);
        auto& input = result.back().input;
        const auto where = input.find("0x");
        if (where != std::string::npos) {
            input[where + 1] = 'X';
        }
    }
    return result;
}

using UpperCase0XTest = ::testing::Test;
TEST_F(UpperCase0XTest, Samples) {
    const auto cases = FloatLiteralTestCaseList{
        {"absent", 0.0}, {"0x", 1.0},      {"0X", 2.0},      {"-0x", 3.0},
        {"-0X", 4.0},    {"  0x1p1", 5.0}, {"  -0x1p", 6.0}, {" examine ", 7.0}};
    const auto expected = FloatLiteralTestCaseList{
        {"absent", 0.0}, {"0X", 1.0},      {"0X", 2.0},      {"-0X", 3.0},
        {"-0X", 4.0},    {"  0X1p1", 5.0}, {"  -0X1p", 6.0}, {" examine ", 7.0}};

    auto result = UpperCase0X(cases);
    EXPECT_THAT(result, ::testing::ElementsAreArray(expected));
}

INSTANTIATE_TEST_SUITE_P(ParserImplFloatLiteralTest_HexFloat_UpperCase0X,
                         ParserImplFloatLiteralTest,
                         testing::ValuesIn(UpperCase0X(HexFloatCases())));

// <error, source>
using InvalidLiteralTestCase = std::tuple<const char*, const char*>;

class ParserImplInvalidLiteralTest : public ParserImplTestWithParam<InvalidLiteralTestCase> {};
TEST_P(ParserImplInvalidLiteralTest, Parse) {
    auto* error = std::get<0>(GetParam());
    auto* source = std::get<1>(GetParam());
    auto p = parser(source);
    auto c = p->const_literal();
    EXPECT_FALSE(c.matched);
    EXPECT_TRUE(c.errored);
    EXPECT_EQ(p->error(), std::string(error));
    ASSERT_EQ(c.value, nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    HexFloatMantissaTooLarge,
    ParserImplInvalidLiteralTest,
    testing::Combine(testing::Values("1:1: mantissa is too large for hex float"),
                     testing::ValuesIn(std::vector<const char*>{
                         "0x1.ffffffffffffffff8p0",
                         "0x1f.fffffffffffffff8p0",
                         "0x1ff.ffffffffffffff8p0",
                         "0x1fff.fffffffffffff8p0",
                         "0x1ffff.ffffffffffff8p0",
                         "0x1fffff.fffffffffff8p0",
                         "0x1ffffff.ffffffffff8p0",
                         "0x1fffffff.fffffffff8p0",
                         "0x1ffffffff.ffffffff8p0",
                         "0x1fffffffff.fffffff8p0",
                         "0x1ffffffffff.ffffff8p0",
                         "0x1fffffffffff.fffff8p0",
                         "0x1ffffffffffff.ffff8p0",
                         "0x1fffffffffffff.fff8p0",
                         "0x1ffffffffffffff.ff8p0",
                         "0x1ffffffffffffffff.8p0",
                         "0x1ffffffffffffffff8.p0",
                     })));

INSTANTIATE_TEST_SUITE_P(
    HexFloatExponentTooLarge,
    ParserImplInvalidLiteralTest,
    testing::Combine(testing::Values("1:1: exponent is too large for hex float"),
                     testing::ValuesIn(std::vector<const char*>{
                         "0x1p+9223372036854774785",
                         "0x1p-9223372036854774785",
                         "0x1p+18446744073709551616",
                         "0x1p-18446744073709551616",
                     })));

INSTANTIATE_TEST_SUITE_P(
    HexFloatMissingExponent,
    ParserImplInvalidLiteralTest,
    testing::Combine(testing::Values("1:1: expected an exponent value for hex float"),
                     testing::ValuesIn(std::vector<const char*>{
                         // Lower case p
                         "0x0p",
                         "0x0p+",
                         "0x0p-",
                         "0x1.0p",
                         "0x0.1p",
                         // Upper case p
                         "0x0P",
                         "0x0P+",
                         "0x0P-",
                         "0x1.0P",
                         "0x0.1P",
                     })));

INSTANTIATE_TEST_SUITE_P(
    HexNaNAFloat,
    ParserImplInvalidLiteralTest,
    testing::Combine(testing::Values("1:1: value cannot be represented as 'abstract-float'"),
                     testing::ValuesIn(std::vector<const char*>{
                         "0x1.8p+1024",
                         "0x1.0002p+1024",
                         "0x1.0018p+1024",
                         "0x1.01ep+1024",
                         "0x1.fffffep+1024",
                         "-0x1.8p+1024",
                         "-0x1.0002p+1024",
                         "-0x1.0018p+1024",
                         "-0x1.01ep+1024",
                         "-0x1.fffffep+1024",
                     })));

INSTANTIATE_TEST_SUITE_P(
    HexNaNF32,
    ParserImplInvalidLiteralTest,
    testing::Combine(testing::Values("1:1: value cannot be represented as 'f32'"),
                     testing::ValuesIn(std::vector<const char*>{
                         "0x1.8p+128f",
                         "0x1.0002p+128f",
                         "0x1.0018p+128f",
                         "0x1.01ep+128f",
                         "0x1.fffffep+128f",
                         "-0x1.8p+128f",
                         "-0x1.0002p+128f",
                         "-0x1.0018p+128f",
                         "-0x1.01ep+128f",
                         "-0x1.fffffep+128f",
                     })));

INSTANTIATE_TEST_SUITE_P(
    HexOverflowAFloat,
    ParserImplInvalidLiteralTest,
    testing::Combine(testing::Values("1:1: value cannot be represented as 'abstract-float'"),
                     testing::ValuesIn(std::vector<const char*>{
                         "0x1p+1024",
                         "-0x1p+1024",
                         "0x1.1p+1024",
                         "-0x1.1p+1024",
                         "0x1p+1025",
                         "-0x1p+1025",
                         "0x32p+1023",
                         "-0x32p+1023",
                         "0x32p+5000",
                         "-0x32p+5000",
                         "0x1.0p9223372036854774784",
                         "-0x1.0p9223372036854774784",
                     })));

INSTANTIATE_TEST_SUITE_P(
    HexOverflowF32,
    ParserImplInvalidLiteralTest,
    testing::Combine(testing::Values("1:1: value cannot be represented as 'f32'"),
                     testing::ValuesIn(std::vector<const char*>{
                         "0x1p+128f",
                         "-0x1p+128f",
                         "0x1.1p+128f",
                         "-0x1.1p+128f",
                         "0x1p+129f",
                         "-0x1p+129f",
                         "0x32p+127f",
                         "-0x32p+127f",
                         "0x32p+500f",
                         "-0x32p+500f",
                     })));

INSTANTIATE_TEST_SUITE_P(
    HexNotExactlyRepresentableF32,
    ParserImplInvalidLiteralTest,
    testing::Combine(testing::Values("1:1: value cannot be exactly represented as 'f32'"),
                     testing::ValuesIn(std::vector<const char*>{
                         "0x1.000001p+0f",    // Quantizes to 0x1.0p+0
                         "0x8.0000f8p+0f",    // Quantizes to 0x8.0000fp+0
                         "0x8.000038p+0f",    // Quantizes to 0x8.00003p+0
                         "0x2.cafef00dp+0f",  // Quantizes to 0x2.cafefp+0
                     })));

INSTANTIATE_TEST_SUITE_P(
    DecOverflowAFloat,
    ParserImplInvalidLiteralTest,
    testing::Combine(testing::Values("1:1: value cannot be represented as 'abstract-float'"),
                     testing::ValuesIn(std::vector<const char*>{
                         "1.e309",
                         "-1.e309",
                         "1.8e308",
                         "-1.8e308",
                         "1.798e308",
                         "-1.798e308",
                         "1.7977e308",
                         "-1.7977e308",
                         "1.2e+5000",
                         "-1.2e+5000",
                     })));

INSTANTIATE_TEST_SUITE_P(
    DecOverflowF32,
    ParserImplInvalidLiteralTest,
    testing::Combine(testing::Values("1:1: value cannot be represented as 'f32'"),
                     testing::ValuesIn(std::vector<const char*>{
                         "1e39f",
                         "-1e39f",
                         "4.0e38f",
                         "-4.0e38f",
                         "3.5e38f",
                         "-3.5e38f",
                         "3.403e38f",
                         "-3.403e38f",
                         "1.2e+256f",
                         "-1.2e+256f",
                     })));

TEST_F(ParserImplTest, ConstLiteral_FloatHighest) {
    const auto highest = std::numeric_limits<float>::max();
    const auto expected_highest = 340282346638528859811704183484516925440.0f;
    if (highest < expected_highest || highest > expected_highest) {
        GTEST_SKIP() << "std::numeric_limits<float>::max() is not as expected for "
                        "this target";
    }
    auto p = parser("340282346638528859811704183484516925440.0");
    auto c = p->const_literal();
    EXPECT_TRUE(c.matched);
    EXPECT_FALSE(c.errored);
    EXPECT_FALSE(p->has_error()) << p->error();
    ASSERT_NE(c.value, nullptr);
    ASSERT_TRUE(c->Is<ast::FloatLiteralExpression>());
    EXPECT_DOUBLE_EQ(c->As<ast::FloatLiteralExpression>()->value,
                     std::numeric_limits<float>::max());
    EXPECT_EQ(c->As<ast::FloatLiteralExpression>()->suffix,
              ast::FloatLiteralExpression::Suffix::kNone);
    EXPECT_EQ(c->source.range, (Source::Range{{1u, 1u}, {1u, 42u}}));
}

TEST_F(ParserImplTest, ConstLiteral_FloatLowest) {
    // Some compilers complain if you test floating point numbers for equality.
    // So say it via two inequalities.
    const auto lowest = std::numeric_limits<float>::lowest();
    const auto expected_lowest = -340282346638528859811704183484516925440.0f;
    if (lowest < expected_lowest || lowest > expected_lowest) {
        GTEST_SKIP() << "std::numeric_limits<float>::lowest() is not as expected for "
                        "this target";
    }

    auto p = parser("-340282346638528859811704183484516925440.0");
    auto c = p->const_literal();
    EXPECT_TRUE(c.matched);
    EXPECT_FALSE(c.errored);
    EXPECT_FALSE(p->has_error()) << p->error();
    ASSERT_NE(c.value, nullptr);
    ASSERT_TRUE(c->Is<ast::FloatLiteralExpression>());
    EXPECT_DOUBLE_EQ(c->As<ast::FloatLiteralExpression>()->value,
                     std::numeric_limits<float>::lowest());
    EXPECT_EQ(c->As<ast::FloatLiteralExpression>()->suffix,
              ast::FloatLiteralExpression::Suffix::kNone);
    EXPECT_EQ(c->source.range, (Source::Range{{1u, 1u}, {1u, 43u}}));
}

TEST_F(ParserImplTest, ConstLiteral_True) {
    auto p = parser("true");
    auto c = p->const_literal();
    EXPECT_TRUE(c.matched);
    EXPECT_FALSE(c.errored);
    EXPECT_FALSE(p->has_error()) << p->error();
    ASSERT_NE(c.value, nullptr);
    ASSERT_TRUE(c->Is<ast::BoolLiteralExpression>());
    EXPECT_TRUE(c->As<ast::BoolLiteralExpression>()->value);
    EXPECT_EQ(c->source.range, (Source::Range{{1u, 1u}, {1u, 5u}}));
}

TEST_F(ParserImplTest, ConstLiteral_False) {
    auto p = parser("false");
    auto c = p->const_literal();
    EXPECT_TRUE(c.matched);
    EXPECT_FALSE(c.errored);
    EXPECT_FALSE(p->has_error()) << p->error();
    ASSERT_NE(c.value, nullptr);
    ASSERT_TRUE(c->Is<ast::BoolLiteralExpression>());
    EXPECT_FALSE(c->As<ast::BoolLiteralExpression>()->value);
    EXPECT_EQ(c->source.range, (Source::Range{{1u, 1u}, {1u, 6u}}));
}

TEST_F(ParserImplTest, ConstLiteral_NoMatch) {
    auto p = parser("another-token");
    auto c = p->const_literal();
    EXPECT_FALSE(c.matched);
    EXPECT_FALSE(c.errored);
    EXPECT_FALSE(p->has_error()) << p->error();
    ASSERT_EQ(c.value, nullptr);
}

}  // namespace
}  // namespace tint::reader::wgsl
