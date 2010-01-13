#ifndef NET_TOOLS_FLIP_SERVER_OTHER_DEFINES
#define NET_TOOLS_FLIP_SERVER_OTHER_DEFINES

#define CHECK_EQ(X, Y) CHECK((X) == (Y))
#define CHECK_NE(X, Y) CHECK((X) != (Y))
#define CHECK_GE(X, Y) CHECK((X) >= (Y))
#define CHECK_GT(X, Y) CHECK((X) > (Y))
#define CHECK_LE(X, Y) CHECK((X) <= (Y))
#define CHECK_LT(X, Y) CHECK((X) < (Y))

class NullStream {
 public:
  NullStream() {}
  template <typename T>
  NullStream operator<<(T t) { return *this;}
};

#define VLOG(X) NullStream()
#define DVLOG(X) NullStream()


#endif  // NET_TOOLS_FLIP_SERVER_OTHER_DEFINES

