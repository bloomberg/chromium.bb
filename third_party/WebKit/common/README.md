# Blink 'common' directory

This directory contains the common Web Platform stuff that needs to be shared by renderer-side and browser-side code.

Things that live in third_party/WebKit/Source and browser-side code (e.g. content/browser for the time being) can both depend on this directory, while anything in this directory should NOT depend on them.

Unlike other directories in WebKit, code in this directory cannot use Blink specific types like WTF, but should use common chromium types.
