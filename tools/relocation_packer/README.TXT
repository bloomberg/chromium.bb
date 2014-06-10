Introduction:
-------------

R_ARM_RELATIVE relocations are the bulk of dynamic relocations (the .rel.dyn
section) in libchrome.<version>.so.  The ELF standard representation of them
is wasteful.

Packing uses run length encoding to store them more efficiently.  Packed
relocations are placed in a new .android.rel.dyn section.  Packing reduces
the footprint of libchrome.<version>.so in the filesystem, in APK downloads,
and in memory when loaded on the device.

A packed libchrome.<version>.so is designed so that it can be loaded directly
on Android, but requires the explicit support of a crazy linker that has been
extended to understand packed relocations.  Packed relocations are currently
only supported on ARM.

A packed libchrome.<version>.so cannot currently be used with the standard
Android runtime linker.

See src/*.h for design and implementation notes.


Notes:
------

Packing does not adjust debug data.  An unstripped libchrome.<version>.so
can be packed and will run, but may no longer be useful for debugging.

Unpacking on the device requires the explicit support of an extended crazy
linker.  Adds the following new .dynamic tags, used by the crazy linker to
find the packed .android.rel.dyn section data:

  DT_ANDROID_ARM_REL_OFFSET = DT_LOPROC    (Processor specific: 0x70000000)
    - The offset of .android.rel.dyn data in libchrome.<version>.so
  DT_ANDROID_ARM_REL_SIZE = DT_LOPROC + 1  (Processor Specific: 0x70000001)
    - The size of .android.rel.dyn data in bytes

The format of .android.rel.dyn data is:

  "APR1" identifier
  N: the number of count-delta pairs in the encoding
  A: the initial offset
  N * C,D: N count-delta pairs

All numbers in the encoding stream are stored as LEB128 values.  For details
see http://en.wikipedia.org/wiki/LEB128.

The streaming unpacking algorithm is:

  skip over "APR1"
  pairs, addr = next leb128 value, next leb128 value
  emit R_ARM_RELATIVE relocation with r_offset = addr
  while pairs:
    count, delta = next leb128 value, next leb128 value
    while count:
      addr += delta
      emit R_ARM_RELATIVE relocation with r_offset = addr
      count--
    pairs--;


Usage instructions:
-------------------

To pack relocations, add an empty .android.rel.dyn and then run the tool:

    echo -n 'NULL' >/tmp/small
    arm-linux-gnueabi-objcopy \
        --add-section .android.rel.dyn=/tmp/small \
        libchrome.<version>.so libchrome.<version>.so.packed
    rm /tmp/small
    relocation_packer libchrome.<version>.so.packed

To unpack and restore the shared library to its original state:

    cp libchrome.<version>.so.packed unpackable
    relocation_packer -u unpackable
    arm-linux-gnueabi-objcopy \
        --remove-section=.android.rel.dyn unpackable libchrome.<version>.so
    rm unpackable


Bugs & TODOs:
-------------

Currently only supports arm32.  Support for arm64 requires some extension
and modification.

Requires two free slots in the .dynamic section.  Uses these to add data that
tells the crazy linker where to find the packed .android.rel.dyn data.  Fails
if insufficient free slots exist (use gold --spare-dynamic-slots to increase
the allocation).

Requires libelf 0.158 or later.  Earlier libelf releases may be buggy in
ways that prevent the packer from working correctly.


Testing:
--------

Unittests run under gtest, on the host system.
