#ifndef NET_TOOLS_FLIP_SERVER_OTHER_DEFINES
#define NET_TOOLS_FLIP_SERVER_OTHER_DEFINES

class NullStream {
 public:
  NullStream() {}
  template <typename T>
  NullStream operator<<(T t) { return *this;}
};

#define VLOG(X) NullStream()
#define DVLOG(X) NullStream()


#endif  // NET_TOOLS_FLIP_SERVER_OTHER_DEFINES

