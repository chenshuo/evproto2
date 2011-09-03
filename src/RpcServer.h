// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/evproto2
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#ifndef EVPROTO2_RPCSERVER_H
#define EVPROTO2_RPCSERVER_H

#include <event2/listener.h>
#include <google/protobuf/service.h>

#include "muduo/Mutex.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace evproto
{

class EventLoop;
class RpcChannel;

namespace gpb = ::google::protobuf;

class RpcServer
{
 public:
  RpcServer(EventLoop* loop, int port);
  ~RpcServer();

  void setThreadNum(int numThreads);
  void registerService(gpb::Service*);
  void start();

 private:
  static void newConnectionCallback(struct evconnlistener* listener,
      evutil_socket_t fd, struct sockaddr* address, int socklen, void* ctx);
  static void disconnectCallback(RpcChannel*, void* ctx);
  static void* runLoop(void* ptr);

  void onConnect(evutil_socket_t fd);
  void onDisconnect(RpcChannel*);

  struct evconnlistener* evListener_;
  std::vector<struct event_base*> loops_;
  int currLoop_;
  std::map<std::string, gpb::Service*> services_;

  muduo::MutexLock mutex_;
  std::set<RpcChannel*> channels_;
};

}

#endif  // EVPROTO2_RPCSERVER_H

