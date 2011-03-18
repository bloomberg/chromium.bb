#include "debug_socket.h"

namespace {
fd_set* kNoFdSet = NULL;
const int kMicrosecondsInOneMillisecons = 1000;
const char* kAnyLocalHost = NULL;
sockaddr* kNoPeerAddress = NULL;
const int kWaitForOneWriteMs = 1000;
const int kTmpBufferSize = 2048;

bool InitSocketLib() {
	WSADATA wsa_data; 
  WORD version_requested = MAKEWORD(1, 1); 
  return (WSAStartup(version_requested, &wsa_data) == 0);
}

void FreeSocketLib() {
	WSACleanup();
}

void CloseSocket(SOCKET* sock) {
  if (INVALID_SOCKET != *sock) {
	  closesocket(*sock);
    *sock = INVALID_SOCKET;
  }
}

timeval CreateTimeval(int milliseconds) {
  timeval timeout; 
  timeout.tv_sec = 0;
  timeout.tv_usec = milliseconds * kMicrosecondsInOneMillisecons;
  return timeout;
}

sockaddr_in CreateSockAddr(const char* host, int port) {
  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
	if ((NULL == host) || (strlen(host) == 0)) {
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
  } else {
  	hostent* hostDescr = gethostbyname(host);
	  if (NULL != hostDescr)
      addr.sin_addr.s_addr =
        *(reinterpret_cast<unsigned long**>(hostDescr->h_addr_list)[0]);
  }
  return addr;
}

void SetSocketOptions(SOCKET sock) {
  if (INVALID_SOCKET != sock) {
	  // Setup socket to flush pending data on close.
	  linger ling;
	  ling.l_onoff  = 1;
	  ling.l_linger = 10; // The socket will remain open for a specified amount
                        // of time (in seconds).
	  ::setsockopt(sock, 
                 SOL_SOCKET, 
                 SO_LINGER, 
                 reinterpret_cast<char*>(&ling), 
                 sizeof(ling));
	// Turn off buffering.
	long opt = 1;
	::setsockopt(sock,
               IPPROTO_TCP,
               TCP_NODELAY,
               reinterpret_cast<char*>(&opt),
               sizeof(opt));
  }
}
}  // namespace

namespace debug {
ListeningSocket::ListeningSocket()
    : sock_(INVALID_SOCKET) {
  init_success_ = InitSocketLib();
}

ListeningSocket::~ListeningSocket() {
  CloseSocket(&sock_);
  if (init_success_)
    FreeSocketLib();
}

void ListeningSocket::Close() {
  CloseSocket(&sock_);
}
  
bool ListeningSocket::Setup(int port) {
  Close();
	sock_ = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == sock_)
		return false;

  // Associate local address with socket.
  sockaddr_in addr = CreateSockAddr(kAnyLocalHost, port);
	if (bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    CloseSocket(&sock_);
		return false;
	}
  // Mark a socket as accepting connections.
	if (listen(sock_, SOMAXCONN) != 0)
    CloseSocket(&sock_);
	return (INVALID_SOCKET != sock_);
}

bool ListeningSocket::Accept(Socket* new_connection, long wait_ms) {
  fd_set socks; 
  FD_ZERO(&socks);
  FD_SET(sock_, &socks);

  // Wait for incoming connection.
  timeval timeout = CreateTimeval(wait_ms);
  if (select(sock_ + 1, &socks, kNoFdSet, kNoFdSet, &timeout) < 0) {
    CloseSocket(&sock_);
    return false;
  } 
  if (!FD_ISSET(sock_, &socks)) 
    return false;

	// Accept a connection request.
	SOCKET sock = accept(sock_, kNoPeerAddress, 0);
	if(INVALID_SOCKET != sock)
		new_connection->AttachTo(sock);

	return new_connection->IsConnected();
}

Socket::Socket() 
    : sock_(INVALID_SOCKET) {
  init_success_ = InitSocketLib();
}

void Socket::AttachTo(SOCKET sock) {
  Close();
  sock_ = sock;
  SetSocketOptions(sock_);
}

Socket::~Socket() {
  CloseSocket(&sock_);
  if (init_success_)
    FreeSocketLib();
}

void Socket::Close() {
  CloseSocket(&sock_);
}

bool Socket::ConnectTo(const std::string& host, int port) {
  Close();
  sock_ = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (INVALID_SOCKET == sock_)
	  return false;

  sockaddr_in addr = CreateSockAddr(host.c_str(), port);
  if (connect(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    CloseSocket(&sock_);
  else
    SetSocketOptions(sock_);
  return IsConnected();
}

bool Socket::IsConnected() const {
  return (INVALID_SOCKET != sock_);
}

size_t Socket::Write(const void* buff, size_t sz, int wait_ms) {
  if (!IsConnected())
    return 0;

  fd_set  socks; 
  FD_ZERO(&socks);
  FD_SET(sock_, &socks);

  // Wait for 'write ready'.
  timeval timeout = CreateTimeval(wait_ms);
  if (select(sock_ + 1, kNoFdSet, &socks, kNoFdSet, &timeout) < 0) {
    CloseSocket(&sock_);
		return 0;
	}
  size_t bytes_send = 0;
  if (FD_ISSET(sock_, &socks)) {
    bytes_send = send(sock_, static_cast<const char*>(buff), sz, 0);
		if (bytes_send < 0)
      CloseSocket(&sock_);
	}
	return bytes_send;
}

void Socket::WriteAll(const void* buff, size_t sz) {
  const char* ptr = static_cast<const char*>(buff);
  while (sz) {
    long wr = Write(ptr, sz, kWaitForOneWriteMs);
    if (!IsConnected())
      break;
    sz -= wr;
    ptr += wr;
  }
}

void Socket::WriteAll(const Blob& blob) {
  char buff[kTmpBufferSize];
  size_t pos = 0;
  while (pos < blob.Size()) {
    size_t num = blob.Size() - pos;
    if (num > sizeof(buff))
      num = sizeof(buff);
    for (size_t i = 0; i < num; i++)
      buff[i] = blob[pos + i];
    WriteAll(buff, num);
    pos += num;
  }
}

size_t Socket::Read(void* buff, size_t sz, int wait_ms) {
  if (!IsConnected())
    return 0;

	fd_set socks; 
	FD_ZERO(&socks);
	FD_SET(sock_, &socks);
  timeval timeout = CreateTimeval(wait_ms);

	// Wait for data.
	if (select(sock_ + 1, &socks, kNoFdSet, kNoFdSet, &timeout) < 0) {
    CloseSocket(&sock_);
		return 0;
	}
  // No data available.
	if (!FD_ISSET(sock_, &socks)) 
		return 0;

	size_t read_bytes = recv(sock_, static_cast<char*>(buff), sz, 0);
	if ((SOCKET_ERROR == read_bytes) || (0 == read_bytes)) {
    CloseSocket(&sock_);
		return 0;
	}
  return read_bytes;
}
}  // namespace debug
