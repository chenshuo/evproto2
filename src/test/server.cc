#include "../RpcChannel.h"
#include "../RpcServer.h"
#include "../EventLoop.h"
#include "echo.pb.h"

namespace echo
{

class EchoServiceImpl : public EchoService
{
 public:
  virtual void Echo(::google::protobuf::RpcController* controller,
                    const ::echo::EchoRequest* request,
                    ::echo::EchoResponse* response,
                    ::google::protobuf::Closure* done)
  {
    //LOG_INFO << "EchoServiceImpl::Solve";
    response->set_payload(request->payload());
    done->Run();
  }
};

}

int main()
{
  evproto::EventLoop loop;
  evproto::RpcServer server(&loop, 8888);

  echo::EchoServiceImpl impl;
  server.registerService(&impl);

  // server.start();
  loop.loop();
}

