# About //ash/components

This directory contains components that are used by //ash system UI and window
manager code. It sits "below" //ash in the dependency graph. For C++ code,
think of //ash/components like top-level //components, but for code that is
only used on Chrome OS, and only for system UI / window manager support.

For example, //ash/components/account_manager manages the user's GAIA accounts,
but only on behalf of Chrome OS code. //components/account_manager_core contains
cross-platform support for accounts.

Some subdirectories contain low-level utility code. For example,
//ash/components/disks has utilities for mounting and unmounting disk volumes.

Much of this code used to live in //chromeos/components. The
[Lacros project](/docs/lacros.md) is extracting browser functionality into a
separate binary. As part of this migration, code used only by the ash-chrome
system UI binary moved into "ash" directories. See the
[Chrome OS source directory migration](https://docs.google.com/document/d/1g-98HpzA8XcoGBWUv1gQNr4rbnD5yfvbtYZyPDDbkaE/edit)
design doc for details.
