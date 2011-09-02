// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/evproto2
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#ifndef EVPROTO2_RPCCHANNEL_H
#define EVPROTO2_RPCCHANNEL_H

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include <event2/bufferevent.h>

#include "muduo/Atomic.h"
#include "muduo/Mutex.h"

#include <map>
#include <string>

namespace evproto
{

class EventLoop;
class RpcMessage;

namespace gpb = ::google::protobuf;

class RpcChannel : public gpb::RpcChannel
{
 public:
  RpcChannel(EventLoop* loop, const std::string& host, int port);
  RpcChannel(struct event_base *base, int fd, const std::map<std::string, gpb::Service*>&);
  ~RpcChannel();

  void CallMethod(const gpb::MethodDescriptor* method,
                  gpb::RpcController* controller,
                  const gpb::Message* request,
                  gpb::Message* response,
                  gpb::Closure* done);

  enum ErrorCode
  {
    kNoError = 0,
    kInvalidLength,
    kCheckSumError,
    kInvalidNameLen,
    kUnknownMessageType,
    kParseError,
  };

 private:
  void onRead();
  static ErrorCode parse(const char* buf, int len, RpcMessage* message);
  void onMessage(const RpcMessage&);
  void sendMessage(const RpcMessage&);
  void doneCallback(::google::protobuf::Message* response, int64_t id);

  void connectFailed();
  void connected();

  static void readCallback(struct bufferevent *bev, void *ptr);
  static void eventCallback(struct bufferevent *bev, short events, void *ptr);

  struct OutstandingCall
  {
    ::google::protobuf::Message* response;
    ::google::protobuf::Closure* done;
  };

  struct bufferevent* evConn_;
  bool connectFailed_;
  muduo::AtomicInt64 id_;

  muduo::MutexLock mutex_;
  std::map<int64_t, OutstandingCall> outstandings_;

  std::map<std::string, gpb::Service*> services_;
};

}

#endif  // EVPROTO2_RPCCHANNEL_H

