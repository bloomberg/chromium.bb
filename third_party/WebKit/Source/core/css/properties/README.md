# Property APIs

This directory contains implementations for CSS property APIs, as well as Utils
files containing functions commonly used by the property APIs.

A CSS property API represents a single CSS property or a group of CSS
properties, and defines the logic for that property or group of properties.

Examples:

*   A single property API: the `CSSPropertyAPILineHeight` class is used only by
    the `line-height` property
*   A group of properties that share logic: the `CSSPropertyAPIImage` class
    is shared by the `border-image-source` and `list-style-image` properties.

Status (March 16 2017): Eventually, all logic pertaining to a single property
should be found only within its CSS property API. Currently, the code base is in
a transitional state and property specific logic is still scattered around the
code base. See Project Ribbon
[tracking bug](https://bugs.chromium.org/p/chromium/issues/detail?id=545324) and
[design doc](https://docs.google.com/document/d/1ywjUTmnxF5FXlpUTuLpint0w4TdSsjJzdWJqmhNzlss/edit#heading=h.1ckibme4i78b)
for details of progress.

## Methods implemented by each property API

Methods implemented by a property API depends on whether it is a longhand or shorthand property.
They implement different methods because a shorthand property will cease to exist after parsing is done.
It will not be relevant in subsequent operations, except for serialization.

Each <LonghandProperty> has a property API called CSSPropertyAPI<LonghandProperty> and each <ShorthandProperty>
has a property API called CSSShorthandPropertyAPI<ShorthandProperty>.

1.  CSSPropertyAPI<LonghandProperty>
    Aims to implement all property-specific logic for this longhand property. Currently(7/6/2017) it implements:
    1. static const CSSValue* ParseSingleValue(CSSParserTokenRange&, const CSSParserContext&, const CSSParserLocalContext&);
       - Parses a single CSS property and returns the corresponding CSSValue. If the input is invalid it returns nullptr.

2.  CSSShorthandPropertyAPI<ShorthandProperty>
    Aims to implement all property-specific logic for this shorthand property. Currently(7/6/2017) it implements:
    1. static bool ParseShorthand(bool important, CSSParserTokenRange&, const CSSParserContext*, HeapVector<CSSProperty, 256>& properties);
       - Returns true if the property can be parsed as a shorthand. It also adds parsed properties to the `properties` set.


## How to add a new property API

1.  Add a .cpp file to this directory named
    `CSSPropertyAPI<Property/GroupName>.cpp`
2.  Implement the property API in the .cpp file
    1.  Add `#include "core/css/properties/CSSPropertyAPI<Property/GroupName>.h"`
        (this will be a generated file)
    2.  Implement the required methods on the API.
3.  If logic is required by multiple property APIs you may need to create a new
    Utils file.
4.  Add the new property to `core/css/CSSProperties.json5`. Ensure that you
    include the 'api_class' flag and the 'api_methods' flag so that the API
    files are generated correctly (see
    [CSSProperties.json5](https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/css/CSSProperties.json5)
    for more details)
5.  Add new files to BUILD files
    1.  Add the new .cpp file to
        [core/css/BUILD.gn](https://codesearch.chromium.org/chromium/src/third_party/WebKit/Source/core/css/BUILD.gn)
        in the `blink_core_sources` target's `sources` parameter
    2.  Add the generated .h file to
        [core/BUILD.gn](https://codesearch.chromium.org/chromium/src/third_party/WebKit/Source/core/BUILD.gn)
        in the `css_properties` target's `outputs` parameter

See [this example CL](https://codereview.chromium.org/2735093005), which
converts the existing line-height property to use the CSSPropertyAPI design.
This new line-height property API only implements the ParseSingleValue method,
using
[CSSPropertyFontUtils.cpp](https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/css/properties/CSSPropertyFontUtils.h)
to access shared font logic.
