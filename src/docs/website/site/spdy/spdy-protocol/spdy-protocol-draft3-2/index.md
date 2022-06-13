---
breadcrumbs:
- - /spdy
  - SPDY
- - /spdy/spdy-protocol
  - SPDY Protocol
page_name: spdy-protocol-draft3-2
title: SPDY Protocol - Draft 3.2
---

[TOC]

# 1. Overview

One of the bottlenecks of HTTP implementations is that HTTP relies on multiple
connections for concurrency. This causes several problems, including additional
round trips for connection setup, slow-start delays, and connection rationing by
the client, where it tries to avoid opening too many connections to any single
server. HTTP pipelining helps some, but only achieves partial multiplexing. In
addition, pipelining has proven non-deployable in existing browsers due to
intermediary interference.

SPDY adds a framing layer for multiplexing multiple, concurrent streams across a
single TCP connection (or any reliable transport stream). The framing layer is
optimized for HTTP-like request-response streams, such that applications which
run over HTTP today can work over SPDY with little or no change on behalf of the
web application writer.

The SPDY session offers four improvements over HTTP:

*   Multiplexed requests: There is no limit to the number of requests
            that can be issued concurrently over a single SPDY connection.
*   Prioritized requests: Clients can request certain resources to be
            delivered first. This avoids the problem of congesting the network
            channel with non-critical resources when a high-priority request is
            pending.
*   Compressed headers: Clients today send a significant amount of
            redundant data in the form of HTTP headers. Because a single web
            page may require 50 or 100 subrequests, this data is significant.
*   Server pushed streams: Server Push enables content to be pushed from
            servers to clients without a request.

SPDY attempts to preserve the existing semantics of HTTP. All features such as
cookies, ETags, Vary headers, Content-Encoding negotiations, etc work as they do
with HTTP; SPDY only replaces the way the data is written to the network.

## 1.1 Document Organization

The SPDY Specification is split into two parts: a framing layer (Section 2),
which multiplexes a TCP connection into independent, length-prefixed frames, and
an HTTP layer (Section 3), which specifies the mechanism for overlaying HTTP
request/response pairs on top of the framing layer. While some of the framing
layer concepts are isolated from the HTTP layer, building a generic framing
layer has not been a goal. The framing layer is tailored to the needs of the
HTTP protocol and server push.

## 1.2 Definitions

*   client: The endpoint initiating the SPDY session.
*   connection: A transport-level connection between two endpoints.
*   endpoint: Either the client or server of a connection.
*   frame: A header-prefixed sequence of bytes sent over a SPDY session.
*   server: The endpoint which did not initiate the SPDY session.
*   session: A synonym for a connection.
*   session error: An error on the SPDY session.
*   stream: A bi-directional flow of bytes across a virtual channel
            within a SPDY session.
*   stream error: An error on an individual SPDY stream.

---

# 2. SPDY Framing Layer

## 2.1 Session (Connections)

The SPDY framing layer (or "session") runs atop a reliable transport layer such
as TCP. The client is the TCP connection initiator. SPDY connections are
persistent connections.

For best performance, it is expected that clients will not close open
connections until the user navigates away from all web pages referencing a
connection, or until the server closes the connection. Servers are encouraged to
leave connections open for as long as possible, but can terminate idle
connections if necessary. When either endpoint closes the transport-level
connection, it MUST first send a GOAWAY (Section 2.6.6) frame so that the
endpoints can reliably determine if requests finished before the close.

## 2.2 Framing

Once the connection is established, clients and servers exchange framed
messages. There are two types of frames: control frames (Section 2.2.1) and data
frames (Section 2.2.2). Frames always have a common header which is 8 bytes in
length.

The first bit is a control bit indicating whether a frame is a control frame or
data frame. Control frames carry a version number, a frame type, flags, and a
length. Data frames contain the stream ID, flags, and the length for the payload
carried after the common header. The simple header is designed to make reading
and writing of frames easy.

All integer values, including length, version, and type, are in network byte
order. SPDY does not enforce alignment of types in dynamically sized frames.

### 2.2.1 Control frames

+----------------------------------+ |C| Version(15bits) | Type(16bits) |
+----------------------------------+ | Flags (8) | Length (24 bits) |
+----------------------------------+ | Data |
+----------------------------------+

Control bit: The 'C' bit is a single bit indicating if this is a control
message. For control frames this value is always 1.

Version: The version number of the SPDY protocol. This document describes SPDY
version 3.1, which is represented as version 3.

Type: The type of control frame. See Control Frames (Section 2.6) for the
complete list of control frames.

Flags: Flags related to this frame. Flags for control frames and data frames are
different.

Length: An unsigned 24-bit value representing the number of bytes after the
length field.

Data: data associated with this control frame. The format and length of this
data is controlled by the control frame type.

Control frame processing requirements:

*   Note that full length control frames (16MB) can be large for
            implementations running on resource-limited hardware. In such cases,
            implementations MAY limit the maximum length frame supported.
            However, all implementations MUST be able to receive control frames
            of at least 8192 octets in length.

### 2.2.2 Data frames

+-------------------------------------+ |C| Stream-ID (31bits) |
+-------------------------------------+ | Flags (8) | Length (24 bits) |
+-------------------------------------+ |\[Pad High (8)\]|\[Pad Low (8)\]|Data
(\*)| +-------------------------------------+ | Data (\*) |
+-------------------------------------+ | Padding (\*) |
+-------------------------------------+

Control bit: For data frames this value is always 0.

Stream-ID: A 31-bit value identifying the stream.

Flags: Flags related to this frame. Valid flags are:

*   0x01 = FLAG_FIN - signifies that this frame represents the last
            frame to be transmitted on this stream. See Stream Close (Section
            2.3.7) below.
*   0x04 = PAD_LOW - bit 3 being set indicates that the Pad Low field is
            present.
*   0x08 = PAD_HIGH - bit 4 being set indicates that the Pad High field
            is present. This bit MUST NOT be set unless the PAD_LOW flag is also
            set.

Length: An unsigned 24-bit value representing the number of bytes after the
length field. The total size of a data frame is 8 bytes + length. It is valid to
have a zero-length data frame.

Pad High: An 8-bit field containing an amount of padding in units of 256 octets.
This field is optional and is only present if the PAD_HIGH flag is set. This
field, in combination with Pad Low, determines how much padding there is on a
frame.

Pad Low: An 8-bit field containing an amount of padding in units of single
octets. This field is optional and is only present if the PAD_LOW flag is set.
This field, in combination with Pad High, determines how much padding there is
on a frame.

Data: The variable-length data payload; the length was defined in the length
field.

Padding: Padding octets that contain no application semantic value. Padding
octets MUST be set to zero when sending and ignored when receiving.

Data frame processing requirements:

*   If an endpoint receives a data frame for a stream-id which is not
            open and the endpoint has not sent a GOAWAY (Section 2.6.6) frame,
            it MUST issue a stream error (Section 2.4.2) with the error code
            INVALID_STREAM for the stream-id.
*   If the endpoint which created the stream receives a data frame
            before receiving a SYN_REPLY on that stream, it is a protocol error,
            and the recipient MUST issue a stream error (Section 2.4.2) with the
            status code PROTOCOL_ERROR for the stream-id.
*   Implementors note: If an endpoint receives multiple data frames for
            invalid stream-ids, it MAY close the session.
*   Padding is not excluded from flow control.
*   The total number of padding octets is determined by multiplying the
            value of the Pad High field by 256 and adding the value of the Pad
            Low field. Both Pad High and Pad Low fields assume a value of zero
            if absent. If the length of the padding is greater than the length
            of the remainder of the frame payload, the recipient MUST treat this
            as a session error of type PROTOCOL_ERROR.
*   Note: A frame can be increased in size by one octet by including a
            Pad Low field with a value of zero.
*   Use of padding is a security feature; as such, its use demands some
            care, see Section 5.5.

## 2.3 Streams

Streams are independent sequences of bi-directional data divided into frames
with several properties:

*   Streams may be created by either the client or server.
*   Streams optionally carry a set of name/value header pairs.
*   Streams can concurrently send data interleaved with other streams.
*   Streams may be cancelled.

### 2.3.1 Stream frames

SPDY defines 3 control frames to manage the lifecycle of a stream:

*   SYN_STREAM - Open a new stream
*   SYN_REPLY - Remote acknowledgement of a new, open stream
*   RST_STREAM - Close a stream

### 2.3.2 Stream creation

A stream is created by sending a control frame with the type set to SYN_STREAM
(Section 2.6.1). If the server is initiating the stream, the Stream-ID must be
even. If the client is initiating the stream, the Stream-ID must be odd. 0 is
not a valid Stream-ID. Stream-IDs from each side of the connection must increase
monotonically as new streams are created. E.g. Stream 2 may be created after
stream 3, but stream 7 must not be created after stream 9. Stream IDs do not
wrap: when a client or server cannot create a new stream id without exceeding a
31 bit value, it MUST NOT create a new stream.

The stream-id MUST increase with each new stream. If an endpoint receives a
SYN_STREAM with a stream id which is less than any previously received
SYN_STREAM, it MUST issue a session error (Section 2.4.1) with the status
PROTOCOL_ERROR.

It is a protocol error to send two SYN_STREAMs with the same stream-id. If a
recipient receives a second SYN_STREAM for the same stream, it MUST issue a
stream error Section (2.4.2) with the status code PROTOCOL_ERROR.

Upon receipt of a SYN_STREAM, the recipient can reject the stream by sending a
stream error (Section 2.4.2) with the error code REFUSED_STREAM. Note, however,
that the creating endpoint may have already sent additional frames for that
stream which cannot be immediately stopped.

Once the stream is created, the creator may immediately send HEADERS or DATA
frames for that stream, without needing to wait for the recipient to
acknowledge.

#### 2.3.2.1 Unidirectional streams

When an endpoint creates a stream with the FLAG_UNIDIRECTIONAL flag set, it
creates a unidirectional stream which the creating endpoint can use to send
frames, but the receiving endpoint cannot. The receiving endpoint is implicitly
already in the half-closed (Section 2.3.6) state.

#### 2.3.2.2 Bidirectional streams

SYN_STREAM frames which do not use the FLAG_UNIDIRECTIONAL flag are
bidirectional streams. Both endpoints can send data on a bi-directional stream.

### 2.3.3 Stream priority

The creator of a stream assigns a priority for that stream. Priority is
represented as an integer from 0 to 7. 0 represents the highest priority and 7
represents the lowest priority.

The sender and recipient SHOULD use best-effort to process streams in the order
of highest priority to lowest priority.

### 2.3.4 Stream headers

Streams carry optional sets of name/value pair headers which carry metadata
about the stream. After the stream has been created, and as long as the sender
is not closed (Section 2.3.7) or half-closed (Section 2.3.6), each side may send
HEADERS frame(s) containing the header data. Header data can be sent in multiple
HEADERS frames, and HEADERS frames may be interleaved with data frames.

### 2.3.5 Stream data exchange

