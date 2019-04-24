# Signing Input Instructions

*** note
The files in [chromite/signing/signer_instructions/] are currently only for
testing.
The real files used by releases live in [crostools/signer_instructions/].
The program managers would prefer to keep them internal for now.
***

[chromite/signing/signer_instructions/]: https://chromium.googlesource.com/chromiumos/chromite/+/master/signing/signer_instructions/
[crostools/signer_instructions/]: https://chrome-internal.googlesource.com/chromeos/crostools/+/master/signer_instructions/

## Overview

This directory holds instruction files that are used when uploading artifacts
for signing with official keys.
[pushimage] will process them to create output instruction files which are then
posted to a Google Storage bucket that the signing processes watch.
The input files tell [pushimage] how to operate, and output files tell the
signer how to operate.

This file covers things that [pushimage] itself cares about.
It does not get into the fields that the signer utilizes.
See [References] below for that.

## Files

*   `DEFAULT.instructions`:
    Default values for all boards/artifacts; loaded first.
*   `DEFAULT.$TYPE.instructions`:
    Default values for all boards for a specific artifact type.
*   `$BOARD.instructions`:
    Default values for all artifacts for $BOARD, and used for recovery images.
*   `$BOARD.$TYPE.instructions`:
    Values specific to a board and artifact type; see [pushimage]'s
    `--sign-types` for more info.

## Input Instruction Format

There are a few main sections that [pushimage] cares about:
* `[insns]`
* `[insns.XXX]` (Where XXX can be anything)
* `[general]`

Other sections are passed through to the signer untouched, and many fields in
the above sections are also unmodified.

The keys that [pushimage] looks at are:
```ini
[insns]
channels = comma/space delimited list of the channels to flag for signing
keysets = comma/space delimited list of the keysets to use when signing
```

A bunch of fields will also be clobbered in the `[general]` section as
[pushimage] writes out metadata based on the command line flags/artifacts.

## Multiple Channel/Keyset Support

When you want to sign a single board/artifact type for multiple channels or
keysets, simply list them in the `insns.channels` and `insn.keysets` fields.
[pushimage] will take care of posting to the right subdirs and creating unique
filenames based on those.

## Multiple Inputs

When you want to sign multiple artifacts for a single board (and all the same
artifact type), you need to use the multiple input form instead.
When you create multiple sections that start with `insns.`, [pushimage] will
overlay that on top of the `insns` section, and then produce multiple ouput
requests.

So if you wrote a file like:
```ini
[insns]
channel = dev
[insns.one]
keyset = Zinger
input_files = zinger/ec.bin
[insns.two]
keyset = Hoho
input_files = hoho/ec.bin
```

[pushimage] will produce two requests for the signer:
```ini
[insns]
channel = dev
keyset = Zinger
input_files = zinger/ec.bin
```
And:
```ini
[insns]
channel = dev
keyset = Hoho
input_files = hoho/ec.bin
```

## References

For details on the fields that the signer uses:
https://sites.google.com/a/google.com/chromeos/resources/engineering/releng/signer-documentation

[pushimage]: /scripts/pushimage.py
