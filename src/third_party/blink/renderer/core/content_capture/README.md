# ContentCapture

[Rendered](https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/core/content_capture/README.md)

This directory contains ContentCapture which is for capturing on-screen text
content and streaming it to a client.

The implementation injects a cc::NodeHolder into cc::DrawTextBlobOp in paint
stage, schedules a best-effort task to retrieve on-screen text content (using
an r-tree to capture all cc::NodeHolder intersecting the screen), and streams
the text out through ContentCaptureClient interface. The ContentCaptureTask is
a best-effort task in the idle queue and could be paused if there are
higher-priority tasks.

There are two ways to associate cc::NodeHolder with Node which are being
compared. One of these approaches will be removed once performance data is
available.

 1. NodeHolder::Type::kID where the ID from DOMNodeIds::IdForNode() is used to
identify nodes.
 2. NodeHolder::Type::kTextHolder which uses ContentCapture’s implementation of
cc::TextHolder to identify the node. Since the NodeHolder is captured and sent
out in a separate task, the Node could be removed. To avoid using removed
Nodes, a weak pointer to Node is implemented. This weak pointer will be reset
when the Node associated with the LayoutText is destroyed. Though a Node can be
associated with multiple LayoutObjects, only the main LayoutObject has the
pointer back to Node, so it isn’t a problem to use the removing of LayoutText
to reset the pointer to the Node.