Once a stream is created, it can be used to send arbitrary amounts of data.
Generally this means that a series of data frames will be sent on the stream
until a frame containing the FLAG_FIN flag is set. The FLAG_FIN can be set on a
SYN_STREAM (Section 2.6.1), SYN_REPLY (Section 2.6.2), HEADERS (Section 2.6.7)
or a DATA (Section 2.2.2) frame. Once the FLAG_FIN has been sent, the stream is
considered to be half-closed.

### 2.3.6 Stream half-close

When one side of the stream sends a frame with the FLAG_FIN flag set, the stream
is half-closed from that endpoint. The sender of the FLAG_FIN MUST NOT send
further frames on that stream. When both sides have half-closed, the stream is
closed. Note that WINDOW_UPDATE is metadata about a stream, and is not part of
any stream, and as such is still allowed when a stream is half-closed.

If an endpoint receives a data frame after the stream is half-closed from the
sender (e.g. the endpoint has already received a prior frame for the stream with
the FIN flag set), it MUST send a RST_STREAM to the sender with the status
STREAM_ALREADY_CLOSED.

### 2.3.7 Stream close

There are 3 ways that streams can be terminated:

*   Normal termination: Normal stream termination occurs when both
            sender and recipient have half-closed the stream by sending a
            FLAG_FIN.
*   Abrupt termination: Either the client or server can send a
            RST_STREAM control frame at any time. A RST_STREAM contains an error
            code to indicate the reason for failure. When a RST_STREAM is sent
            from the stream originator, it indicates a failure to complete the
            stream and that no further data will be sent on the stream. When a
            RST_STREAM is sent from the stream recipient, the sender, upon
            receipt, should stop sending any data on the stream. The stream
            recipient should be aware that there is a race between data already
            in transit from the sender and the time the RST_STREAM is received.
            See Stream Error Handling (Section 2.4.2)
*   TCP connection teardown: If the TCP connection is torn down while
            un-closed streams exist, then the endpoint must assume that the
            stream was abnormally interrupted and may be incomplete.

If an endpoint receives a data frame after the stream is closed, it must send a
RST_STREAM to the sender with the status PROTOCOL_ERROR.

## 2.4 Error Handling

The SPDY framing layer has only two types of errors, and they are always handled
consistently. Any reference in this specification to "issue a session error"
refers to Section 2.4.1. Any reference to "issue a stream error" refers to
Section 2.4.2.

### 2.4.1 Session Error Handling

A session error is any error which prevents further processing of the framing
layer or which corrupts the session compression state. When a session error
occurs, the endpoint encountering the error MUST first send a GOAWAY (Section
2.6.6) frame with the stream id of most recently received stream from the remote
endpoint, and the error code for why the session is terminating. After sending
the GOAWAY frame, the endpoint MUST close the TCP connection.

Note that the session compression state is dependent upon both endpoints always
processing all compressed data. If an endpoint partially processes a frame
containing compressed data without updating compression state properly, future
control frames which use compression will be always be errored. Implementations
SHOULD always try to process compressed data so that errors which could be
handled as stream errors do not become session errors.

Note that because this GOAWAY is sent during a session error case, it is
possible that the GOAWAY will not be reliably received by the receiving
endpoint. It is a best-effort attempt to communicate with the remote about why
the session is going down.

### 2.4.2 Stream Error Handling

A stream error is an error related to a specific stream-id which does not affect
processing of other streams at the framing layer. Upon a stream error, the
endpoint MUST send a RST_STREAM (Section 2.6.3) frame which contains the stream
id of the stream where the error occurred and the error status which caused the
error. After sending the RST_STREAM, the stream is closed to the sending
endpoint. After sending the RST_STREAM, if the sender receives any frames other
than a RST_STREAM for that stream id, it will result in sending additional
RST_STREAM frames. An endpoint MUST NOT send a RST_STREAM in response to an
RST_STREAM, as doing so would lead to RST_STREAM loops. Sending a RST_STREAM
does not cause the SPDY session to be closed.

If an endpoint has multiple RST_STREAM frames to send in succession for the same
stream-id and the same error code, it MAY coalesce them into a single RST_STREAM
frame. (This can happen if a stream is closed, but the remote sends multiple
data frames. There is no reason to send a RST_STREAM for each frame in
succession).

## 2.5 Data flow

Because TCP provides a single stream of data on which SPDY multiplexes multiple
logical streams, clients and servers must intelligently interleave data messages
for concurrent sessions.

## 2.6 Control frame types

### 2.6.1 SYN_STREAM

The SYN_STREAM control frame allows the sender to asynchronously create a stream
between the endpoints. See Stream Creation (section 2.3.2)

+------------------------------------+ |1| version | 1 |
+------------------------------------+ | Flags (8) | Length (24 bits) |
+------------------------------------+ |X| Stream-ID (31bits) |
+------------------------------------+ |X| Associated-To-Stream-ID (31bits) |
+------------------------------------+ | Pri|Unused | Slot | |
+-------------------+ | | Number of Name/Value pairs (int32) | &lt;+
+------------------------------------+ | | Length of name (int32) | | This
section is the "Name/Value +------------------------------------+ | Header
Block", and is compressed. | Name (string) | |
+------------------------------------+ | | Length of value (int32) | |
+------------------------------------+ | | Value (string) | |
+------------------------------------+ | | (repeats) | &lt;+

Flags: Flags related to this frame. Valid flags are:

*   0x01 = FLAG_FIN - marks this frame as the last frame to be
            transmitted on this stream and puts the sender in the half-closed
            (Section 2.3.6) state.
*   0x02 = FLAG_UNIDIRECTIONAL - a stream created with this flag puts
            the recipient in the half-closed (Section 2.3.6) state.

Length: The length is the number of bytes which follow the length field in the
frame. For SYN_STREAM frames, this is 10 bytes plus the length of the compressed
Name/Value block.

Stream-ID: The 31-bit identifier for this stream. This stream-id will be used in
frames which are part of this stream.

Associated-To-Stream-ID: The 31-bit identifier for a stream which this stream is
associated to. If this stream is independent of all other streams, it should be
0.

Priority: A 3-bit priority (Section 2.3.3) field.

Unused: 5 bits of unused space, reserved for future use.

Slot: 8 bits of unused space, reserved for future use. Name/Value Header Block:
A set of name/value pairs carried as part of the SYN_STREAM. see Name/Value
Header Block (Section 2.6.10).

