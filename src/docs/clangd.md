# Clangd

## Introduction

[clangd](https://clangd.llvm.org/) is a clang-based [language server](https://langserver.org/).
It brings IDE features (e.g. diagnostics, code completion, code navigations) to
your editor.

## Quick Start

* **Googlers**: clangd is available by default on glinux (`/usr/bin/clangd`)
* Make sure generated ninja files are up-to-date
* Optional: build chrome normally to get generated headers
* Generate compilation database (note: it's not regenerated automatically):
```
tools/clang/scripts/generate_compdb.py -p out/<build> > compile_commands.json
```
* Indexing is enabled by default (since clangd 9), note that this might consume
  lots of CPU and RAM. There's also a
  [remote-index service](https://github.com/clangd/chrome-remote-index/blob/main/docs/index.md)
  to have an instant project-wide index without consuming local resources
* Use clangd in your favourite editor

## Getting clangd

See [instructions](https://clangd.llvm.org/installation.html#installing-clangd).

**Googlers:** clangd has been installed on your glinux by default, just use
`/usr/bin/clangd`.

Alternative: download clangd from the official [Releases](https://github.com/clangd/clangd/releases)
page.

Note: clangd 10.0.0 does not work with Chromium; use one of the more recent
pre-release versions of 11 or later on the Releases page.

If you prefer to build clangd locally, use the following command to build from
LLVM source, and you will get the binary at
`out/Default/tools/clang/third_party/llvm/build/bin/clangd`.

```
tools/clang/scripts/build_clang_tools_extra.py --fetch out/Default clangd
```

## Setting Up

1. Make sure generated ninja files are up-to-date.

```
gn gen out/Default
```

2. Generate the compilation database, clangd needs it to know how to build a
source file.

```
tools/clang/scripts/generate_compdb.py -p out/Default > compile_commands.json
```

Note: the compilation database is not regenerated automatically. You need to
regenerate it manually whenever build rules change, e.g., when you have new files
checked in or when you sync to head.

If using Windows PowerShell, use the following command instead to set the
output's encoding to UTF-8 (otherwise Clangd will hit "YAML:1:4: error: Got
empty plain scalar" while parsing it).

```
tools/clang/scripts/generate_compdb.py -p out/Default | out-file -encoding utf8 compile_commands.json
```

3. Optional: build chrome normally. This ensures generated headers exist and are
up-to-date. clangd will still work without this step, but it may give errors or
inaccurate results for files which depend on generated headers.

```
ninja -C out/Default chrome
```

4. Optional: configure clangd to use remote-index service for an instant
   project-wide index and reduced local CPU and RAM usage. See
   [instructions](https://github.com/clangd/chrome-remote-index/blob/main/docs/index.md).

5. Use clangd in your favourite editor, see detailed [instructions](
https://clangd.llvm.org/installation.html#editor-plugins).

## Background Indexing

clangd builds an incremental index of your project (all files listed in the
compilation database). The index improves code navigation features
(go-to-definition, find-references) and code completion.

* clangd only uses idle cores to build the index, you can limit the total amount
  of cores by passing the *-j=\<number\>* flag;
* the index is saved to the `.clangd/index` in the project root; index shards
  for common headers e.g. STL will be stored in *$HOME/.clangd/index*;
* background indexing can be disabled by the `--background-index=false` flag;
  Note that, disabling background-index will limit clangd’s knowledge about your
  codebase to files you are currently editing.

Note: the first index time may take hours (for reference, it took 2~3 hours on
a 48-core, 64GB machine). A full index of Chromium (including v8, blink) takes
~550 MB disk space and ~2.7 GB memory in clangd.

Note: [Remote-index service](https://github.com/clangd/chrome-remote-index/blob/main/docs/index.md)
replaces background-index with some downsides like being ~a day old (Clangd will
still know about your changes in the current editing session) and not covering
all configurations (not available for mac&windows specific code or non-main
branches).

## Questions

If you have any questions, reach out to
[clangd/clangd](https://github.com/clangd/clangd) or clangd-dev@lists.llvm.org.
