#include "../RpcChannel.h"
#include "../EventLoop.h"
#include "kvdb.pb.h"

void donePut(kvdb::PutResponse* response)
{
  printf("put response: %s\n", response->DebugString().c_str());
}

void doneGet(kvdb::GetResponse* response)
{
  printf("get response: %s\n", response->DebugString().c_str());
}

int main(int argc, char* argv[])
{
  if (argc < 2)
  {
    printf("Usage: client server_addr\n");
    return 0;
  }

  evproto::EventLoop loop;
  evproto::RpcChannel channel(&loop, argv[1], 12345);
  kvdb::LeveldbService::Stub remoteService(&channel);

  {
    kvdb::PutRequest request;
    request.set_key("name");
    request.set_value("Shuo Chen");
    kvdb::PutResponse* response = new kvdb::PutResponse;
    remoteService.Put(NULL, &request, response, NewCallback(&donePut, response));
  }

  {
    kvdb::GetRequest request;
    request.set_key("name");
    kvdb::GetResponse* response = new kvdb::GetResponse;
    remoteService.Get(NULL, &request, response, NewCallback(&doneGet, response));
  }

  loop.loop();
}
