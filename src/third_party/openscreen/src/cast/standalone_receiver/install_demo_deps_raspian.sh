#!/usr/bin/env sh

# Installs dependencies necessary for libSDL and libAVcodec on
# Raspberry PI units running Raspian.

sudo apt-get install libavcodec58=7:4.1.4* libavcodec-dev=7:4.1.4*       \
                     libsdl2-2.0-0=2.0.9* libsdl2-dev=2.0.9*             \
                     libavformat-dev=7:4.1.4*
