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

#include <map>
#include <string>

namespace evproto
{

class EventLoop;

namespace gpb = ::google::protobuf;

class RpcServer
{
 public:
  RpcServer(EventLoop* loop, int port);

  void registerService(gpb::Service*);
  void start();

 private:
  static void newConnectionCallback(struct evconnlistener *listener,
      evutil_socket_t fd, struct sockaddr *address, int socklen, void *ctx);
  void onConnection(evutil_socket_t fd);
/*

  void onMessage(const TcpConnectionPtr& conn,
                 Buffer* buf,
                 Timestamp time);
*/

  EventLoop* loop_;
  struct evconnlistener* evListener_;
  std::map<std::string, gpb::Service*> services_;
};

}

#endif  // EVPROTO2_RPCSERVER_H

