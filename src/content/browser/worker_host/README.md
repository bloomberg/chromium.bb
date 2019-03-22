# Web Worker Host

`content/browser/worker_host` implements the host side of web workers (dedicated
workers and shared workers). It tracks the security principal of the worker in
the renderer and uses it to broker access to mojo interfaces providing powerful
web APIs. See: [Design doc].

[Design doc]: https://docs.google.com/document/d/1Bg84lQqeJ8D2J-_wOOLlRVAtZNMstEUHmVhJqCxpjUk/edit?usp=sharing
