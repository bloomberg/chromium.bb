# core/streams/

This directory contains the Blink implementation of [the WHATWG Streams
standard][1].

We use [V8 extras][2] to implement it.

There is also a new implementation that is written in C++ rather than
JavaScript. It is currently off by default, behind the Blink "StreamsNative"
feature. The following files are part of the new implementation:

    readable_stream_default_controller.idl
    readable_stream_default_controller_interface.cc
    readable_stream_default_controller_interface.h
    readable_stream_default_reader.h
    readable_stream_default_reader.idl
    readable_stream_native.cc
    readable_stream_native.h
    readable_stream_reader.cc
    readable_stream_reader.h
    writable_stream_default_controller.cc
    writable_stream_default_controller.h
    writable_stream_default_controller.idl
    writable_stream_default_writer.cc
    writable_stream_default_writer.h
    writable_stream_default_writer.idl
    writable_stream_native.cc
    writable_stream_native.h
    transferable_streams.cc
    transferable_streams.h
    transform_stream_default_controller.cc
    transform_stream_default_controller.h
    transform_stream_native.cc
    transform_stream_native.h

See also [Streams C++ port design doc][3].

[1]: https://streams.spec.whatwg.org/
[2]: https://docs.google.com/document/d/1AT5-T0aHGp7Lt29vPWFr2-qG8r3l9CByyvKwEuA8Ec0
[3]: https://docs.google.com/document/d/1n0IIRmJb0R-DFc2IhhJfS2-LUwl6iKSBNaR0klr3o40/edit
