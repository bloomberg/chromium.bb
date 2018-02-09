# Blink 'common' public directory

This directory contains public headers for the Web Platform stuff that can
be referenced both from renderer-side and browser-side code, also from
outside the WebKit directory (e.g. from `//content` and `//chrome`).

Anything in this directory should **NOT** depend on other WebKit headers.

See `DEPS` and `WebKit/common/README.md` for more details.
