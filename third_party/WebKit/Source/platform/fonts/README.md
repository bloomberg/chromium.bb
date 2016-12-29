# Blink's Text Stack #

This README serves as an documentation entry point of Blink's text stack.

This README can be viewed in formatted form [here](https://chromium.googlesource.com/chromium/src/+/master/third_party/WebKit/Source/platform/fonts/README.md).

## Overview ##

Blink's text stack covers those functional parts of the layout engine that
process CSS-styled HTML text. From source document to visual output this rougly
comprises the following stages:

* [Processing CSS style information into a font definition](#Mapping-CSS-Style-to-Font-Objects)
* [Using this font definition for matching against available web and system fonts](#Font-Matching)
* [Segmenting text into portions suitable for shaping](#Run-Segmentation)
* [Looking up elements from the previously shaped entries in the word cache](#Word-Cache)
* [Using the matched font for shaping and mapping from characters to glyphs](#Text-Shaping)
* [Font fallback](#Font-Fallback)

## Mapping CSS Style to Font Objects ##

TODO(drott): Describe steps from `ComputedStyle`, `FontBuilder` to
`FontDescription` and `Font` objects.

## Font Matching ##

TODO(drott): Describe font matching against system fonts in platform specific
implementations as well as matching against web fonts in `FontStyleMatcher`.

## Word Cache ##

TODO(drott,eae): Describe at which level the word cache hooks in, how the cache
keys are calculated and its approach of word and CJK segmentation.

## Run Segmentation ##

TODO(drott): Describe purpose and run segmentation approach of `RunSegmenter`.

## Text Shaping ##

The low level shaping implementation is
in
[shaping/HarfBuzzShaper.h](https://cs.chromium.org/chromium/src/third_party/WebKit/Source/platform/fonts/shaping/HarfBuzzShaper.h) and
[shaping/HarfBuzzShaper.cpp](https://cs.chromium.org/chromium/src/third_party/WebKit/Source/platform/fonts/shaping/HarfBuzzShaper.cpp)

Shaping text runs is split into several
stages: [Run segmentation](#Run-Segmentation), shaping the initial segment,
identify shaped and non-shaped sequences of the shaping result, and processing
sub-runs by trying to shape them with a fallback font until the last resort font
is reached.

If caps formatting is requested, an additional lowercase/uppercase
segmentation stage is required. In this stage, OpenType features in the font
are matched against the requested formatting and formatting is synthesized as
required by the CSS Level 3 Fonts Module.

Below we will go through one example - for simplicity without caps formatting -
to illustrate the process: The following is a run of vertical text to be
shaped. After run segmentation in `RunSegmenter` it is split into 4 segments. The
segments indicated by the segementation results showing the script, orientation
information and small caps handling of the individual segment. The Japanese text
at the beginning has script "Hiragana", does not need rotation when laid out
vertically and does not need uppercasing when small caps is requested.

```
0 い
1 ろ
2 は USCRIPT_HIRAGANA,
    OrientationIterator::OrientationKeep,
    SmallCapsIterator::SmallCapsSameCase

3 a
4 ̄ (Combining Macron)
5 a
6 A USCRIPT_LATIN,
    OrientationIterator::OrientationRotateSideways,
    SmallCapsIterator::SmallCapsUppercaseNeeded

7 い
8 ろ
9 は USCRIPT_HIRAGANA,
     OrientationIterator::OrientationKeep,
     SmallCapsIterator::SmallCapsSameCase
```

Let's assume the CSS for this text run is as follows:
    `font-family: "Heiti SC", Tinos, sans-serif;`
where Tinos is a web font, defined as a composite font, with two sub ranges,
one for Latin `U+00-U+FF` and one unrestricted unicode-range.

`FontFallbackIterator` provides the shaper with Heiti SC, then Tinos of the
restricted unicode-range, then the unrestricted full unicode-range Tinos,
then a system sans-serif.

The initial segment 0-2 to the shaper, together with the segmentation
properties and the initial Heiti SC font. Characters 0-2 are shaped
successfully with Heiti SC. The next segment, 3-5 is passed to the shaper.
The shaper attempts to shape it with Heiti SC, which fails for the Combining
Macron. So the shaping result for this segment would look similar to this.

```
Glyphpos: 0 1 2 3
Cluster:  0 0 2 3
Glyph:    a x a A (where x is .notdef)
```

Now in the `extractShapeResults()` step we notice that there is more work to
do, since Heiti SC does not have a glyph for the Combining Macron combined
with an a. So, this cluster together with a Todo item for switching to the
next font is put into `HolesQueue`.

After shaping the initial segment, the remaining items in the `HolesQueue` are
processed, picking them from the head of the queue. So, first, the next font is
requested from the `FontFallbackIterator`. In this case, Tinos (for the range
`U+00-U+FF`) comes back. Shaping using this font, assuming it is subsetted,
fails again since there is no combining mark available. This triggers requesting
yet another font. This time, the Tinos font for the full range. With this,
shaping succeeds with the following HarfBuzz result:

```
Glyphpos 0 1 2 3
Cluster: 0 0 2 3
Glyph:   a ◌̄ a A (with glyph coordinates placing the ◌̄ above the first a)
```

Now this sub run is successfully processed and can be appended to
`ShapeResult`. A new `ShapeResult::RunInfo` is created. The logic in
`ShapeResult::insertRun` then takes care of merging the shape result into the
right position the vector of `RunInfo`s in `ShapeResult`.

Shaping then continues analogously for the remaining Hiragana Japanese sub-run,
and the result is inserted into `ShapeResult` as well.

## Font Fallback ##

TODO(drott): Describe when font fallback is invoked, and how
`FontFallbackIterator` cycles through fallback fonts during shaping.
