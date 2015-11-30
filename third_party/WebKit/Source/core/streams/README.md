This directory contains files from several different implementations and
implementation strategies:

## Traditional ReadableStream Implementation

- ReadableByteStream.{cpp,h,idl}
- ReadableByteStreamReader.{h,idl}
- ReadableStream.{cpp,h,idl}
- ReadableStreamImpl.h
- ReadableStreamReader.{cpp,h,idl}
- ReadableStreamReaderTest.cpp
- ReadableStreamTest.cpp
- UnderlyingSource.h

These files implement the current streams spec, plus the more speculative
ReadableByteStream, to the extent necessary to support Fetch response bodies.
They do not support author-constructed readable streams. They use the normal
approach for implementing web-exposed classes, i.e. IDL bindings with C++
implementation. They are currently shipping in Chrome.

## V8 Extras ReadableStream Implementation

- ByteLengthQueuingStrategy.js
- CountQueuingStrategy.js
- ReadableStream.js
- ReadableStreamTempStub.js

These files are an in-progress implementation of the current streams spec,
using [V8 extras][1]. They allow author construction. We hope eventually to
have these supercede the traditional ReadableStream implementation, but the
work to build Fetch response body streams on top of this is ongoing.

[1]: https://docs.google.com/document/d/1AT5-T0aHGp7Lt29vPWFr2-qG8r3l9CByyvKwEuA8Ec0

## Old streams

- Stream.{cpp,h,idl}

These files support an old streams spec. They should eventually be removed, but
right now XMLHttpRequest and Media Streams Extension code in Blink still
depends on them.
