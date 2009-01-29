{
  'targets': [
    {
      'name': 'libevent',  # Maybe this should be called "event" so that it
                           # outputs libevent.a.
      'type': 'static_library',
      'sources': [
        'buffer.c',
        'evbuffer.c',
        'evdns.c',
        'event.c',
        'event_tagging.c',
        'evrpc.c',
        'evutil.c',
        'http.c',
        'log.c',
        'poll.c',
        'select.c',
        'signal.c',
        'strlcpy.c',
      ],
      'defines': [
        'HAVE_CONFIG_H',
      ],
      'include_dirs': [
        '.',   # libevent includes some of its own headers with #include <...>
               # instead of #include "..."
      ],
      'conditions': [
        # Platform-specific implementation files.
        [ 'OS==linux', { 'sources': [ 'epoll.c', 'epoll_sub.c' ] } ],
        [ 'OS==mac',   { 'sources': [ 'kqueue.c' ] } ],

        # Pre-generated config.h files are platform-specific and live in
        # platform-specific directories.
        [ 'OS==linux', { 'include_dirs': [ 'linux' ] } ],
        [ 'OS==mac',   { 'include_dirs': [ 'mac' ] } ],
      ],
    },
  ],
}
