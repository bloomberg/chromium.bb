IdentityManager serves as the primary client-side interface to the Identity
Service, encapsulating a connection to a remote implementation of
identity::mojom::IdentityManager. It provides conveniences over the bare Mojo
interfaces such as:

- Synchronous access to the information of the primary account (via caching)

IMPLEMENTATION NOTES

The IdentityManager is being developed in parallel with the implementation and
interfaces of the Identity Service itself. The motivation is to allow clients to
be converted to use the IdentityManager in a parallel and pipelined fashion with
building out the Identity Service as the backing implementation of the
IdentityManager.

In the near term, IdentityManager is backed directly by //components/signin
classes. We are striving to make its interactions with those classes as similar
as possible to its eventual interaction with the Identity Service. In places
where those interactions vary significantly from the envisioned eventual
interaction with the Identity Service, we have placed TODOs.
