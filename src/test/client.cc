#include "../RpcChannel.h"
#include "../EventLoop.h"
#include "echo.pb.h"

void done(echo::EchoResponse* response)
{
  printf("response: %s\n", response->payload().c_str());
}

int main()
{
  evproto::EventLoop loop;
  evproto::RpcChannel channel(&loop, "10.0.0.8", 8888);
  echo::EchoService::Stub remoteService(&channel);

  echo::EchoRequest request;
  request.set_payload("Hello");
  echo::EchoResponse* response = new echo::EchoResponse;
  remoteService.Echo(NULL, &request, response, NewCallback(&done, response));

  loop.loop();
}
