#include "RpcChannel.h"
#include "EventLoop.h"
#include "rpc.pb.h"
#include <event2/buffer.h>
#include <zlib.h>

using namespace evproto;
using std::string;

RpcChannel::RpcChannel(EventLoop* loop, const string& host, int port)
  : evConn_(bufferevent_socket_new(loop->eventBase(), -1, BEV_OPT_CLOSE_ON_FREE)),
    connectFailed_(false)
{
  bufferevent_setcb(evConn_, readCallback, NULL, eventCallback, this);
  bufferevent_socket_connect_hostname(evConn_, NULL, AF_INET, host.c_str(), port);
}

RpcChannel::RpcChannel(struct event_base* base, int fd, const std::map<std::string, gpb::Service*>& services)
  : evConn_(bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE)),
    connectFailed_(false),
    services_(services)
{
  bufferevent_setcb(evConn_, readCallback, NULL, eventCallback, this);
  bufferevent_enable(evConn_, EV_READ|EV_WRITE);
}

RpcChannel::~RpcChannel()
{
  bufferevent_free(evConn_);
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
  int readable = evbuffer_get_length(input);
  while (readable >= 12)
  {
    int be32 = 0;
    evbuffer_copyout(input, &be32, sizeof be32);
    int len = ntohl(be32);
    if (len > 64*1024*1024 || len < 8)
    {
      // FIXME: error kInvalidLength
    }
    else if (readable >= len + 4)
    {
      RpcMessage message;
      const char* data = reinterpret_cast<char*>(evbuffer_pullup(input, len + 4));
      ErrorCode errorCode = parse(data + 4, len, &message);
      if (errorCode == kNoError)
      {
        onMessage(message);
        evbuffer_drain(input, len + 4);
        readable = evbuffer_get_length(input);
      }
      else
      {
        // FIXME: error
      }
    }
    else
    {
      break;
    }
  }
}
RpcChannel::ErrorCode RpcChannel::parse(const char* buf, int len, RpcMessage* message)
{
  ErrorCode error = kNoError;

  // check sum
  int32_t be32 = 0;
  memcpy(&be32, buf + len - 4, sizeof be32);
  int32_t expectedCheckSum = ntohl(be32);
  int32_t checkSum = static_cast<int32_t>(
      ::adler32(1,
                reinterpret_cast<const Bytef*>(buf),
                len - 4));

  if (checkSum == expectedCheckSum)
  {
    if (memcmp(buf, "RPC0", 4) == 0)
    {
      // parse from buffer
      const char* data = buf + 4;
      int32_t dataLen = len - 8;
      if (message->ParseFromArray(data, dataLen))
      {
        error = kNoError;
      }
      else
      {
        error = kParseError;
      }
    }
    else
    {
      error = kUnknownMessageType;
    }
  }
  else
  {
    error = kCheckSumError;
  }

  return error;
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
  struct evbuffer* buf = evbuffer_new();
  const int byte_size = message.ByteSize();
  const int len = byte_size + 8; // RPC0 + adler32
  const int total_len = len + 4; // length prepend
  evbuffer_expand(buf, len + 4);
  struct evbuffer_iovec vec;
  evbuffer_reserve_space(buf, total_len, &vec, 1);

  uint8_t* start = static_cast<uint8_t*>(vec.iov_base);
  int len_be = htonl(len);
  memcpy(start, &len_be, sizeof len_be);
  start += 4;
  memcpy(start, "RPC0", 4);
  start += 4;
  uint8_t* end = message.SerializeWithCachedSizesToArray(start);
  assert (end - start == byte_size);
  start += byte_size;

  int32_t checkSum = static_cast<int32_t>(
      ::adler32(1,
	static_cast<const Bytef*>(vec.iov_base)+4,
	byte_size + 4));

  checkSum = htonl(checkSum);
  memcpy(start, &checkSum, sizeof checkSum);
  start += 4;

  assert(start - static_cast<uint8_t*>(vec.iov_base) == total_len);
  vec.iov_len = total_len;
  evbuffer_commit_space(buf, &vec, 1);
  bufferevent_write_buffer(evConn_, buf);
  evbuffer_free(buf);
}

void RpcChannel::connectFailed()
{
  connectFailed_ = true;
 // FIXME
}

void RpcChannel::connected()
{
  if (!connectFailed_)
  {
    bufferevent_enable(evConn_, EV_READ|EV_WRITE);
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
  else if (events & BEV_EVENT_ERROR)
  {
    printf("connect error\n");
    self->connectFailed();
  }
}
