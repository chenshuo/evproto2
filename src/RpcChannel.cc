#include "RpcChannel.h"
#include "EventLoop.h"
#include "rpc.pb.h"
#include <event2/buffer.h>
#include <zlib.h>

#include <event2/thread.h>

#if !defined(LIBEVENT_VERSION_NUMBER) || LIBEVENT_VERSION_NUMBER < 0x02000a00
#error "This version of Libevent is not supported; Get 2.0.10 or later."
#endif

struct InitObj
{
  InitObj()
  {
#if EVTHREAD_USE_PTHREADS_IMPLEMENTED
  GOOGLE_CHECK_EQ(::evthread_use_pthreads(), 0);
#endif

#ifndef NDEBUG
  // ::evthread_enable_lock_debuging();
  // ::event_enable_debug_mode();
#endif

  GOOGLE_CHECK_EQ(LIBEVENT_VERSION_NUMBER, ::event_get_version_number())
    << "libevent2 version number mismatch";
  }
};

static InitObj initObj;

#include "Codec-inl.h"

using namespace evproto;
using std::string;

RpcChannel::RpcChannel(EventLoop* loop, const string& host, int port)
  : evConn_(bufferevent_socket_new(loop->eventBase(), -1, BEV_OPT_CLOSE_ON_FREE)),
    connectFailed_(false),
    disconnect_cb_(NULL),
    ptr_(NULL)
{
  bufferevent_setcb(evConn_, readCallback, NULL, eventCallback, this);
  bufferevent_socket_connect_hostname(evConn_, NULL, AF_INET, host.c_str(), port);
}

RpcChannel::RpcChannel(struct event_base* base, int fd, const std::map<std::string, gpb::Service*>& services)
  : evConn_(bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE)),
    connectFailed_(false),
    disconnect_cb_(NULL),
    ptr_(NULL),
    services_(services)
{
  bufferevent_setcb(evConn_, readCallback, NULL, eventCallback, this);
  bufferevent_enable(evConn_, EV_READ|EV_WRITE);
}

RpcChannel::~RpcChannel()
{
  bufferevent_free(evConn_);
  printf("~RpcChannel()\n");
}

void RpcChannel::setDisconnectCb(disconnect_cb cb, void* ptr)
{
  disconnect_cb_ = cb;
  ptr_ = ptr;
}

void RpcChannel::CallMethod(const gpb::MethodDescriptor* method,
                            gpb::RpcController* controller,
                            const gpb::Message* request,
                            gpb::Message* response,
                            gpb::Closure* done)
{
  RpcMessage message;
  message.set_type(REQUEST);
  int64_t id = id_.incrementAndGet();
  message.set_id(id);
  message.set_service(method->service()->full_name());
  message.set_method(method->name());
  message.set_request(request->SerializeAsString()); // FIXME: error check
  sendMessage(message);

  OutstandingCall out = { response, done };
  muduo::MutexLockGuard lock(mutex_);
  outstandings_[id] = out;
}

void RpcChannel::onRead()
{
  struct evbuffer* input = bufferevent_get_input(evConn_);
  ParseErrorCode errorCode = read(input, this);
  if (errorCode != kNoError)
  {
    // FIXME:
  }
}

void RpcChannel::onMessage(const RpcMessage& message)
{
  if (message.type() == RESPONSE)
  {
    int64_t id = message.id();
    assert(message.has_response());

    OutstandingCall out = { NULL, NULL };

    {
      muduo::MutexLockGuard lock(mutex_);
      std::map<int64_t, OutstandingCall>::iterator it = outstandings_.find(id);
      if (it != outstandings_.end())
      {
        out = it->second;
        outstandings_.erase(it);
      }
    }

    if (out.response)
    {
      out.response->ParseFromString(message.response());
      if (out.done)
      {
        out.done->Run();
      }
      delete out.response;
    }
  }
  else if (message.type() == REQUEST)
  {
    // FIXME: extract to a function
    std::map<std::string, gpb::Service*>::const_iterator it = services_.find(message.service());
    if (it != services_.end())
    {
      gpb::Service* service = it->second;
      assert(service != NULL);
      const gpb::ServiceDescriptor* desc = service->GetDescriptor();
      const gpb::MethodDescriptor* method
	= desc->FindMethodByName(message.method());
      if (method)
      {
	gpb::Message* request = service->GetRequestPrototype(method).New();
	request->ParseFromString(message.request());
	gpb::Message* response = service->GetResponsePrototype(method).New();
	int64_t id = message.id();
	service->CallMethod(method, NULL, request, response,
	    NewCallback(this, &RpcChannel::doneCallback, response, id));
        delete request;
      }
      else
      {
	// FIXME:
      }
    }
    else
    {
      // FIXME:
    }
  }
}

void RpcChannel::doneCallback(::google::protobuf::Message* response, int64_t id)
{
  RpcMessage message;
  message.set_type(RESPONSE);
  message.set_id(id);
  message.set_response(response->SerializeAsString()); // FIXME: error check
  sendMessage(message);
  delete response;
}

void RpcChannel::sendMessage(const RpcMessage& message)
{
  send(evConn_, message);
}

void RpcChannel::connectFailed()
{
  connectFailed_ = true;
  disconnected();
}

void RpcChannel::connected()
{
  if (!connectFailed_)
  {
    bufferevent_enable(evConn_, EV_READ|EV_WRITE);
  }
}

void RpcChannel::disconnected()
{
  if (disconnect_cb_)
  {
    disconnect_cb_(this, ptr_);
  }
}

void RpcChannel::readCallback(struct bufferevent* bev, void* ptr)
{
  RpcChannel* self = static_cast<RpcChannel*>(ptr);
  assert(self->evConn_ == bev);
  self->onRead();
}

void RpcChannel::eventCallback(struct bufferevent* bev, short events, void* ptr)
{
  RpcChannel* self = static_cast<RpcChannel*>(ptr);
  if (events & BEV_EVENT_CONNECTED)
  {
    printf("connected\n");
    self->connected();
  }
  else if (events & BEV_EVENT_EOF)
  {
    printf("disconnected\n");
    self->disconnected();
  }
  else if (events & BEV_EVENT_ERROR)
  {
    printf("connect error\n");
    self->connectFailed();
  }
}
