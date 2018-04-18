This directory contains the code for building a Window Service
implementation on top of an existing Aura hierarchy. There are two
distinct ways of attaching clients to the WindowService:

+ With a WindowTreeFactory. This class provides an implementation of
  mojom::WindowTreeFactory that internally creates
  WindowServiceClientBindings for each mojom::WindowTreeRequest. The
  following shows how to do this:

```cpp
WindowService window_service;
WindowTreeFactory window_tree_factory(&window_service);
service_manager::BinderRegistry registry;
registry.AddInterface<mojom::WindowTreeFactory>(
    base::BindRepeating(&WindowTreeFactory::AddBinding,
                        base::Unretained(&window_tree_factory)));
```

+ Create a WindowServiceClientBinding to embed a client in a
  Window. This is useful when obtaining mojom::WindowTreeClient
  through some other means (perhaps another mojo api):
```cpp
WindowService window_service;
aura::Window* window_to_embed_client_in;
mojom::WindowTreeClientPtr window_tree_client;
WindowServiceClientBinding binding;
// This is Run() when the connection to the client is lost. Typically this
// signal is used to delete the WindowServiceClientBinding.
base::Closure connection_lost_callback;
binding.InitForEmbed(&window_service,
                     std::move(window_tree_client),
                     window_to_embed_client_in,
                     std::move(connection_lost_callback));
```

Each client is managed by a WindowServiceClient. WindowServiceClient
implements the WindowTree implementation that is passed to the
client. WindowServiceClient creates a ClientRoot for the window the
client is embedded in, as well as a ClientRoot for all top-level
window requests.

Clients use the WindowService by configuring Aura with a mode of
mus. See aura::Env::Mode for details.
