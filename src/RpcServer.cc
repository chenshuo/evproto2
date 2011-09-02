#include "RpcServer.h"
#include "RpcChannel.h"
#include "EventLoop.h"

using namespace evproto;

struct sockaddr* getListenSock(int port)
{
  static struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(port);
  return (struct sockaddr*)&sin;
}

RpcServer::RpcServer(EventLoop* loop, int port)
  : evListener_(evconnlistener_new_bind(loop->eventBase(),
	newConnectionCallback, this, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1,
       	getListenSock(port), sizeof(struct sockaddr_in)))

{
}

void RpcServer::registerService(gpb::Service* service)
{
  const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
  services_[desc->full_name()] = service;
}

void RpcServer::onConnection(evutil_socket_t fd)
{
  struct event_base *base = evconnlistener_get_base(evListener_);
  RpcChannel* channel = new RpcChannel(base, fd, services_);
  // FIXME
}

void RpcServer::newConnectionCallback(struct evconnlistener *listener,
      evutil_socket_t fd, struct sockaddr *address, int socklen, void *ctx)
{
  printf("newConnectionCallback\n");
  RpcServer* self = static_cast<RpcServer*>(ctx);
  assert(self->evListener_ == listener);
  self->onConnection(fd);
}
