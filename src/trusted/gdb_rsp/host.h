/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

//  This module provides an object for handling RSP responsibilities of
//  the host side of the connection.  The host behaves like a cache, and
//  is responsible for syncronization of state between the Target and Host.
//  For example, the Host is responsible for updating the thread context
//  before restarting the Target, and for updating it's internal array of
//  threads whenever the Target stops.

#ifndef NATIVE_CLIENT_GDB_RSP_HOST_H_
#define NATIVE_CLIENT_GDB_RSP_HOST_H_ 1

#include <map>
#include <string>
#include <vector>

#include "native_client/src/trusted/port/std_types.h"

namespace gdb_rsp {

class Abi;
class Packet;
class Session;

class Host {
 public:
  enum Status {
    HS_RUNNING = 0,
    HS_STOPPED = 1,
    HS_EXIT = 2,
    HS_TERMINATED = 3
  };

  // The Host::Thread class represents a thread on the Target side, providing
  // a cache for its state, which is automatically updated by the parent Host
  // object whenever the target starts or stops.
  class Thread {
   public:
    Thread(const Abi *abi, uint32_t id);
    ~Thread();

    public:
     uint32_t GetId() const;
     const Abi* GetAbi() const;
     void GetRegister(uint32_t index, void *dst) const;
     void SetRegister(uint32_t index, const void *src);

    private:
     uint32_t id_;
     uint8_t *ctx_;
     const Abi *abi_;

    friend class Host;
  };

  typedef std::map<uint32_t, Host::Thread*> ThreadMap_t;
  typedef std::map<std::string, std::string> PropertyMap_t;
  typedef std::vector<uint32_t> ThreadVector_t;

  explicit Host(Session *session);
  ~Host();

  bool Init();
  bool Update();

  bool Break();
  bool Detach();
  Status  GetStatus();
  int32_t GetSignal();
  Thread* GetThread(uint32_t id);
  bool GetMemory(void *dst, uint64_t addr, uint32_t size);
  bool SetMemory(const void *src, uint64_t addr, uint32_t size);

  // TODO(noelallen): Still to be implemented
#if 0
  bool Step();
  bool Continue();
#endif

 protected:
  bool Send(Packet *req, Packet *resp);
  bool SendOnly(Packet *req);

  bool Request(const std::string& req, std::string* resp);
  bool RequestOnly(const std::string& req);

  bool RequestThreadList(ThreadVector_t* ids);


 protected:
  // Read locally cached properties
  bool HasProperty(const char *name) const;
  bool ReadProperty(const char *name, std::string* val) const;

  // Read remote object
  bool ReadObject(const char *type, const char *name, std::string* val);

  // Parse a string, returning true and updating if a valid stop packet
  bool ParseStopPacket(const char *data);

 private:
  Session *session_;
  const Abi* abi_;

  PropertyMap_t properties_;
  ThreadMap_t threads_;
  int32_t lastSignal_;
  Status  status_;
};


}  // namespace gdb_rsp

#endif  // NATIVE_CLIENT_GDB_RSP_HOST_H_