If an endpoint receives a SYN_STREAM which is larger than the implementation
supports, it MAY send a RST_STREAM with error code FRAME_TOO_LARGE. All
implementations MUST support the minimum size limits defined in the Control
Frames section
([S](/spdy/spdy-protocol/spdy-protocol-draft3#TOC-2.2.1-Control-frames)ection
2.2.1).

### 2.6.2 SYN_REPLY

SYN_REPLY indicates the acceptance of a stream creation by the recipient of a
SYN_STREAM frame.

+------------------------------------+ |1| version | 2 |
+------------------------------------+ | Flags (8) | Length (24 bits) |
+------------------------------------+ |X| Stream-ID (31bits) |
+------------------------------------+ | Number of Name/Value pairs (int32) |
&lt;+ +------------------------------------+ | | Length of name (int32) | | This
section is the "Name/Value +------------------------------------+ | Header
Block", and is compressed. | Name (string) | |
+------------------------------------+ | | Length of value (int32) | |
+------------------------------------+ | | Value (string) | |
+------------------------------------+ | | (repeats) | &lt;+

Flags: Flags related to this frame. Valid flags are:

*   0x01 = FLAG_FIN - marks this frame as the last frame to be
            transmitted on this stream and puts the sender in the half-closed
            (Section 2.3.6) state.

Length: The length is the number of bytes which follow the length field in the
frame. For SYN_REPLY frames, this is 4 bytes plus the length of the compressed
Name/Value block.

Stream-ID: The 31-bit identifier for this stream.

If an endpoint receives multiple SYN_REPLY frames for the same active stream ID,
it MUST issue a stream error (Section 2.4.2) with the error code STREAM_IN_USE.

Name/Value Header Block: A set of name/value pairs carried as part of the
SYN_STREAM. see Name/Value Header Block (Section 2.6.10).

If an endpoint receives a SYN_REPLY which is larger than the implementation
supports, it MAY send a RST_STREAM with error code FRAME_TOO_LARGE. All
implementations MUST support the minimum size limits defined in the Control
Frames section
([S](/spdy/spdy-protocol/spdy-protocol-draft3#TOC-2.2.1-Control-frames)ection
2.2.1).

### 2.6.3 RST_STREAM

The RST_STREAM frame allows for abnormal termination of a stream. When sent by
the creator of a stream, it indicates the creator wishes to cancel the stream.
When sent by the recipient of a stream, it indicates an error or that the
recipient did not want to accept the stream, so the stream should be closed.

+----------------------------------+ |1| version | 3 |
+----------------------------------+ | Flags (8) | 8 |
+----------------------------------+ |X| Stream-ID (31bits) |
+----------------------------------+ | Status code |
+----------------------------------+

Flags: Flags related to this frame. RST_STREAM does not define any flags. This
value must be 0.

Length: An unsigned 24-bit value representing the number of bytes after the
length field. For RST_STREAM control frames, this value is always 8.

Stream-ID: The 31-bit identifier for this stream.

Status code: (32 bits) An indicator for why the stream is being terminated.The
following status codes are defined:

*   1 - PROTOCOL_ERROR. This is a generic error, and should only be used
            if a more specific error is not available.
*   2 - INVALID_STREAM. This is returned when a frame is received for a
            stream which is not active.
*   3 - REFUSED_STREAM. Indicates that the stream was refused before any
            processing has been done on the stream.
*   4 - UNSUPPORTED_VERSION. Indicates that the recipient of a stream
            does not support the SPDY version requested.
*   5 - CANCEL. Used by the creator of a stream to indicate that the
            stream is no longer needed.
*   6 - INTERNAL_ERROR. This is a generic error which can be used when
            the implementation has internally failed, not due to anything in the
            protocol.
*   7 - FLOW_CONTROL_ERROR. The endpoint detected that its peer violated
            the flow control protocol.
*   8 - STREAM_IN_USE. The endpoint received a SYN_REPLY for a stream
            already open.
*   9 - STREAM_ALREADY_CLOSED. The endpoint received a data or SYN_REPLY
            frame for a stream which is half closed.
*   10 - deprecated/unused.
*   11 - FRAME_TOO_LARGE. The endpoint received a frame which this
            implementation could not support. If FRAME_TOO_LARGE is sent for a
            SYN_STREAM, HEADERS, or SYN_REPLY frame without fully processing the
            compressed portion of those frames, then the compression state will
            be out-of-sync with the other endpoint. In this case, senders of
            FRAME_TOO_LARGE MUST close the session.
*   Note: 0 is not a valid status code for a RST_STREAM.

After receiving a RST_STREAM on a stream, the recipient must not send additional
frames for that stream, and the stream moves into the closed state.

### 2.6.4 SETTINGS

A SETTINGS frame contains a set of id/value pairs for communicating
configuration data about how the two endpoints may communicate. SETTINGS frames
can be sent at any time by either endpoint, are optionally sent, and are fully
asynchronous. When the server is the sender, the sender can request that
configuration data be persisted by the client across SPDY sessions and returned
to the server in future communications.

Persistence of SETTINGS ID/Value pairs is done on a per origin/IP pair (the
"origin" is the set of scheme, host, and port from the URI. See RFC6454). That
is, when a client connects to a server, and the server persists settings within
the client, the client SHOULD return the persisted settings on future
connections to the same origin AND IP address and TCP port. Clients MUST NOT
request servers to use the persistence features of the SETTINGS frames, and
servers MUST ignore persistence related flags sent by a client.

+----------------------------------+ |1| version | 4 |
+----------------------------------+ | Flags (8) | Length (24 bits) |
+----------------------------------+ | Number of entries |
+----------------------------------+ | ID/Value Pairs | | ... |

Control bit: The control bit is always 1 for this message.

Version: The SPDY version number.

Type: The message type for a SETTINGS message is 4.

Flags: FLAG_SETTINGS_CLEAR_SETTINGS (0x1): When set, the client should clear any
previously persisted SETTINGS ID/Value pairs. If this frame contains ID/Value
pairs with the FLAG_SETTINGS_PERSIST_VALUE set, then the client will first clear
its existing, persisted settings, and then persist the values with the flag set
which are contained within this frame. Because persistence is only implemented
on the client, this flag can only be used when the sender is the server.

Length: An unsigned 24-bit value representing the number of bytes after the
length field. The total size of a SETTINGS frame is 8 bytes + length.

Number of entries: A 32-bit value representing the number of ID/value pairs in
this message.

Each ID/value pair is as follows:

+----------------------------------+ | Flags(8) | ID (24 bits) |
+----------------------------------+ | Value (32 bits) |
+----------------------------------+

Flags: An 8 bit value. Defined Flags:

*   FLAG_SETTINGS_PERSIST_VALUE (0x1): When set, the sender of this
            SETTINGS frame is requesting that the recipient persist the ID/Value
            and return it in future SETTINGS frames sent from the sender to this
            recipient. Because persistence is only implemented on the client,
            this flag is only sent by the server.
*   FLAG_SETTINGS_PERSISTED (0x2): When set, the sender is notifying the
            recipient that this ID/Value pair was previously sent to the sender
            by the recipient with the FLAG_SETTINGS_PERSIST_VALUE, and the
            sender is returning it. Because persistence is only implemented on
            the client, this flag is only sent by the client.

ID: 24-bits in network byte order. Defined IDs:

*   1 - SETTINGS_UPLOAD_BANDWIDTH allows the sender to send its expected
            upload bandwidth on this channel. This number is an estimate. The
            value should be the integral number of kilobytes per second that the
            sender predicts as an expected maximum upload channel capacity.
*   2 - SETTINGS_DOWNLOAD_BANDWIDTH allows the sender to send its
            expected download bandwidth on this channel. This number is an
            estimate. The value should be the integral number of kilobytes per
            second that the sender predicts as an expected maximum download
            channel capacity.
*   3 - SETTINGS_ROUND_TRIP_TIME allows the sender to send its expected
            round-trip-time on this channel. The round trip time is defined as
            the minimum amount of time to send a control frame from this client
            to the remote and receive a response. The value is represented in
            milliseconds.
*   4 - SETTINGS_MAX_CONCURRENT_STREAMS allows the sender to inform the
            remote endpoint the maximum number of remote-initiated concurrent
            streams which it will allow. By default there is no limit. For
            implementors it is recommended that this value be no smaller than
            100.
*   5 - SETTINGS_CURRENT_CWND allows the sender to inform the remote
            endpoint of the current TCP CWND value.
*   6 - SETTINGS_DOWNLOAD_RETRANS_RATE allows the sender to inform the
            remote endpoint the retransmission rate (bytes retransmitted / total
            bytes transmitted).
*   7 - SETTINGS_INITIAL_WINDOW_SIZE allows the sender to inform the
            remote endpoint the initial window size (in bytes) for new streams.
*   8 - SETTINGS_CLIENT_CERTIFICATE_VECTOR_SIZE allows the server to
            inform the client if the new size of the client certificate vector.

Value: A 32-bit value.

The message is intentionally extensible for future information which may improve
client-server communications. The sender does not need to send every type of
ID/value. It must only send those for which it has accurate values to convey.
When multiple ID/value pairs are sent, they should be sent in order of lowest id
to highest id. A single SETTINGS frame MUST not contain multiple values for the
same ID. If the recipient of a SETTINGS frame discovers multiple values for the
same ID, it MUST ignore all values except the first one.

A server may send multiple SETTINGS frames containing different ID/Value pairs.
When the same ID/Value is sent twice, the most recent value overrides any
previously sent values. If the server sends IDs 1, 2, and 3 with the
FLAG_SETTINGS_PERSIST_VALUE in a first SETTINGS frame, and then sends IDs 4 and
5 with the FLAG_SETTINGS_PERSIST_VALUE, when the client returns the persisted
state on its next SETTINGS frame, it SHOULD send all 5 settings (1, 2, 3, 4, and
5 in this example) to the server.

### 2.6.5 PING

The PING control frame is a mechanism for measuring a minimal round-trip time
from the sender. It can be sent from the client or the server. Recipients of a
PING frame should send an identical frame to the sender as soon as possible (if
there is other pending data waiting to be sent, PING should take highest
priority). Each ping sent by a sender should use a unique ID.

+----------------------------------+ |1| version | 6 |
+----------------------------------+ | 0 (flags) | 4 (length) |
+----------------------------------| | 32-bit ID |
+----------------------------------+

Control bit: The control bit is always 1 for this message.

Version: The SPDY version number.

Type: The message type for a PING message is 6.

Length: This frame is always 4 bytes long.

ID: A unique ID for this ping, represented as an unsigned 32 bit value. When the
client initiates a ping, it must use an odd numbered ID. When the server
initiates a ping, it must use an even numbered ping. Use of odd/even IDs is
required in order to avoid accidental looping on PINGs (where each side
initiates an identical PING at the same time).

Note: If a sender uses all possible PING ids (e.g. has sent all 2^31 possible
IDs), it can wrap and start re-using IDs.

If a server receives an even numbered PING which it did not initiate, it must
ignore the PING. If a client receives an odd numbered PING which it did not
initiate, it must ignore the PING.

### 2.6.6 GOAWAY

The GOAWAY control frame is a mechanism to tell the remote side of the
connection to stop creating streams on this session. It can be sent from the
client or the server. Once sent, the sender will not respond to any new
SYN_STREAMs on this session. Recipients of a GOAWAY frame must not send
additional streams on this session, although a new session can be established
for new streams. The purpose of this message is to allow an endpoint to
gracefully stop accepting new streams (perhaps for a reboot or maintenance),
while still finishing processing of previously established streams.

There is an inherent race condition between an endpoint sending SYN_STREAMs and
the remote sending a GOAWAY message. To deal with this case, the GOAWAY contains
a last-stream-id indicating the stream-id of the last stream which was created
on the sending endpoint in this session. If the receiver of the GOAWAY sent new
SYN_STREAMs for sessions after this last-stream-id, they were not processed by
the server and the receiver may treat the stream as though it had never been
created at all (hence the receiver may want to re-create the stream later on a
new session).

Endpoints should always send a GOAWAY message before closing a connection so
that the remote can know whether a stream has been partially processed or not.
(For example, if an HTTP client sends a POST at the same time that a server
closes a connection, the client cannot know if the server started to process
that POST request if the server does not send a GOAWAY frame to indicate where
it stopped working).

After sending a GOAWAY message, the sender must ignore all SYN_STREAM frames for
new streams, and MUST NOT create any new streams.

+----------------------------------+ |1| version | 7 |
+----------------------------------+ | 0 (flags) | 8 (length) |
+----------------------------------| |X| Last-good-stream-ID (31 bits) |
+----------------------------------+ | Status code |
+----------------------------------+

Control bit: The control bit is always 1 for this message.

Version: The SPDY version number.

Type: The message type for a GOAWAY message is 7.

Length: This frame is always 8 bytes long.

Last-good-stream-Id: The last stream id which was accepted by the sender of the
GOAWAY message. If no streams were replied to, this value MUST be 0.

Status: The reason for closing the session.

*   0 - OK. This is a normal session teardown.
*   1 - PROTOCOL_ERROR. This is a generic error, and should only be used
            if a more specific error is not available.
*   2 - INTERNAL_ERROR. This is a generic error which can be used when
            the implementation has internally failed, not due to anything in the
            protocol.

### 2.6.7 HEADERS

The HEADERS frame augments a stream with additional headers. It may be
optionally sent on an existing stream at any time. Specific application of the
headers in this frame is application-dependent. The name/value header block
within this frame is compressed.

+------------------------------------+ |1| version | 8 |
+------------------------------------+ | Flags (8) | Length (24 bits) |
+------------------------------------+ |X| Stream-ID (31bits) |
+------------------------------------+ | Number of Name/Value pairs (int32) |
&lt;+ +------------------------------------+ | | Length of name (int32) | | This
section is the "Name/Value +------------------------------------+ | Header
Block", and is compressed. | Name (string) | |
+------------------------------------+ | | Length of value (int32) | |
+------------------------------------+ | | Value (string) | |
+------------------------------------+ | | (repeats) | &lt;+

Flags: Flags related to this frame. Valid flags are:

*   0x01 = FLAG_FIN - marks this frame as the last frame to be
            transmitted on this stream and puts the sender in the half-closed
            (Section 2.3.6) state.

Length: An unsigned 24 bit value representing the number of bytes after the
length field. The minimum length of the length field is 4 (when the number of
name value pairs is 0).

Stream-ID: The stream this HEADERS block is associated with.

Name/Value Header Block: A set of name/value pairs carried as part of the
SYN_STREAM. see Name/Value Header Block (Section 2.6.10).

### 2.6.8 WINDOW_UPDATE

The WINDOW_UPDATE frame is used to implement flow control.

Flow control operates at two levels: on each individual stream and on the entire
connection.

Both types of flow control are hop by hop; that is, only between the two
endpoints. Intermediaries do not forward WINDOW_UPDATE frames between dependent
connections. However, throttling of data transfer by any receiver can indirectly
cause the propagation of flow control information toward the original sender.

Flow control only applies to frames that are identified as being subject to flow
control. Of the frame types defined in this document, this includes only DATA
frame. Frames that are exempt from flow control MUST be accepted and processed,
unless the receiver is unable to assign resources to handling the frame. A
receiver MAY respond with a stream error (Section 2.4.2) or session error
(Section 2.4.1) of type FLOW_CONTROL_ERROR if it is unable accept a frame.

+----------------------------------+ |1| version | 9 |
+----------------------------------+ | 0 (flags) | 8 (length) |
+----------------------------------+ |X| Stream-ID (31-bits) |
+----------------------------------+ |X| Delta-Window-Size (31-bits) |
+----------------------------------+

Control bit: The control bit is always 1 for this message.

Version: The SPDY version number.

Flags: The flags field is always zero.

Type: The message type for a WINDOW_UPDATE message is 9.

Length: The length field is always 8 for this frame (there are always 8 bytes
after the length field).

Stream-ID: The stream ID for which this WINDOW_UPDATE control frame applies to,
or 0 if applied to connection-level flow control.

Delta-Window-Size: The additional number of bytes that the sender can transmit
in addition to existing remaining window size. The legal range for this field is
1 to 2^31 - 1 (0x7fffffff) bytes.

A receiver that receives a flow controlled frame MUST always account for its
contribution against the connection flow control window, unless the receiver
treats this as a session error (Section 2.4.1). This is necessary even if the
frame is in error. Since the sender counts the frame toward the flow control
window, if the receiver does not, the flow control window at sender and receiver
can become different.

#### The Flow Control Window

Flow control in SPDY 3.1 is implemented using a window kept by each sender on
every stream. The flow control window is a simple integer value that indicates
how many bytes of data the sender is permitted to transmit; as such, its size is
a measure of the buffering capability of the receiver.

Two flow control windows are applicable: the stream flow control window and the
connection flow control window. The sender MUST NOT send a flow controlled frame
with a length that exceeds the space available in either of the flow control
windows advertised by the receiver. Frames with zero length with the END_STREAM
flag set (for example, an empty data frame) MAY be sent if there is no available
space in either flow control window.

For flow control calculations, the 8 byte frame header is not counted.

After sending a flow controlled frame, the sender reduces the space available in
both windows by the length of the transmitted frame.

The receiver of a frame sends a WINDOW_UPDATE frame as it consumes data and
frees up space in flow control windows. Separate WINDOW_UPDATE frames are sent
for the stream and connection level flow control windows.

A sender that receives a WINDOW_UPDATE frame updates the corresponding window by
the amount specified in the frame.

A sender MUST NOT allow a flow control window to exceed 2^31 - 1 bytes. If a
sender receives a WINDOW_UPDATE that causes a flow control window to exceed this
maximum it MUST terminate either the stream or the connection, as appropriate.
For streams, the sender sends a RST_STREAM with the error code of
FLOW_CONTROL_ERROR code; for the connection, a GOAWAY frame with a
FLOW_CONTROL_ERROR code.

Flow controlled frames from the sender and WINDOW_UPDATE frames from the
receiver are completely asynchronous with respect to each other. This property
allows a receiver to aggressively update the window size kept by the sender to
prevent streams from stalling.

#### Initial Flow Control Window Size

When a SPDY/3.1 connection is first established, new streams are created with an
initial flow control window size of 64KB. The connection flow control window is
64KB bytes. Both endpoints can adjust the initial window size for new streams by
including a value for SETTINGS_INITIAL_WINDOW_SIZE in the SETTINGS frame that
forms part of the connection header.

Prior to receiving a SETTINGS frame that sets a value for
SETTINGS_INITIAL_WINDOW_SIZE, an endpoint can only use the default initial
window size when sending flow controlled frames. Similarly, the connection flow
control window is set to the default initial window size until a WINDOW_UPDATE
frame is received.

A SETTINGS frame can alter the initial flow control window size for all current
streams. When the value of SETTINGS_INITIAL_WINDOW_SIZE changes, a receiver MUST
adjust the size of all stream flow control windows that it maintains by the
difference between the new value and the old value. A SETTINGS frame cannot
alter the connection flow control window.

A change to SETTINGS_INITIAL_WINDOW_SIZE could cause the available space in a
flow control window to become negative. A sender MUST track the negative flow
control window, and MUST NOT send new flow controlled frames until it receives
WINDOW_UPDATE frames that cause the flow control window to become positive.

For example, if the client sends 60KB immediately on connection establishment,
and the server sets the initial window size to be 16KB, the client will
recalculate the available flow control window to be -44KB on receipt of the
SETTINGS frame. The client retains a negative flow control window until
WINDOW_UPDATE frames restore the window to being positive, after which the
client can resume sending.

#### Reducing the Stream Window Size

A receiver that wishes to use a smaller flow control window than the current
size can send a new SETTINGS frame. However, the receiver MUST be prepared to
receive data that exceeds this window size, since the sender might send data
that exceeds the lower limit prior to processing the SETTINGS frame.

A receiver has two options for handling streams that exceed flow control limits:

1.  The receiver can immediately send RST_STREAM with FLOW_CONTROL_ERROR
            error code for the affected streams.
2.  The receiver can accept the streams and tolerate the resulting head
            of line blocking, sending WINDOW_UPDATE frames as it consumes data.

If a receiver decides to accept streams, both sides MUST recompute the available
flow control window based on the initial window size sent in the SETTINGS.

### 2.6.9 CREDENTIAL

DEPRECATED.

### 2.6.10 Name/Value Header Block

The Name/Value Header Block is found in the SYN_STREAM, SYN_REPLY and HEADERS
control frames, and shares a common format:

+------------------------------------+ | Number of Name/Value pairs (int32) |
+------------------------------------+ | Length of name (int32) |
+------------------------------------+ | Name (string) |
+------------------------------------+ | Length of value (int32) |
+------------------------------------+ | Value (string) |
+------------------------------------+ | (repeats) |

Number of Name/Value pairs: The number of repeating name/value pairs following
this field.

List of Name/Value pairs:

*   Length of Name: a 32-bit value containing the number of octets in
            the name field. Note that in practice, this length must not exceed
            2^24, as that is the maximum size of a SPDY frame.
*   Name: 0 or more octets, 8-bit sequences of data, excluding 0.
*   Length of Value: a 32-bit value containing the number of octets in
            the value field. Note that in practice, this length must not exceed
            2^24, as that is the maximum size of a SPDY frame.
*   Value: 0 or more octets, 8-bit sequences of data, excluding 0.

Each header name must have at least one value. Header names are encoded using
the [US-ASCII character
set](http://mbelshe.github.com/SPDY-Specification/draft-mbelshe-spdy-00.xml#ASCII)
\[ASCII\] and must be all lower case. The length of each name must be greater
than zero. A recipient of a zero-length name MUST issue a stream error (Section
2.4.2) with the status code PROTOCOL_ERROR for the stream-id.

Duplicate header names are not allowed. To send two identically named headers,
send a header with two values, where the values are separated by a single NUL
(0) byte. A header value can either be empty (e.g. the length is zero) or it can
contain multiple, NUL-separated values, each with length greater than zero. The
value never starts nor ends with a NUL character. Recipients of illegal value
fields MUST issue a stream error (Section 2.4.2) with the status code
PROTOCOL_ERROR for the stream-id.

#### 2.6.10.1 Compression

The Name/Value Header Block is a section of the SYN_STREAM, SYN_REPLY, and
HEADERS frames used to carry header meta-data. This block is always compressed
using zlib compression. Within this specification, any reference to 'zlib' is
referring to the [ZLIB Compressed Data Format Specification Version 3.3 as part
of
RFC1950.](http://mbelshe.github.com/SPDY-Specification/draft-mbelshe-spdy-00.xml#RFC1950)

For each HEADERS compression instance, the initial state is initialized using
the following
[dictionary](http://mbelshe.github.com/SPDY-Specification/draft-mbelshe-spdy-00.xml#UDELCOMPRESSION)
\[UDELCOMPRESSION\]:

const unsigned char SPDY_dictionary_txt\[\] = {         0x00, 0x00, 0x00, 0x07,
0x6f, 0x70, 0x74, 0x69, \\\\ - - - - o p t i    0x6f, 0x6e, 0x73, 0x00, 0x00,
0x00, 0x04, 0x68, \\\\ o n s - - - - h    0x65, 0x61, 0x64, 0x00, 0x00, 0x00,
0x04, 0x70, \\\\ e a d - - - - p    0x6f, 0x73, 0x74, 0x00, 0x00, 0x00, 0x03,
0x70, \\\\ o s t - - - - p    0x75, 0x74, 0x00, 0x00, 0x00, 0x06, 0x64, 0x65,
\\\\ u t - - - - d e    0x6c, 0x65, 0x74, 0x65, 0x00, 0x00, 0x00, 0x05, \\\\ l e
t e - - - -    0x74, 0x72, 0x61, 0x63, 0x65, 0x00, 0x00, 0x00, \\\\ t r a c e -
- -    0x06, 0x61, 0x63, 0x63, 0x65, 0x70, 0x74, 0x00, \\\\ - a c c e p t -
0x00, 0x00, 0x0e, 0x61, 0x63, 0x63, 0x65, 0x70, \\\\ - - - a c c e p    0x74,
0x2d, 0x63, 0x68, 0x61, 0x72, 0x73, 0x65, \\\\ t - c h a r s e    0x74, 0x00,
0x00, 0x00, 0x0f, 0x61, 0x63, 0x63, \\\\ t - - - - a c c    0x65, 0x70, 0x74,
0x2d, 0x65, 0x6e, 0x63, 0x6f, \\\\ e p t - e n c o    0x64, 0x69, 0x6e, 0x67,
0x00, 0x00, 0x00, 0x0f, \\\\ d i n g - - - -    0x61, 0x63, 0x63, 0x65, 0x70,
0x74, 0x2d, 0x6c, \\\\ a c c e p t - l    0x61, 0x6e, 0x67, 0x75, 0x61, 0x67,
0x65, 0x00, \\\\ a n g u a g e -    0x00, 0x00, 0x0d, 0x61, 0x63, 0x63, 0x65,
0x70, \\\\ - - - a c c e p    0x74, 0x2d, 0x72, 0x61, 0x6e, 0x67, 0x65, 0x73,
\\\\ t - r a n g e s    0x00, 0x00, 0x00, 0x03, 0x61, 0x67, 0x65, 0x00, \\\\ - -
- - a g e -    0x00, 0x00, 0x05, 0x61, 0x6c, 0x6c, 0x6f, 0x77, \\\\ - - - a l l
o w    0x00, 0x00, 0x00, 0x0d, 0x61, 0x75, 0x74, 0x68, \\\\ - - - - a u t h
0x6f, 0x72, 0x69, 0x7a, 0x61, 0x74, 0x69, 0x6f, \\\\ o r i z a t i o    0x6e,
0x00, 0x00, 0x00, 0x0d, 0x63, 0x61, 0x63, \\\\ n - - - - c a c    0x68, 0x65,
0x2d, 0x63, 0x6f, 0x6e, 0x74, 0x72, \\\\ h e - c o n t r    0x6f, 0x6c, 0x00,
0x00, 0x00, 0x0a, 0x63, 0x6f, \\\\ o l - - - - c o    0x6e, 0x6e, 0x65, 0x63,
0x74, 0x69, 0x6f, 0x6e, \\\\ n n e c t i o n    0x00, 0x00, 0x00, 0x0c, 0x63,
0x6f, 0x6e, 0x74, \\\\ - - - - c o n t    0x65, 0x6e, 0x74, 0x2d, 0x62, 0x61,
0x73, 0x65, \\\\ e n t - b a s e    0x00, 0x00, 0x00, 0x10, 0x63, 0x6f, 0x6e,
0x74, \\\\ - - - - c o n t    0x65, 0x6e, 0x74, 0x2d, 0x65, 0x6e, 0x63, 0x6f,
\\\\ e n t - e n c o    0x64, 0x69, 0x6e, 0x67, 0x00, 0x00, 0x00, 0x10, \\\\ d i
n g - - - -    0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, \\\\ c o n t e n
t -    0x6c, 0x61, 0x6e, 0x67, 0x75, 0x61, 0x67, 0x65, \\\\ l a n g u a g e
0x00, 0x00, 0x00, 0x0e, 0x63, 0x6f, 0x6e, 0x74, \\\\ - - - - c o n t    0x65,
0x6e, 0x74, 0x2d, 0x6c, 0x65, 0x6e, 0x67, \\\\ e n t - l e n g    0x74, 0x68,
0x00, 0x00, 0x00, 0x10, 0x63, 0x6f, \\\\ t h - - - - c o    0x6e, 0x74, 0x65,
0x6e, 0x74, 0x2d, 0x6c, 0x6f, \\\\ n t e n t - l o    0x63, 0x61, 0x74, 0x69,
0x6f, 0x6e, 0x00, 0x00, \\\\ c a t i o n - -    0x00, 0x0b, 0x63, 0x6f, 0x6e,
0x74, 0x65, 0x6e, \\\\ - - c o n t e n    0x74, 0x2d, 0x6d, 0x64, 0x35, 0x00,
0x00, 0x00, \\\\ t - m d 5 - - -    0x0d, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e,
0x74, \\\\ - c o n t e n t    0x2d, 0x72, 0x61, 0x6e, 0x67, 0x65, 0x00, 0x00,
\\\\ - r a n g e - -    0x00, 0x0c, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, \\\\ - -
c o n t e n    0x74, 0x2d, 0x74, 0x79, 0x70, 0x65, 0x00, 0x00, \\\\ t - t y p e
- -    0x00, 0x04, 0x64, 0x61, 0x74, 0x65, 0x00, 0x00, \\\\ - - d a t e - -
0x00, 0x04, 0x65, 0x74, 0x61, 0x67, 0x00, 0x00, \\\\ - - e t a g - -    0x00,
0x06, 0x65, 0x78, 0x70, 0x65, 0x63, 0x74, \\\\ - - e x p e c t    0x00, 0x00,
0x00, 0x07, 0x65, 0x78, 0x70, 0x69, \\\\ - - - - e x p i    0x72, 0x65, 0x73,
0x00, 0x00, 0x00, 0x04, 0x66, \\\\ r e s - - - - f    0x72, 0x6f, 0x6d, 0x00,
0x00, 0x00, 0x04, 0x68, \\\\ r o m - - - - h    0x6f, 0x73, 0x74, 0x00, 0x00,
0x00, 0x08, 0x69, \\\\ o s t - - - - i    0x66, 0x2d, 0x6d, 0x61, 0x74, 0x63,
0x68, 0x00, \\\\ f - m a t c h -    0x00, 0x00, 0x11, 0x69, 0x66, 0x2d, 0x6d,
0x6f, \\\\ - - - i f - m o    0x64, 0x69, 0x66, 0x69, 0x65, 0x64, 0x2d, 0x73,
\\\\ d i f i e d - s    0x69, 0x6e, 0x63, 0x65, 0x00, 0x00, 0x00, 0x0d, \\\\ i n
c e - - - -    0x69, 0x66, 0x2d, 0x6e, 0x6f, 0x6e, 0x65, 0x2d, \\\\ i f - n o n
e -    0x6d, 0x61, 0x74, 0x63, 0x68, 0x00, 0x00, 0x00, \\\\ m a t c h - - -
0x08, 0x69, 0x66, 0x2d, 0x72, 0x61, 0x6e, 0x67, \\\\ - i f - r a n g    0x65,
0x00, 0x00, 0x00, 0x13, 0x69, 0x66, 0x2d, \\\\ e - - - - i f -    0x75, 0x6e,
0x6d, 0x6f, 0x64, 0x69, 0x66, 0x69, \\\\ u n m o d i f i    0x65, 0x64, 0x2d,
0x73, 0x69, 0x6e, 0x63, 0x65, \\\\ e d - s i n c e    0x00, 0x00, 0x00, 0x0d,
0x6c, 0x61, 0x73, 0x74, \\\\ - - - - l a s t    0x2d, 0x6d, 0x6f, 0x64, 0x69,
0x66, 0x69, 0x65, \\\\ - m o d i f i e    0x64, 0x00, 0x00, 0x00, 0x08, 0x6c,
0x6f, 0x63, \\\\ d - - - - l o c    0x61, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x00,
0x00, \\\\ a t i o n - - -    0x0c, 0x6d, 0x61, 0x78, 0x2d, 0x66, 0x6f, 0x72,
\\\\ - m a x - f o r    0x77, 0x61, 0x72, 0x64, 0x73, 0x00, 0x00, 0x00, \\\\ w a
r d s - - -    0x06, 0x70, 0x72, 0x61, 0x67, 0x6d, 0x61, 0x00, \\\\ - p r a g m
a -    0x00, 0x00, 0x12, 0x70, 0x72, 0x6f, 0x78, 0x79, \\\\ - - - p r o x y
0x2d, 0x61, 0x75, 0x74, 0x68, 0x65, 0x6e, 0x74, \\\\ - a u t h e n t    0x69,
0x63, 0x61, 0x74, 0x65, 0x00, 0x00, 0x00, \\\\ i c a t e - - -    0x13, 0x70,
0x72, 0x6f, 0x78, 0x79, 0x2d, 0x61, \\\\ - p r o x y - a    0x75, 0x74, 0x68,
0x6f, 0x72, 0x69, 0x7a, 0x61, \\\\ u t h o r i z a    0x74, 0x69, 0x6f, 0x6e,
0x00, 0x00, 0x00, 0x05, \\\\ t i o n - - - -    0x72, 0x61, 0x6e, 0x67, 0x65,
0x00, 0x00, 0x00, \\\\ r a n g e - - -    0x07, 0x72, 0x65, 0x66, 0x65, 0x72,
0x65, 0x72, \\\\ - r e f e r e r    0x00, 0x00, 0x00, 0x0b, 0x72, 0x65, 0x74,
0x72, \\\\ - - - - r e t r    0x79, 0x2d, 0x61, 0x66, 0x74, 0x65, 0x72, 0x00,
\\\\ y - a f t e r -    0x00, 0x00, 0x06, 0x73, 0x65, 0x72, 0x76, 0x65, \\\\ - -
- s e r v e    0x72, 0x00, 0x00, 0x00, 0x02, 0x74, 0x65, 0x00, \\\\ r - - - - t
e -    0x00, 0x00, 0x07, 0x74, 0x72, 0x61, 0x69, 0x6c, \\\\ - - - t r a i l
0x65, 0x72, 0x00, 0x00, 0x00, 0x11, 0x74, 0x72, \\\\ e r - - - - t r    0x61,
0x6e, 0x73, 0x66, 0x65, 0x72, 0x2d, 0x65, \\\\ a n s f e r - e    0x6e, 0x63,
0x6f, 0x64, 0x69, 0x6e, 0x67, 0x00, \\\\ n c o d i n g -    0x00, 0x00, 0x07,
0x75, 0x70, 0x67, 0x72, 0x61, \\\\ - - - u p g r a    0x64, 0x65, 0x00, 0x00,
0x00, 0x0a, 0x75, 0x73, \\\\ d e - - - - u s    0x65, 0x72, 0x2d, 0x61, 0x67,
0x65, 0x6e, 0x74, \\\\ e r - a g e n t    0x00, 0x00, 0x00, 0x04, 0x76, 0x61,
0x72, 0x79, \\\\ - - - - v a r y    0x00, 0x00, 0x00, 0x03, 0x76, 0x69, 0x61,
0x00, \\\\ - - - - v i a -    0x00, 0x00, 0x07, 0x77, 0x61, 0x72, 0x6e, 0x69,
\\\\ - - - w a r n i    0x6e, 0x67, 0x00, 0x00, 0x00, 0x10, 0x77, 0x77, \\\\ n g
- - - - w w    0x77, 0x2d, 0x61, 0x75, 0x74, 0x68, 0x65, 0x6e, \\\\ w - a u t h
e n    0x74, 0x69, 0x63, 0x61, 0x74, 0x65, 0x00, 0x00, \\\\ t i c a t e - -
0x00, 0x06, 0x6d, 0x65, 0x74, 0x68, 0x6f, 0x64, \\\\ - - m e t h o d    0x00,
0x00, 0x00, 0x03, 0x67, 0x65, 0x74, 0x00, \\\\ - - - - g e t -    0x00, 0x00,
0x06, 0x73, 0x74, 0x61, 0x74, 0x75, \\\\ - - - s t a t u    0x73, 0x00, 0x00,
0x00, 0x06, 0x32, 0x30, 0x30, \\\\ s - - - - 2 0 0    0x20, 0x4f, 0x4b, 0x00,
0x00, 0x00, 0x07, 0x76, \\\\ - O K - - - - v    0x65, 0x72, 0x73, 0x69, 0x6f,
0x6e, 0x00, 0x00, \\\\ e r s i o n - -    0x00, 0x08, 0x48, 0x54, 0x54, 0x50,
0x2f, 0x31, \\\\ - - H T T P - 1    0x2e, 0x31, 0x00, 0x00, 0x00, 0x03, 0x75,
0x72, \\\\ - 1 - - - - u r    0x6c, 0x00, 0x00, 0x00, 0x06, 0x70, 0x75, 0x62,
\\\\ l - - - - p u b    0x6c, 0x69, 0x63, 0x00, 0x00, 0x00, 0x0a, 0x73, \\\\ l i
c - - - - s    0x65, 0x74, 0x2d, 0x63, 0x6f, 0x6f, 0x6b, 0x69, \\\\ e t - c o o
k i    0x65, 0x00, 0x00, 0x00, 0x0a, 0x6b, 0x65, 0x65, \\\\ e - - - - k e e
0x70, 0x2d, 0x61, 0x6c, 0x69, 0x76, 0x65, 0x00, \\\\ p - a l i v e -    0x00,
0x00, 0x06, 0x6f, 0x72, 0x69, 0x67, 0x69, \\\\ - - - o r i g i    0x6e, 0x31,
0x30, 0x30, 0x31, 0x30, 0x31, 0x32, \\\\ n 1 0 0 1 0 1 2    0x30, 0x31, 0x32,
0x30, 0x32, 0x32, 0x30, 0x35, \\\\ 0 1 2 0 2 2 0 5    0x32, 0x30, 0x36, 0x33,
0x30, 0x30, 0x33, 0x30, \\\\ 2 0 6 3 0 0 3 0    0x32, 0x33, 0x30, 0x33, 0x33,
0x30, 0x34, 0x33, \\\\ 2 3 0 3 3 0 4 3    0x30, 0x35, 0x33, 0x30, 0x36, 0x33,
0x30, 0x37, \\\\ 0 5 3 0 6 3 0 7    0x34, 0x30, 0x32, 0x34, 0x30, 0x35, 0x34,
0x30, \\\\ 4 0 2 4 0 5 4 0    0x36, 0x34, 0x30, 0x37, 0x34, 0x30, 0x38, 0x34,
\\\\ 6 4 0 7 4 0 8 4    0x30, 0x39, 0x34, 0x31, 0x30, 0x34, 0x31, 0x31, \\\\ 0 9
4 1 0 4 1 1    0x34, 0x31, 0x32, 0x34, 0x31, 0x33, 0x34, 0x31, \\\\ 4 1 2 4 1 3
4 1    0x34, 0x34, 0x31, 0x35, 0x34, 0x31, 0x36, 0x34, \\\\ 4 4 1 5 4 1 6 4
0x31, 0x37, 0x35, 0x30, 0x32, 0x35, 0x30, 0x34, \\\\ 1 7 5 0 2 5 0 4    0x35,
0x30, 0x35, 0x32, 0x30, 0x33, 0x20, 0x4e, \\\\ 5 0 5 2 0 3 - N    0x6f, 0x6e,
0x2d, 0x41, 0x75, 0x74, 0x68, 0x6f, \\\\ o n - A u t h o    0x72, 0x69, 0x74,
0x61, 0x74, 0x69, 0x76, 0x65, \\\\ r i t a t i v e    0x20, 0x49, 0x6e, 0x66,
0x6f, 0x72, 0x6d, 0x61, \\\\ - I n f o r m a    0x74, 0x69, 0x6f, 0x6e, 0x32,
0x30, 0x34, 0x20, \\\\ t i o n 2 0 4 -    0x4e, 0x6f, 0x20, 0x43, 0x6f, 0x6e,
0x74, 0x65, \\\\ N o - C o n t e    0x6e, 0x74, 0x33, 0x30, 0x31, 0x20, 0x4d,
0x6f, \\\\ n t 3 0 1 - M o    0x76, 0x65, 0x64, 0x20, 0x50, 0x65, 0x72, 0x6d,
\\\\ v e d - P e r m    0x61, 0x6e, 0x65, 0x6e, 0x74, 0x6c, 0x79, 0x34, \\\\ a n
e n t l y 4    0x30, 0x30, 0x20, 0x42, 0x61, 0x64, 0x20, 0x52, \\\\ 0 0 - B a d
- R    0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x34, 0x30, \\\\ e q u e s t 4 0
0x31, 0x20, 0x55, 0x6e, 0x61, 0x75, 0x74, 0x68, \\\\ 1 - U n a u t h    0x6f,
0x72, 0x69, 0x7a, 0x65, 0x64, 0x34, 0x30, \\\\ o r i z e d 4 0    0x33, 0x20,
0x46, 0x6f, 0x72, 0x62, 0x69, 0x64, \\\\ 3 - F o r b i d    0x64, 0x65, 0x6e,
0x34, 0x30, 0x34, 0x20, 0x4e, \\\\ d e n 4 0 4 - N    0x6f, 0x74, 0x20, 0x46,
0x6f, 0x75, 0x6e, 0x64, \\\\ o t - F o u n d    0x35, 0x30, 0x30, 0x20, 0x49,
0x6e, 0x74, 0x65, \\\\ 5 0 0 - I n t e    0x72, 0x6e, 0x61, 0x6c, 0x20, 0x53,
0x65, 0x72, \\\\ r n a l - S e r    0x76, 0x65, 0x72, 0x20, 0x45, 0x72, 0x72,
0x6f, \\\\ v e r - E r r o    0x72, 0x35, 0x30, 0x31, 0x20, 0x4e, 0x6f, 0x74,
\\\\ r 5 0 1 - N o t    0x20, 0x49, 0x6d, 0x70, 0x6c, 0x65, 0x6d, 0x65, \\\\ - I
m p l e m e    0x6e, 0x74, 0x65, 0x64, 0x35, 0x30, 0x33, 0x20, \\\\ n t e d 5 0
3 -    0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x20, \\\\ S e r v i c e -
0x55, 0x6e, 0x61, 0x76, 0x61, 0x69, 0x6c, 0x61, \\\\ U n a v a i l a    0x62,
0x6c, 0x65, 0x4a, 0x61, 0x6e, 0x20, 0x46, \\\\ b l e J a n - F    0x65, 0x62,
0x20, 0x4d, 0x61, 0x72, 0x20, 0x41, \\\\ e b - M a r - A    0x70, 0x72, 0x20,
0x4d, 0x61, 0x79, 0x20, 0x4a, \\\\ p r - M a y - J    0x75, 0x6e, 0x20, 0x4a,
0x75, 0x6c, 0x20, 0x41, \\\\ u n - J u l - A    0x75, 0x67, 0x20, 0x53, 0x65,
0x70, 0x74, 0x20, \\\\ u g - S e p t -    0x4f, 0x63, 0x74, 0x20, 0x4e, 0x6f,
0x76, 0x20, \\\\ O c t - N o v -    0x44, 0x65, 0x63, 0x20, 0x30, 0x30, 0x3a,
0x30, \\\\ D e c - 0 0 - 0    0x30, 0x3a, 0x30, 0x30, 0x20, 0x4d, 0x6f, 0x6e,
\\\\ 0 - 0 0 - M o n    0x2c, 0x20, 0x54, 0x75, 0x65, 0x2c, 0x20, 0x57, \\\\ - -
T u e - - W    0x65, 0x64, 0x2c, 0x20, 0x54, 0x68, 0x75, 0x2c, \\\\ e d - - T h
u -    0x20, 0x46, 0x72, 0x69, 0x2c, 0x20, 0x53, 0x61, \\\\ - F r i - - S a
0x74, 0x2c, 0x20, 0x53, 0x75, 0x6e, 0x2c, 0x20, \\\\ t - - S u n - -    0x47,
0x4d, 0x54, 0x63, 0x68, 0x75, 0x6e, 0x6b, \\\\ G M T c h u n k    0x65, 0x64,
0x2c, 0x74, 0x65, 0x78, 0x74, 0x2f, \\\\ e d - t e x t -    0x68, 0x74, 0x6d,
0x6c, 0x2c, 0x69, 0x6d, 0x61, \\\\ h t m l - i m a    0x67, 0x65, 0x2f, 0x70,
0x6e, 0x67, 0x2c, 0x69, \\\\ g e - p n g - i    0x6d, 0x61, 0x67, 0x65, 0x2f,
0x6a, 0x70, 0x67, \\\\ m a g e - j p g    0x2c, 0x69, 0x6d, 0x61, 0x67, 0x65,
0x2f, 0x67, \\\\ - i m a g e - g    0x69, 0x66, 0x2c, 0x61, 0x70, 0x70, 0x6c,
0x69, \\\\ i f - a p p l i    0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x78,
\\\\ c a t i o n - x    0x6d, 0x6c, 0x2c, 0x61, 0x70, 0x70, 0x6c, 0x69, \\\\ m l
- a p p l i    0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x78, \\\\ c a t i o n
- x    0x68, 0x74, 0x6d, 0x6c, 0x2b, 0x78, 0x6d, 0x6c, \\\\ h t m l - x m l
0x2c, 0x74, 0x65, 0x78, 0x74, 0x2f, 0x70, 0x6c, \\\\ - t e x t - p l    0x61,
0x69, 0x6e, 0x2c, 0x74, 0x65, 0x78, 0x74, \\\\ a i n - t e x t    0x2f, 0x6a,
0x61, 0x76, 0x61, 0x73, 0x63, 0x72, \\\\ - j a v a s c r    0x69, 0x70, 0x74,
0x2c, 0x70, 0x75, 0x62, 0x6c, \\\\ i p t - p u b l    0x69, 0x63, 0x70, 0x72,
0x69, 0x76, 0x61, 0x74, \\\\ i c p r i v a t    0x65, 0x6d, 0x61, 0x78, 0x2d,
0x61, 0x67, 0x65, \\\\ e m a x - a g e    0x3d, 0x67, 0x7a, 0x69, 0x70, 0x2c,
0x64, 0x65, \\\\ - g z i p - d e    0x66, 0x6c, 0x61, 0x74, 0x65, 0x2c, 0x73,
0x64, \\\\ f l a t e - s d    0x63, 0x68, 0x63, 0x68, 0x61, 0x72, 0x73, 0x65,
\\\\ c h c h a r s e    0x74, 0x3d, 0x75, 0x74, 0x66, 0x2d, 0x38, 0x63, \\\\ t -
u t f - 8 c    0x68, 0x61, 0x72, 0x73, 0x65, 0x74, 0x3d, 0x69, \\\\ h a r s e t
- i    0x73, 0x6f, 0x2d, 0x38, 0x38, 0x35, 0x39, 0x2d, \\\\ s o - 8 8 5 9 -
0x31, 0x2c, 0x75, 0x74, 0x66, 0x2d, 0x2c, 0x2a, \\\\ 1 - u t f - - -    0x2c,
0x65, 0x6e, 0x71, 0x3d, 0x30, 0x2e \\\\ - e n q - 0 - };

The entire contents of the name/value header block is compressed using zlib.
There is a single zlib stream for all name value pairs in one direction on a
connection. SPDY uses a SYNC_FLUSH between each compressed frame.

Implementation notes: the compression engine can be tuned to favor speed or
size. Optimizing for size increases memory use and CPU consumption. Because
header blocks are generally small, implementors may want to reduce the
window-size of the compression engine from the default 15bits (a 32KB window) to
more like 11bits (a 2KB window). The exact setting is chosen by the compressor,
the decompressor will work with any setting.

---

# 3. HTTP Layering over SPDY

SPDY is intended to be as compatible as possible with current web-based
applications. This means that, from the perspective of the server business logic
or application API, the features of HTTP are unchanged. To achieve this, all of
the application request and response header semantics are preserved, although
the syntax of conveying those semantics has changed. Thus, the rules from the
[HTTP/1.1 specification in
RFC2616](http://mbelshe.github.com/SPDY-Specification/draft-mbelshe-spdy-00.xml#RFC2616)
apply with the changes in the sections below.

## 3.1 Connection Management

Clients SHOULD NOT open more than one SPDY session to a given
[origin](http://mbelshe.github.com/SPDY-Specification/draft-mbelshe-spdy-00.xml#RFC6454)
concurrently.

Note that it is possible for one SPDY session to be finishing (e.g. a GOAWAY
message has been sent, but not all streams have finished), while another SPDY
session is starting.

### 3.1.1 Use of GOAWAY

SPDY provides a GOAWAY message which can be used when closing a connection from
either the client or server. Without a server GOAWAY message, HTTP has a race
condition where the client sends a request (a new SYN_STREAM) just as the server
is closing the connection, and the client cannot know if the server received the
stream or not. By using the last-stream-id in the GOAWAY, servers can indicate
to the client if a request was processed or not.

Note that some servers will choose to send the GOAWAY and immediately terminate
the connection without waiting for active streams to finish. The client will be
able to determine this because SPDY streams are determinstically closed. This
abrupt termination will force the client to heuristically decide whether to
retry the pending requests. Clients always need to be capable of dealing with
this case because they must deal with accidental connection termination cases,
which are the same as the server never having sent a GOAWAY.

More sophisticated servers will use GOAWAY to implement a graceful teardown.
They will send the GOAWAY and provide some time for the active streams to finish
before terminating the connection.

If a SPDY client closes the connection, it should also send a GOAWAY message.
This allows the server to know if any server-push streams were received by the
client.

If the endpoint closing the connection has not received any SYN_STREAMs from the
remote, the GOAWAY will contain a last-stream-id of 0.

## 3.2 HTTP Request/Response

### 3.2.1 Request

The client initiates a request by sending a SYN_STREAM frame. For requests which
do not contain a body, the SYN_STREAM frame MUST set the FLAG_FIN, indicating
that the client intends to send no further data on this stream. For requests
which do contain a body, the SYN_STREAM will not contain the FLAG_FIN, and the
body will follow the SYN_STREAM in a series of DATA frames. The last DATA frame
will set the FLAG_FIN to indicate the end of the body.

The SYN_STREAM Name/Value section will contain all of the HTTP headers which are
associated with an HTTP request. The header block in SPDY is mostly unchanged
from today's HTTP header block, with the following differences:

*   The first line of the request is unfolded into name/value pairs like
            other HTTP headers and MUST be present:
    *   ":method" - the HTTP method for this request (e.g. "GET",
                "POST", "HEAD", etc)
    *   ":path" - the url-path for this url with "/" prefixed. (See
                [RFC3986](http://mbelshe.github.com/SPDY-Specification/draft-mbelshe-spdy-00.xml#RFC3986)).
                For example, for "http://www.google.com/search?q=dogs" the path
                would be "/search?q=dogs".
    *   ":version" - the HTTP version of this request (e.g. "HTTP/1.1")
*   In addition, the following two name/value pairs must also be present
            in every request:
    *   ":host" - the hostport (See
                [RFC1738](http://mbelshe.github.com/SPDY-Specification/draft-mbelshe-spdy-00.xml#RFC1738))
                portion of the URL for this request (e.g.
                "www.google.com:1234"). This header is the same as the HTTP
                'Host' header.
    *   ":scheme" - the scheme portion of the URL for this request (e.g.
                "https"))
*   Header names are all lowercase.
*   The Connection, Host, Keep-Alive, Proxy-Connection, and
            Transfer-Encoding headers are not valid and MUST not be sent.
*   User-agents MUST support gzip compression. Regardless of the
            Accept-Encoding sent by the user-agent, the server may always send
            content encoded with gzip or deflate encoding.
*   If a server receives a request where the sum of the data frame
            payload lengths does not equal the size of the Content-Length
            header, the server MUST return a 400 (Bad Request) error.
*   POST-specific changes:
    *   Although POSTs are inherently chunked, POST requests SHOULD also
                be accompanied by a Content-Length header. There are two reasons
                for this: First, it assists with upload progress meters for an
                improved user experience. But second, we know from early
                versions of SPDY that failure to send a content length header is
                incompatible with many existing HTTP server implementations.
                Existing user-agents do not omit the Content-Length header, and
                server implementations have come to depend upon this.

The user-agent is free to prioritize requests as it sees fit. If the user-agent
cannot make progress without receiving a resource, it should attempt to raise
the priority of that resource. Resources such as images, SHOULD generally use
the lowest priority.

If a client sends a SYN_STREAM without all of the method, host, path, scheme,
and version headers, the server MUST reply with a HTTP 400 Bad Request reply.

### 3.2.2 Response

The server responds to a client request with a SYN_REPLY frame. Symmetric to the
client's upload stream, server will send data after the SYN_REPLY frame via a
series of DATA frames, and the last data frame will contain the FLAG_FIN to
indicate successful end-of-stream. If a response (like a 202 or 204 response)
contains no body, the SYN_REPLY frame may contain the FLAG_FIN flag to indicate
no further data will be sent on the stream.

*   The response status line is unfolded into name/value pairs like
            other HTTP headers and must be present:
    *   ":status" - The HTTP response status code (e.g. "200" or "200
                OK")
    *   ":version" - The HTTP response version (e.g. "HTTP/1.1")
*   All header names must be lowercase.
*   The Connection, Keep-Alive, Proxy-Connection, and Transfer-Encoding
            headers are not valid and MUST not be sent.
*   Responses MAY be accompanied by a Content-Length header for advisory
            purposes. (e.g. for UI progress meters)
*   If a client receives a response where the sum of the data frame
            payload lengths does not equal the size of the Content-Length
            header, the client MUST ignore the content length header.

If a client receives a SYN_REPLY without a status or without a version header,
the client must reply with a RST_STREAM frame indicating a PROTOCOL ERROR.

### 3.2.3 Authentication

When a client sends a request to an origin server that requires authentication,
the server can reply with a "401 Unauthorized" response, and include a
WWW-Authenticate challenge header that defines the authentication scheme to be
used. The client then retries the request with an Authorization header
appropriate to the specified authentication scheme.

There are four options for proxy authentication, Basic, Digest, NTLM and
Negotiate (SPNEGO). The first two options were defined in
[RFC2617](http://mbelshe.github.com/SPDY-Specification/draft-mbelshe-spdy-00.xml#RFC2617),
and are stateless. The second two options were developed by Microsoft and
specified in
[RFC4559](http://mbelshe.github.com/SPDY-Specification/draft-mbelshe-spdy-00.xml#RFC4559),
and are stateful; otherwise known as multi-round authentication, or connection
authentication.

#### 3.2.3.1 Stateless Authentication

Stateless Authentication over SPDY is identical to how it is performed over
HTTP. If multiple SPDY streams are concurrently sent to a single server, each
will authenticate independently, similar to how two HTTP connections would
independently authenticate to a proxy server.

#### 3.2.3.2 Stateful Authentication

Unfortunately, the stateful authentication mechanisms were implemented and
defined in a such a way that directly violates RFC2617 - they do not include a
"realm" as part of the request. This is problematic in SPDY because it makes it
impossible for a client to disambiguate two concurrent server authentication
challenges.

To deal with this case, SPDY servers using Stateful Authentication MUST
implement one of two changes:

*   Servers can add a "realm=&lt;desired realm&gt;" header so that the
            two authentication requests can be disambiguated and run
            concurrently. Unfortunately, given how these mechanisms work, this
            is probably not practical.
*   Upon sending the first stateful challenge response, the server MUST
            buffer and defer all further frames which are not part of completing
            the challenge until the challenge has completed. Completing the
            authentication challenge may take multiple round trips. Once the
            client receives a "401 Authenticate" response for a stateful
            authentication type, it MUST stop sending new requests to the server
            until the authentication has completed by receiving a non-401
            response on at least one stream.

## 3.3 Server Push Transactions

SPDY enables a server to send multiple replies to a client for a single request.
The rationale for this feature is that sometimes a server knows that it will
need to send multiple resources in response to a single request. Without server
push features, the client must first download the primary resource, then
discover the secondary resource(s), and request them. Pushing of resources
avoids the round-trip delay, but also creates a potential race where a server
can be pushing content which a user-agent is in the process of requesting. The
following mechanics attempt to prevent the race condition while enabling the
performance benefit.

Browsers receiving a pushed response MUST validate that the server is authorized
to push the URL using the [browser
same-origin](http://mbelshe.github.com/SPDY-Specification/draft-mbelshe-spdy-00.xml#RFC6454)
policy. For example, a SPDY connection to www.foo.com is generally not permitted
to push a response for www.evil.com.

If the browser accepts a pushed response (e.g. it does not send a RST_STREAM),
the browser MUST attempt to cache the pushed response in same way that it would
cache any other response. This means validating the response headers and
inserting into the cache.

Because pushed responses have no request, they have no request headers
associated with them. At the framing layer, SPDY pushed streams contain an
"associated-stream-id" which indicates the requested stream for which the pushed
stream is related. The pushed stream inherits all of the headers from the
associated-stream-id with the exception of ":host", ":scheme", and ":path",
which are provided as part of the pushed response stream headers. The browser
MUST store these inherited and implied request headers with the cached resource.

Implementation note: With server push, it is theoretically possible for servers
to push unreasonable amounts of content or resources to the user-agent. Browsers
MUST implement throttles to protect against unreasonable push attacks.

### 3.3.1 Server implementation

When the server intends to push a resource to the user-agent, it opens a new
stream by sending a unidirectional SYN_STREAM. The SYN_STREAM MUST include an
Associated-To-Stream-ID, and MUST set the FLAG_UNIDIRECTIONAL flag. The
SYN_STREAM MUST include headers for ":scheme", ":host", ":path", which represent
the URL for the resource being pushed. Subsequent headers may follow in HEADERS
frames. The purpose of the association is so that the user-agent can
differentiate which request induced the pushed stream; without it, if the
user-agent had two tabs open to the same page, each pushing unique content under
a fixed URL, the user-agent would not be able to differentiate the requests.

The Associated-To-Stream-ID must be the ID of an existing, open stream. The
reason for this restriction is to have a clear endpoint for pushed content. If
the user-agent requested a resource on stream 11, the server replies on stream
11. It can push any number of additional streams to the client before sending a
FLAG_FIN on stream 11. However, once the originating stream is closed no further
push streams may be associated with it. The pushed streams do not need to be
closed (FIN set) before the originating stream is closed, they only need to be
created before the originating stream closes.

It is illegal for a server to push a resource with the Associated-To-Stream-ID
of 0.

To minimize race conditions with the client, the SYN_STREAM for the pushed
resources MUST be sent prior to sending any content which could allow the client
to discover the pushed resource and request it.

The server MUST only push resources which would have been returned from a GET
request.

Note: If the server does not have all of the Name/Value Response headers
available at the time it issues the HEADERS frame for the pushed resource, it
may later use an additional HEADERS frame to augment the name/value pairs to be
associated with the pushed stream. The subsequent HEADERS frame(s) must not
contain a header for ':host', ':scheme', or ':path' (e.g. the server can't
change the identity of the resource to be pushed). The HEADERS frame must not
contain duplicate headers with a previously sent HEADERS frame. The server must
send a HEADERS frame including the scheme/host/port headers before sending any
data frames on the stream.

### 3.3.2 Client implementation

When fetching a resource the client has 3 possibilities:

*   the resource is not being pushed
*   the resource is being pushed, but the data has not yet arrived
*   the resource is being pushed, and the data has started to arrive

When a SYN_STREAM and HEADERS frame which contains an Associated-To-Stream-ID is
received, the client must not issue GET requests for the resource in the pushed
stream, and instead wait for the pushed stream to arrive.

If a client receives a server push stream with stream-id 0, it MUST issue a
session error (Section 2.4.2) with the status code PROTOCOL_ERROR.

When a client receives a SYN_STREAM from the server without a the ':host',
':scheme', and ':path' headers in the Name/Value section, it MUST reply with a
RST_STREAM with error code HTTP_PROTOCOL_ERROR.

To cancel individual server push streams, the client can issue a stream error
(Section 2.4.2) with error code CANCEL. Upon receipt, the server MUST stop
sending on this stream immediately (this is an Abrupt termination).

To cancel all server push streams related to a request, the client may issue a
stream error (Section 2.4.2) with error code CANCEL on the associated-stream-id.
By cancelling that stream, the server MUST immediately stop sending frames for
any streams with in-association-to for the original stream.

If the server sends a HEADER frame containing duplicate headers with a previous
HEADERS frame for the same stream, the client must issue a stream error (Section
2.4.2) with error code PROTOCOL ERROR.

If the server sends a HEADERS frame after sending a data frame for the same
stream, the client MAY ignore the HEADERS frame. Ignoring the HEADERS frame
after a data frame prevents handling of HTTP's trailing headers
(http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.40).

---

# 4. Design Rationale and Notes

Authors' notes: The notes in this section have no bearing on the SPDY protocol
as specified within this document, and none of these notes should be considered
authoritative about how the protocol works. However, these notes may prove
useful in future debates about how to resolve protocol ambiguities or how to
evolve the protocol going forward. They may be removed before the final draft.

## 4.1 Separation of Framing Layer and Application Layer

Readers may note that this specification sometimes blends the framing layer
(Section 2) with requirements of a specific application - HTTP (Section 3). This
is reflected in the request/response nature of the streams, the definition of
the HEADERS and compression contexts which are very similar to HTTP, and other
areas as well.

This blending is intentional - the primary goal of this protocol is to create a
low-latency protocol for use with HTTP. Isolating the two layers is convenient
for description of the protocol and how it relates to existing HTTP
implementations. However, the ability to reuse the SPDY framing layer is a non
goal.

## 4.2 Error handling - Framing Layer

Error handling at the SPDY layer splits errors into two groups: Those that
affect an individual SPDY stream, and those that do not.

When an error is confined to a single stream, but general framing is in tact,
SPDY attempts to use the RST_STREAM as a mechanism to invalidate the stream but
move forward without aborting the connection altogether.

For errors occuring outside of a single stream context, SPDY assumes the entire
session is hosed. In this case, the endpoint detecting the error should initiate
a connection close.

## 4.3 One Connection Per Domain

SPDY attempts to use fewer connections than other protocols have traditionally
used. The rationale for this behavior is because it is very difficult to provide
a consistent level of service (e.g. TCP slow-start), prioritization, or optimal
compression when the client is connecting to the server through multiple
channels.

Through lab measurements, we have seen consistent latency benefits by using
fewer connections from the client. The overall number of packets sent by SPDY
can be as much as 40% less than HTTP. Handling large numbers of concurrent
connections on the server also does become a scalability problem, and SPDY
reduces this load.

The use of multiple connections is not without benefit, however. Because SPDY
multiplexes multiple, independent streams onto a single stream, it creates a
potential for head-of-line blocking problems at the transport level. In tests so
far, the negative effects of head-of-line blocking (especially in the presence
of packet loss) is outweighed by the benefits of compression and prioritization.

## 4.4 Fixed vs Variable Length Fields

SPDY favors use of fixed length 32bit fields in cases where smaller, variable
length encodings could have been used. To some, this seems like a tragic waste
of bandwidth. SPDY choses the simple encoding for speed and simplicity.

The goal of SPDY is to reduce latency on the network. The overhead of SPDY
frames is generally quite low. Each data frame is only an 8 byte overhead for a
1452 byte payload (~0.6%). At the time of this writing, bandwidth is already
plentiful, and there is a strong trend indicating that bandwidth will continue
to increase. With an average worldwide bandwidth of 1Mbps, and assuming that a
variable length encoding could reduce the overhead by 50%, the latency saved by
using a variable length encoding would be less than 100 nanoseconds. More
interesting are the effects when the larger encodings force a packet boundary,
in which case a round-trip could be induced. However, by addressing other
aspects of SPDY and TCP interactions, we believe this is completely mitigated.

## 4.5 Compression Context(s)

When isolating the compression contexts used for communicating with multiple
origins, we had a few choices to make. We could have maintained a map (or list)
of compression contexts usable for each origin. The basic case is easy - each
HEADERS frame would need to identify the context to use for that frame. However,
compression contexts are not cheap, so the lifecycle of each context would need
to be bounded. For proxy servers, where we could churn through many contexts,
this would be a concern. We considered using a static set of contexts, say 16 of
them, which would bound the memory use. We also considered dynamic contexts,
which could be created on the fly, and would need to be subsequently destroyed.
All of these are complicated, and ultimately we decided that such a mechanism
creates too many problems to solve.

Alternatively, we've chosen the simple approach, which is to simply provide a
flag for resetting the compression context. For the common case (no proxy), this
fine because most requests are to the same origin and we never need to reset the
context. For cases where we are using two different origins over a single SPDY
session, we simply reset the compression state between each transition.

## 4.6 Unidirectional streams

Many readers notice that unidirectional streams are both a bit confusing in
concept and also somewhat redundant. If the recipient of a stream doesn't wish
to send data on a stream, it could simply send a SYN_REPLY with the FLAG_FIN bit
set. The FLAG_UNIDIRECTIONAL is, therefore, not necessary.

It is true that we don't need the UNIDIRECTIONAL markings. It is added because
it avoids the recipient of pushed streams from needing to send a set of empty
frames (e.g. the SYN_STREAM w/ FLAG_FIN) which otherwise serve no purpose.

## 4.7 Data Compression

Generic compression of data portion of the streams (as opposed to compression of
the headers) without knowing the content of the stream is redundant. There is no
value in compressing a stream which is already compressed. Because of this, SPDY
initially allowed data compression to be optional. We included it because study
of existing websites shows that many sites are not using compression as they
should, and users suffer because of it. We wanted a mechanism where, at the SPDY
layer, site administrators could simply force compression - it is better to
compress twice than to not compress.

Overall, however, with this feature being optional and sometimes redundant, it
was unclear if it was useful at all. We removed it from the specification.

## 4.8 Server Push

A subtle but important point is that server push streams must be declared before
the associated stream is closed. The reason for this is so that proxies have a
lifetime for which they can discard information about previous streams. If a
pushed stream could associate itself with an already-closed stream, then
endpoints would not have a specific lifecycle for when they could disavow
knowledge of the streams which went before.

---

# 5. Security Considerations

## 5.1 Use of Same-origin constraints

This specification uses the [same-origin
policy](http://mbelshe.github.com/SPDY-Specification/draft-mbelshe-spdy-00.xml#RFC6454)
in all cases where verification of content is required.

## 5.2 HTTP Headers and SPDY Headers

At the application level, HTTP uses name/value pairs in its headers. Because
SPDY merges the existing HTTP headers with SPDY headers, there is a possibility
that some HTTP applications already use a particular header name. To avoid any
conflicts, all headers introduced for layering HTTP over SPDY are prefixed with
":". ":" is not a valid sequence in HTTP header naming, preventing any possible
conflict.

## 5.3 Cross-Protocol Attacks

By utilizing TLS, we believe that SPDY introduces no new cross-protocol attacks.
TLS encrypts the contents of all transmission (except the handshake itself),
making it difficult for attackers to control the data which could be used in a
cross-protocol attack.

## 5.4 Server Push Implicit Headers

Pushed resources do not have an associated request. In order for existing HTTP
cache control validations (such as the Vary header) to work, however, all cached
resources must have a set of request headers. For this reason, browsers MUST be
careful to inherit request headers from the associated stream for the push. This
includes the 'Cookie' header.

## 5.5 Use of Padding

Padding within SPDY is not intended as a replacement for general purpose
padding, such as might be provided by TLS. Redundant padding could even be
counterproductive.

Padding can be used to hide the exact size of frame content. Padding is provided
to mitigate specific attacks within HTTP. For example, attacks where compressed
content includes both attacker-controlled plaintext and secret data (see for
example,
[BREACH](http://breachattack.com/resources/BREACH%20-%20SSL,%20gone%20in%2030%20seconds.pdf)).

Use of padding can result in less protection than might seem immediately
obvious. In particular, randomized amounts of padding only increase the number
of frames that an attacker has to observe to recover the length. Padding to a
constant size is preferable, since that reveals minimal size information. For
attacks based on compression, disabling compression might be preferable to use
of padding.

Intermediaries SHOULD NOT remove padding; though an intermediary could remove
padding and add differing amounts if the intent is to improve the protections
padding affords.

---

# 6. Privacy Considerations

## 6.1 Long Lived Connections

SPDY aims to keep connections open longer between clients and servers in order
to reduce the latency when a user makes a request. The maintenance of these
connections over time could be used to expose private information. For example,
a user using a browser hours after the previous user stopped using that browser
may be able to learn about what the previous user was doing. This is a problem
with HTTP in its current form as well, however the short lived connections make
it less of a risk.

## 6.2 SETTINGS frame

The SPDY SETTINGS frame allows servers to store out-of-band transmitted
information about the communication between client and server on the client.
Although this is intended only to be used to reduce latency, renegade servers
could use it as a mechanism to store identifying information about the client in
future requests.

Clients implementing privacy modes, such as Google Chrome's "incognito mode",
may wish to disable client-persisted SETTINGS storage.

Clients MUST clear persisted SETTINGS information when clearing the cookies.

TODO: Put range maximums on each type of setting to limit inappropriate uses.

---

# 7. Incompatibilities with SPDY draft 3

Here is a list of the major changes between draft 3.1 and draft 3.

*   Revision of flow control to include session-level flow control.
*   Removal of CREDENTIAL frame and associated error codes.
*   GOAWAY clarified to ensure that SETTINGS and GOAWAY remain
            orthogonal.

Here is a list of the major changes between draft 3.2 and draft 3.1

*   Added DATA frame padding

---

# 8. Requirements Notation

The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD",
"SHOULD NOT", "RECOMMENDED", "MAY", and "OPTIONAL" in this document are to be
interpreted as described in [RFC
2119](http://mbelshe.github.com/SPDY-Specification/draft-mbelshe-spdy-00.xml#RFC2119).

---

# 9. Acknowledgements

Many individuals have contributed to the design and evolution of SPDY: Adam
Langley, Wan-Teh Chang, Jim Morrison, Mark Nottingham, Alyssa Wilk, Costin
Manolache, William Chan, Vitaliy Lvin, Joe Chan, Adam Barth, Ryan Hamilton,
Gavin Peters, Kent Alstad, Kevin Lindsay, Paul Amer, Fan Yang, Jonathan
Leighton, Hasan Khalil, Ryan

---

# 10. Normative References

<table>
<tr>
<td><b>[TLSNPN]</b></td>
<td>Langley, A., “<a href="http://tools.ietf.org/html/draft-agl-tls-nextprotoneg-01">TLS Next Protocol Negotiation</a>”, &lt;<a href="http://tools.ietf.org/html/draft-agl-tls-nextprotoneg-01">http://tools.ietf.org/html/draft-agl-tls-nextprotoneg-01</a>&gt;.</td>
</tr>
<tr>
<td><b>[ASCII]</b></td>
<td>“US-ASCII. Coded Character Set - 7-Bit American Standard Code for Information Interchange. Standard ANSI X3.4-1986, ANSI, 1986.”.</td>
</tr>
<tr>
<td><b>[UDELCOMPRESSION]</b></td>
<td>Yang, F., Amer, P., and J. Leighton, “<a href="http://www.eecis.udel.edu/~amer/PEL/poc/pdf/SPDY-Fan.pdf">A Methodology to Derive SPDY’s Initial Dictionary for Zlib Compression</a>”, &lt;<a href="http://www.eecis.udel.edu/~amer/PEL/poc/pdf/SPDY-Fan.pdf">http://www.eecis.udel.edu/~amer/PEL/poc/pdf/SPDY-Fan.pdf</a>&gt;.</td>
</tr>
</table>

---

# 11. Errata

1.  Section 2.6.6 originally listed INTERNAL_ERROR as status code 11.

---

# Authors' Addresses

Mike Belshe Twist EMail:
[mbelshe@chromium.org](mailto:mbelshe@chromium.org)Roberto Peon Google, Inc
EMail: [fenix@google.com](mailto:fenix@google.com)