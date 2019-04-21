This directory contains the code for building a Window Service
implementation on top of an existing Aura hierarchy.

Each client is managed by an instance of WindowTree. In this directory, a client
generally means a unique connection to the WindowService. More specifically a
client is an implementation of ws::mojom::WindowTreeClient.
WindowTree implements the ws::mojom::WindowTree implementation that is
passed to the client. WindowTree creates a ClientRoot for the window the client
is embedded in, as well as a ClientRoot for all top-level window requests.

Clients establish a connection to the WindowService by configuring Aura with a
mode of MUS. See aura::Env::Mode for details.

The WindowService provides a way for one client to embed another client in a
specific window (application composition). Embedding establishes a connection
to a new client and provides the embedded client with a window to use. See the
mojom for more details.

For example, on Chrome OS, Ash uses the WindowService to enable separate
processes, such as the tap_visualizer, to connect to the WindowService. The
tap_visualizer is a client of the WindowService. The tap_visualizer uses the
WindowService to create and manage windows, receive events, and ultimately
draw to the screen (using Viz). This is mostly seamless to the tap_visualizer.
The tap_visualizer configures Views to use Mus, which results in Views and Aura,
using the WindowService.

## Ids

Each client connected to the Window Service is assigned a unique id inside the
Window Service. This id is a monotonically increasing uint32_t. This is often
referred to as the client_id.

As clients do not know their id, they always supply 0 as the client id in the
mojom related functions. Internally the Window Service maps 0 to the real client
id.

Windows have a couple of different (related) ids.

### ClientWindowId

ClientWindowId is a uint64_t pairing of a client_id and a window_id. The
window_id is a uint32_t assigned by the client, and should be unique within that
client's scope. When communicating with the Window Service, clients may use 0 as
the client_id to refer to their own windows. The Window Service maps 0 to the
real client_id. In Window Service code the id from the client is typically
referred to as the transport_window_id. Mojom functions that receive the
transport_window_id map it to a ClientWindowId. ClientWindowId is a real class
that provides type safety.

When a client is embedded in an existing window, the embedded client is given
visibility to a Window created by the embedder. In this case the Window Service
supplies the ClientWindowId to the embedded client and uses the ClientWindowId
at the time the Window was created (the ClientWindowId actually comes from the
FrameSinkId, see below for details on FrameSinkId). In other words, both the
embedder and embedded client use the same ClientWindowId for the Window. See
discussion on FrameSinkId for more details.

For a client to establish an embed root, it first calls
ScheduleEmbedForExistingClient(), so it can provide a window_id that is unique
within its own scope. That client then passes the returned token to what will
become its embedder to call EmbedUsingToken(). In this case, the embedder and
embedded client do not use the same ClientWindowId for the Window.

ClientWindowId is globally unique, but a Window may have multiple
ClientWindowIds associated with it.

### FrameSinkId

Each Window has a FrameSinkId that is needed for both hit-testing and
embedding. The FrameSinkId is initialized to the ClientWindowId of the client
creating the Window, but it changes during an embedding. In particular, when a
client calls Embed() the FrameSinkId of the Window changes such that the
client_id of the FrameSinkId matches the client_id of the client being
embedded and the sink_id is set to 0. The embedder is informed of this by way of
OnFrameSinkIdAllocated(). The embedded client is informed of the original
FrameSinkId (the client_id of the FrameSinkId matches the embedder's client_id).
In client code the embedded client ends up *always* using a client_id of 0 for
the FrameSinkId. This works because Viz knows the real client_id and handles
mapping 0 to the real client_id.

The FrameSinkId of top-level windows is set to the ClientWindowId from the
client requesting the top-level (top-levels are created and owned by the Window
Manager). The Window Manager is told the updated FrameSinkId when it is asked
to create the top-level (WmCreateTopLevelWindow()).

The FrameSinkId of an embed root's Window is set to the ClientWindowId of the
embed root's Window from the embedded client.

### LocalSurfaceId

The LocalSurfaceId (which contains unguessable) is necessary if the client wants
to submit a compositor-frame for the Window (it wants to show something on
screen), and not needed if the client only wants to submit a hit-test region.
The LocalSurfaceId may be assigned when the bounds and/or device-scale-factor
changes. The LocalSurfaceId can change at other times as well (perhaps to
synchronize an effect with the embedded client). The LocalSurfaceId is intended
to allow for smooth resizes and ensures at embed points the CompositorFrame from
both clients match. Client code supplies a LocalSurfaceId for windows that have
another client embedded in them as well as windows with a LayerTreeFrameSink.
The LocalSurfaceId comes from the owner of the window. The embedded client is
told of changes to the LocalSurfaceId by way of OnWindowBoundsChanged(). This is
still very much a work in progress.

FrameSinkId is derived from the embedded client, where as LocalSurfaceId
comes from the embedder.
