//services/identity/public/cpp contains the next-generation C++ API for
interacting with the user's Google identities. This library is almost certainly
what you want to use if you are developing a browser feature. See
//services/identity/public/cpp/README.md for details.

If you are looking to connect to the Identity Service (e.g., if you are
developing a feature that is intended to run out of the browser process), use
//services/identity/public/mojom.
