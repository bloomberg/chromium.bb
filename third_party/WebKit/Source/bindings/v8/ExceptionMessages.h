/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ExceptionMessages_h
#define ExceptionMessages_h

#include "wtf/MathExtras.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Decimal;

class ExceptionMessages {
public:
    enum BoundType {
        InclusiveBound,
        ExclusiveBound,
    };

    static String failedToConstruct(const String& type, const String& detail = String());
    static String failedToEnumerate(const String& type, const String& detail = String());
    static String failedToExecute(const String& method, const String& type, const String& detail = String());
    static String failedToGet(const String& property, const String& type, const String& detail);
    static String failedToSet(const String& property, const String& type, const String& detail);
    static String failedToDelete(const String& property, const String& type, const String& detail);
    static String failedToGetIndexed(const String& type, const String& detail);
    static String failedToSetIndexed(const String& type, const String& detail);
    static String failedToDeleteIndexed(const String& type, const String& detail);

    static String incorrectPropertyType(const String& property, const String& detail);

    static String argumentNullOrIncorrectType(int argumentIndex, const String& expectedType);

    // If  > 0, the argument index that failed type check (1-indexed.)
    // If == 0, a (non-argument) value (e.g., a setter) failed the same check.
    static String notAnArrayTypeArgumentOrValue(int argumentIndex);
    static String notASequenceTypeProperty(const String& propertyName);
    static String notAFiniteNumber(double value, const char* name = "value provided");
    static String notAFiniteNumber(const Decimal& value, const char* name = "value provided");

    static String notEnoughArguments(unsigned expected, unsigned providedleastNumMandatoryParams);

    static String readOnly(const char* detail = 0);

    template <typename NumberType>
    static String indexExceedsMaximumBound(const char* name, NumberType given, NumberType bound)
    {
        bool eq = given == bound;
        StringBuilder result;
        result.append("The ");
        result.append(name);
        result.append(" provided (");
        result.append(formatNumber(given));
        result.append(") is greater than ");
        result.append(eq ? "or equal to " : "");
        result.append("the maximum bound (");
        result.append(formatNumber(bound));
        result.append(").");
        return result.toString();
    }

    template <typename NumberType>
    static String indexExceedsMinimumBound(const char* name, NumberType given, NumberType bound)
    {
        bool eq = given == bound;
        StringBuilder result;
        result.append("The ");
        result.append(name);
        result.append(" provided (");
        result.append(formatNumber(given));
        result.append(") is less than ");
        result.append(eq ? "or equal to " : "");
        result.append("the minimum bound (");
        result.append(formatNumber(bound));
        result.append(").");
        return result.toString();
    }

    template <typename NumberType>
    static String indexOutsideRange(const char* name, NumberType given, NumberType lowerBound, BoundType lowerType, NumberType upperBound, BoundType upperType)
    {
        StringBuilder result;
        result.append("The ");
        result.append(name);
        result.append(" provided (");
        result.append(formatNumber(given));
        result.append(") is outside the range ");
        result.append(lowerType == ExclusiveBound ? '(' : '[');
        result.append(formatNumber(lowerBound));
        result.append(", ");
        result.append(formatNumber(upperBound));
        result.append(upperType == ExclusiveBound ? ')' : ']');
        result.append('.');
        return result.toString();
    }

    template <typename NumType>
    static String formatNumber(NumType number)
    {
        return formatFiniteNumber(number);
    }

private:
    static String ordinalNumber(int number);


    template <typename NumType>
    static String formatFiniteNumber(NumType number)
    {
        if (number > 1e20 || number < -1e20)
            return String::format("%e", 1.0*number);
        return String::number(number);
    }

    template <typename NumType>
    static String formatPotentiallyNonFiniteNumber(NumType number)
    {
        if (std::isnan(number))
            return "NaN";
        if (std::isinf(number))
            return number > 0 ? "Infinity" : "-Infinity";
        if (number > 1e20 || number < -1e20)
            return String::format("%e", number);
        return String::number(number);
    }
};

template <> String ExceptionMessages::formatNumber<float>(float number);
template <> String ExceptionMessages::formatNumber<double>(double number);

} // namespace WebCore

#endif // ExceptionMessages_h
