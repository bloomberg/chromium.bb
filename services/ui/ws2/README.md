This directory contains the code for building a Window Service
implementation on top of an existing Aura hierarchy.

Each client is managed by a WindowServiceClient. WindowServiceClient
implements the WindowTree implementation that is passed to the
client. WindowServiceClient creates a ClientRoot for the window the
client is embedded in, as well as a ClientRoot for all top-level
window requests.

Clients use the WindowService by configuring Aura with a mode of
MUS. See aura::Env::Mode for details.
