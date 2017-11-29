# Property classes

This directory contains implementations for CSS property classes, as well as
Utils files containing functions commonly used by the property classes.

[TOC]

## Overview

CSSProperty is the base class which all property implementations are derived
from. It contains all the methods that can be called on a property class, and a
default implementation for each. The .cpp implementation files for
CSSProperty are split between the generated code (CSSProperty.cpp) and the
hand written code (CSSPropertyBaseCustom.cpp).

The methods that are overriden from the base class by a property class depends
on the functionality required by that property. Methods may have a working
default implementation in the base class, or may assert that this default
implementation is not reached.

A derived CSS property class (<Property|GroupName\>) represents a
single CSS property or a group of CSS properties that share implementation
logic. Property groups can be determined from
[CSSProperties.json5](https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/css/CSSProperties.json5).
Properties that are a part of a group define the group class name as
'property_class' here.

Examples:

*   A single property class: the `LineHeight` class is used only by
    the `line-height` property
*   A group of properties that share logic: the `ImageSource`
    class is shared by the `border-image-source` and
    `-webkit-mask-box-image-source` properties.

## Special property classes

### Shorthand properties

Shorthand properties only exist in the context of parsing and serialization.
Therefore only a subset of methods may be implemented by shorthand properties,
e.g. ParseShorthand.

### Descriptors

Descriptors define the characteristics of an at-rule. For example, @viewport is
an at-rule, and width is a valid descriptor for @viewport. Within the context of
@viewport, width is a shorthand comprising the min-width and max-width of the
viewport. From this example we can see that a descriptor is not the same as a
CSS property with the same name. Sometimes descriptors and CSS properties with
the same name are handled together, but that should not be taken to mean that
they are the same thing. Fixing this possible source of confusion is an open
issue crbug.com/752745.

### Variable

The Variable rule is not a true CSS property, but is treated
as such in places for convience. It does not appear in CSSProperties.json5; thus
its property class header needed to be hand-written & manually added to the
list of property classes by make_css_property_base.py. Those hand-written
headers are in this directory.

## How to add a new property class

1.  Add a .cpp file to this directory named
    `<Property|GroupName>.cpp`
2.  Implement the property class in the .cpp file
    1.  Add `#include "core/css/properties/<longhands|shorthands>/<Property|GroupName>.h"`
        (this will be a generated file).
    2.  Implement the required methods on the property class.
3.  If logic is required by multiple property classes you may need to create a
    new Utils file.
4.  Add the new property to `core/css/CSSProperties.json5`. Ensure that you
    include the 'property_class' flag and the 'property_methods' flag so that
    the property class files are generated correctly (see
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
converts the existing line-height property to use the CSSProperty design.
This new line-height property class only implements the ParseSingleValue method,
using
[CSSPropertyFontUtils.cpp](https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/css/properties/CSSPropertyFontUtils.h)
to access shared font logic.

## Status

Eventually, all logic pertaining to a single property will be found only
within its CSS property class where possible.

Currently (September 1 2017) the code base is in a transitional state and
property specific logic is still scattered around the code base. See Project
Ribbon
[tracking bug](https://bugs.chromium.org/p/chromium/issues/detail?id=545324) and
[design doc](https://docs.google.com/document/d/1ywjUTmnxF5FXlpUTuLpint0w4TdSsjJzdWJqmhNzlss/edit#heading=h.1ckibme4i78b)
for details of progress.
