A simple thread test for NaCl

To build
  From your native_client directory: if you haven't 
  done so yet, hammer build your dbg client for the
  target machine you are on (linux, win, mac)
  This demo requires sdl.

  For example, to build on debug on linux:
    .../native_client$ ./hammer MODE=dbg-linux sdl=1

  Then hammer build the platform independent nacl code,
  which will build this test:
    .../native_client$ ./hammer MODE=nacl sdl=1

To run, from this directory execute the python script:
    .../native_client/tests/createthreads$ ./createthreads.py

