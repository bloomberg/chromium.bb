# Builds the Netty fork of Tomcat Native. See http://netty.io/wiki/forked-tomcat-native.html
{
  'targets': [
    {
      'target_name': 'netty-tcnative-so',
      'product_name': 'netty-tcnative',
      'type': 'shared_library',
      'sources': [
        'src/c/address.c',
        'src/c/bb.c',
        'src/c/dir.c',
        'src/c/error.c',
        'src/c/file.c',
        'src/c/info.c',
        'src/c/jnilib.c',
        'src/c/lock.c',
        'src/c/misc.c',
        'src/c/mmap.c',
        'src/c/multicast.c',
        'src/c/network.c',
        'src/c/os.c',
        'src/c/os_unix_system.c',
        'src/c/os_unix_uxpipe.c',
        'src/c/poll.c',
        'src/c/pool.c',
        'src/c/proc.c',
        'src/c/shm.c',
        'src/c/ssl.c',
        'src/c/sslcontext.c',
        'src/c/sslinfo.c',
        'src/c/sslnetwork.c',
        'src/c/ssl_private.h',
        'src/c/sslutils.c',
        'src/c/stdlib.c',
        'src/c/tcn_api.h',
        'src/c/tcn.h',
        'src/c/tcn_version.h',
        'src/c/thread.c',
        'src/c/user.c',
      ],
      'include_dirs': [
        '../apache-portable-runtime/src/include',
      ],
      'defines': [
        'HAVE_OPENSSL',
      ],
      'cflags': [
        '-w',
      ],
      'dependencies': [
        '../apache-portable-runtime/apr.gyp:apr',
        '../boringssl/boringssl.gyp:boringssl',
      ],
      'variables': {
        'component': 'static_library',
        'use_native_jni_exports': 1,
      }
    },
    {
      'target_name': 'netty-tcnative',
      'type': 'none',
      'variables': {
        'java_in_dir': 'src/java',
        'javac_includes': [ '**/org/apache/tomcat/jni/*.java' ],
      },
      'includes': [ '../../build/java.gypi' ],
      'dependencies': [
        'netty-tcnative-so',
      ],
    },
  ],
}