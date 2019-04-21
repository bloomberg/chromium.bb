//services/identity/public/cpp contains the next-generation C++ API for
interacting with the user's Google identities. In the long term, this library
will become a client library for the Identity Service. In the present, it is
being developed based directly on the internal C++ classes in
//components/signin and //google_apis/gaia. This library is almost certainly
what you want to use if you are developing a browser feature. See
//services/identity/public/cpp/README.md for details. 

If you are looking to connect to the Identity Service, the current way to do so
is via //services/identity/public/mojom. You may wish to do this if you are
developing a feature that is intended to run out of the browser process and
does not require having a view of Google identity that is synchronously updated
with the view of Google identity that the browser sees.
