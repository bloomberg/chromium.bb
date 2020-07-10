# Blink 'common' public directory

This directory contains public headers for the Web Platform stuff that can
be referenced both from renderer-side and browser-side code, also from
outside the Blink directory (e.g. from `//content` and `//chrome`).

Anything in this directory should **NOT** depend on other Blink headers.

Corresponding .cc code normally lives in `blink/common`, and public
`.mojom` files live in `blink/public/mojom`.

See `DEPS` and `blink/common/README.md` for more details.
