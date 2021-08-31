# AVIF Test Files

[TOC]

## Instructions

To add, update or remove a test file, please update the list below.

Please provide full reference and steps to generate the test file so that
any people can regenerate or update the file in the future.

## List of Test Files

### red-(full|limited)-range-(420|422|444)-(8|10|12)bpc.avif
These are all generated from red.png with the appropriate avifenc command line:

Limited:
```
avifenc -r l -d  8 -y 420 -s 0 red.png red-limited-range-420-8bpc.avif
avifenc -r l -d 10 -y 420 -s 0 red.png red-limited-range-420-10bpc.avif
avifenc -r l -d 12 -y 420 -s 0 red.png red-limited-range-420-12bpc.avif
avifenc -r l -d  8 -y 422 -s 0 red.png red-limited-range-422-8bpc.avif
avifenc -r l -d 10 -y 422 -s 0 red.png red-limited-range-422-10bpc.avif
avifenc -r l -d 12 -y 422 -s 0 red.png red-limited-range-422-12bpc.avif
avifenc -r l -d  8 -y 444 -s 0 red.png red-limited-range-444-8bpc.avif
avifenc -r l -d 10 -y 444 -s 0 red.png red-limited-range-444-10bpc.avif
avifenc -r l -d 12 -y 444 -s 0 red.png red-limited-range-444-12bpc.avif
```

Full:
```
avifenc -r f -d  8 -y 420 -s 0 red.png red-full-range-420-8bpc.avif
avifenc -r f -d 10 -y 420 -s 0 red.png red-full-range-420-10bpc.avif
avifenc -r f -d 12 -y 420 -s 0 red.png red-full-range-420-12bpc.avif
avifenc -r f -d  8 -y 422 -s 0 red.png red-full-range-422-8bpc.avif
avifenc -r f -d 10 -y 422 -s 0 red.png red-full-range-422-10bpc.avif
avifenc -r f -d 12 -y 422 -s 0 red.png red-full-range-422-12bpc.avif
avifenc -r f -d  8 -y 444 -s 0 red.png red-full-range-444-8bpc.avif
avifenc -r f -d 10 -y 444 -s 0 red.png red-full-range-444-10bpc.avif
avifenc -r f -d 12 -y 444 -s 0 red.png red-full-range-444-12bpc.avif
```

### red-full-range-(bt709|bt2020)-(pq|hlg)-444-(10|12)bpc.avif
These are all generated from red.png with the appropriate avifenc command line:

BT.709:
```
avifenc -r f -d  8 -y 444 -s 0 --nclx 1/1/1 red.png red-full-range-bt709-444-8bpc.avif
```

PQ:
```
avifenc -r f -d 10 -y 444 -s 0 --nclx 9/16/9 red.png red-full-range-bt2020-pq-444-10bpc.avif
avifenc -r f -d 10 -y 444 -s 0 --nclx 9/16/9 red.png red-full-range-bt2020-pq-444-12bpc.avif
```

HLG:
```
avifenc -r f -d 10 -y 444 -s 0 --nclx 9/18/9 red.png red-full-range-bt2020-hlg-444-10bpc.avif
avifenc -r f -d 10 -y 444 -s 0 --nclx 9/18/9 red.png red-full-range-bt2020-hlg-444-12bpc.avif
```

Unspecified color primaries, transfer characteristics, and matrix coefficients:
```
avifenc -r f -d  8 -y 420 -s 0 --nclx 2/2/2 red.png red-full-range-unspecified-420-8bpc.avif
```

### silver-full-range-srgb-420-8bpc.avif
This is generated from silver.png (3x3 rgb(192, 192, 192)) with the appropriate
avifenc command line:

```
avifenc -r f -d  8 -y 420 -s 0 --nclx 1/13/1 silver.png silver-full-range-srgb-420-8bpc.avif
```

### red-full-range-angle-(0|1|2|3)-mode-(0|1)-420-8bpc.avif
These are all generated from red.png with the appropriate avifenc command line:

```
avifenc -r f -d  8 -y 420 -s 0 --irot 1 red.png red-full-range-angle-1-420-8bpc.avif
avifenc -r f -d  8 -y 420 -s 0 --imir 0 red.png red-full-range-mode-0-420-8bpc.avif
avifenc -r f -d  8 -y 420 -s 0 --imir 1 red.png red-full-range-mode-1-420-8bpc.avif
avifenc -r f -d  8 -y 420 -s 0 --irot 2 --imir 0 red.png red-full-range-angle-2-mode-0-420-8bpc.avif
avifenc -r f -d  8 -y 420 -s 0 --irot 3 --imir 1 red.png red-full-range-angle-3-mode-1-420-8bpc.avif
```

### TODO(crbug.com/960620): Figure out how the rest of files were generated.
