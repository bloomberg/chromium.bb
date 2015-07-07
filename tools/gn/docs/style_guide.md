# GN Style Guide 

[TOC]
## Naming and ordering within the file

### Location of build files

It usually makes sense to have more build files closer to the code than
fewer ones at the toplevel (this is in contrast with what we did with
GYP). This makes things easier to find and owners reviews easier since
changes are more focused.

### Targets

  * Most BUILD files should have a target with the same name of the
    directory. This target should be the first target.
  * Other targets should be in "some order that makes sense." Usually
    more important targets will be first, and unit tests will follow the
    corresponding target. If there's no clear ordering, consider
    alphabetical order.
  * Test support libraries should be source sets named "test\_support".
    So "//ui/compositor:test\_support". Test support libraries should
    include as public deps the non-test-support version of the library
    so tests need only depend on the test\_support target (rather than
    both).

Naming advice

  * Targets and configs should be named using lowercase with underscores
    separating words, unless there is a strong reason to do otherwise.
  * Source sets, groups, and static libraries do not need globally unique names.
    Prefer to give such targets short, non-redundant names without worrying
    about global uniqueness. For example, it looks much better to write a
    dependency as `"//mojo/public/bindings"` rather than
    `"//mojo/public/bindings:mojo_bindings"
  * Shared libraries (and by extension, components) must have globally unique
    output names. Give such targets short non-unique names above, and then
    provide a globally unique `output_name` for that target.
  * Executables and tests should be given a globally unique name. Technically
    only the output names must be unique, but since only the output names
    appear in the shell and on bots, it's much less confusing if the name
    matches the other places the executable appears.

### Configs

  * A config associated with a single target should be named the same as
    the target with `_config` following it.
  * A config should appear immediately before the corresponding target
    that uses it.

### Example

Example for the `src/foo/BUILD.gn` file:

```
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Config for foo is named foo_config and immediately precedes it in the file.
config("foo_config") {
}

# Target matching path name is the first target.
executable("foo") {
}

# Test for foo follows it.
test("foo_unittests") {
}

config("bar_config") {
}

source_set("bar") {
}
```

## Ordering within a target

  1. `output_name` / `visibility` / `testonly`
  2. `sources`
  3. `cflags`, `include_dirs`, `defines`, `configs` etc. in whatever
     order makes sense to you.
  4. `public_deps`
  5. `deps`

### Conditions

Simple conditions affecting just one variable (e.g. adding a single
source or adding a flag for one particular OS) can go beneath the
variable they affect. More complicated conditions affecting more than
one thing should go at the bottom.

Conditions should be written to minimize the number of conditional blocks.

## Formatting and indenting

  * Indents are two spaces, both for indented blocks and wrapped lines.
  * Variables are `lower_case_with_underscores`
  * Complicated conditions and if statements should generally follow the
    Google C++ style guide for formatting.
  * Comments should be complete sentences with periods at the end.
  * End-of-line-comments should have two spaces separating them and the
    code.
  * Compiler flags and such should always be commented with what they do
    and why the flag is needed.
  * Try to keep lines under 80 columns. If a file name or similar string
    puts you beyond 80 with reasonable indenting, it's OK, but most
    things can be wrapped nicely under that for the code review tool.

### List formatting

```
  deps = [
    "afile.cc",
    "bar.cc",  # Note trailing comma on last element.
  ]
```

Alphabetize the list elements unless there is a more obvious ordering.
In some cases, it makes more sense to put more than one list member on a
line if they clearly go together (for example, two short compiler flags
that must go next to each other).

Prefer use the multi-line style for lists of more than one elements.
Lists with single-elements can be written on one line if desired:

```
  all_dependent_configs = [ ":foo_config" ]  # No trailing comma.
```

### Sources

  * Sources should always be alphabetized within a given list.
  * List sources only once. It is OK to conditionally include sources
    rather than listing them all at the top and then conditionally
    excluding them when they don't apply. Conditional inclusion is often
    clearer since a file is only listed once and it's easier to reason
    about when reading.

```
  sources = [
    "main.cc",
  ]
  if (use_aura) {
    sources += [ "thing_aura.cc" ]
  }
  if (use_gtk) {
    sources += [ "thing_gtk.cc" ]
  }
```

### Deps

  * Deps should be in alphabetical order.
  * Deps within the current file should be written first and not
    qualified with the file name (just `:foo`).
  * Other deps should always use fully-qualified path names unless
    relative ones are required for some reason.

```
  deps = [
    ":a_thing",
    ":mystatic",
    "//foo/bar:other_thing",
    "//foo/baz:that_thing",
  ]
```

### Import

Use fully-qualified paths for imports:

```
import("//foo/bar/baz.gni")  # Even if this file is in the foo/bar directory
```

## Usage

Use `source_set` rather than `static_library` unless you have a reason
to do otherwise. A static library is a standalone library which can be
slow to generate. A source set just links all the object files from that
target into the targets depending on it, which saves the "lib" step.
