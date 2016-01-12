// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/LengthValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "wtf/HashMap.h"

namespace blink {

namespace {

using UnitTable = HashMap<String, LengthValue::LengthUnit>;

UnitTable createStrToLenUnitTable()
{
    UnitTable table;
    table.set(String("px"), LengthValue::Px);
    table.set(String("percent"), LengthValue::Percent);
    table.set(String("%"), LengthValue::Percent);
    table.set(String("em"), LengthValue::Em);
    table.set(String("ex"), LengthValue::Ex);
    table.set(String("ch"), LengthValue::Ch);
    table.set(String("rem"), LengthValue::Rem);
    table.set(String("vw"), LengthValue::Vw);
    table.set(String("vh"), LengthValue::Vh);
    table.set(String("vmin"), LengthValue::Vmin);
    table.set(String("vmax"), LengthValue::Vmax);
    table.set(String("cm"), LengthValue::Cm);
    table.set(String("mm"), LengthValue::Mm);
    table.set(String("in"), LengthValue::In);
    table.set(String("pc"), LengthValue::Pc);
    table.set(String("pt"), LengthValue::Pt);
    return table;
}

UnitTable& typeTable()
{
    DEFINE_STATIC_LOCAL(UnitTable, typeTable, (createStrToLenUnitTable()));
    return typeTable;
}

} // namespace

LengthValue* LengthValue::parse(const String& cssString)
{
    // TODO: Implement
    return nullptr;
}

LengthValue* LengthValue::fromValue(double value, const String& typeStr)
{
    return nullptr;
    // return SimpleLength::create(value, lengthUnitFromName(typeStr)).get();
}

LengthValue* LengthValue::add(const LengthValue* other, ExceptionState& exceptionState)
{
    if (type() == other->type())
        return addInternal(other, exceptionState);

    exceptionState.throwTypeError("Not implemented yet");
    return nullptr;
}

LengthValue* LengthValue::subtract(const LengthValue* other, ExceptionState& exceptionState)
{
    if (type() == other->type())
        return subtractInternal(other, exceptionState);

    exceptionState.throwTypeError("Not implemented yet");
    return nullptr;
}

LengthValue* LengthValue::multiply(double x, ExceptionState& exceptionState)
{
    return multiplyInternal(x, exceptionState);
}

LengthValue* LengthValue::divide(double x, ExceptionState& exceptionState)
{
    return divideInternal(x, exceptionState);
}

LengthValue::LengthUnit LengthValue::lengthUnitFromName(const String& str)
{
    if (typeTable().contains(str.lower()))
        return typeTable().get(str.lower());
    return LengthUnit::Count;
}

const String& LengthValue::lengthTypeToString(LengthValue::LengthUnit unit)
{
    DEFINE_STATIC_LOCAL(const String, PxStr, ("px"));
    DEFINE_STATIC_LOCAL(const String, PercentStr, ("%"));
    DEFINE_STATIC_LOCAL(const String, EmStr, ("em"));
    DEFINE_STATIC_LOCAL(const String, ExStr, ("ex"));
    DEFINE_STATIC_LOCAL(const String, ChStr, ("ch"));
    DEFINE_STATIC_LOCAL(const String, RemStr, ("rem"));
    DEFINE_STATIC_LOCAL(const String, VwStr, ("vw"));
    DEFINE_STATIC_LOCAL(const String, VhStr, ("vh"));
    DEFINE_STATIC_LOCAL(const String, VminStr, ("vmin"));
    DEFINE_STATIC_LOCAL(const String, VmaxStr, ("vmax"));
    DEFINE_STATIC_LOCAL(const String, CmStr, ("cm"));
    DEFINE_STATIC_LOCAL(const String, MmStr, ("mm"));
    DEFINE_STATIC_LOCAL(const String, InStr, ("in"));
    DEFINE_STATIC_LOCAL(const String, PcStr, ("pc"));
    DEFINE_STATIC_LOCAL(const String, PtStr, ("pt"));

    switch (unit) {
    case Px:
        return PxStr;
    case Percent:
        return PercentStr;
    case Em:
        return EmStr;
    case Ex:
        return ExStr;
    case Ch:
        return ChStr;
    case Rem:
        return RemStr;
    case Vw:
        return VwStr;
    case Vh:
        return VhStr;
    case Vmin:
        return VminStr;
    case Vmax:
        return VmaxStr;
    case Cm:
        return CmStr;
    case Mm:
        return MmStr;
    case In:
        return InStr;
    case Pc:
        return PcStr;
    case Pt:
        return PtStr;
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
}

LengthValue* LengthValue::addInternal(const LengthValue*, ExceptionState&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

LengthValue* LengthValue::subtractInternal(const LengthValue*, ExceptionState&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

LengthValue* LengthValue::multiplyInternal(double, ExceptionState&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

LengthValue* LengthValue::divideInternal(double, ExceptionState&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace blink
