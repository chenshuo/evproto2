#include "../RpcChannel.h"
#include "../RpcServer.h"
#include "../EventLoop.h"
#include "kvdb.pb.h"

#include "leveldb/db.h"

namespace kvdb
{

class LeveldbServiceImpl : public LeveldbService
{
 public:
  LeveldbServiceImpl(const leveldb::Options& options, const std::string& name)
  {
    leveldb::Status status = leveldb::DB::Open(options, name, &db);
    assert(status.ok());
  }

  ~LeveldbServiceImpl()
  {
    delete db;
  }

  virtual void Get(::google::protobuf::RpcController* controller,
                       const ::kvdb::GetRequest* request,
                       ::kvdb::GetResponse* response,
                       ::google::protobuf::Closure* done)
  {
    leveldb::Status s = db->Get(leveldb::ReadOptions(),
                                request->key(),
                                response->mutable_value());
    allDone(s, response, done);
  }

  virtual void Put(::google::protobuf::RpcController* controller,
                       const ::kvdb::PutRequest* request,
                       ::kvdb::PutResponse* response,
                       ::google::protobuf::Closure* done)
  {
    leveldb::Status s = db->Put(leveldb::WriteOptions(),
                                request->key(),
                                request->value());
    allDone(s, response, done);
  }

  virtual void Delete(::google::protobuf::RpcController* controller,
                       const ::kvdb::DeleteRequest* request,
                       ::kvdb::DeleteResponse* response,
                       ::google::protobuf::Closure* done)
  {
    leveldb::Status s = db->Delete(leveldb::WriteOptions(),
                                   request->key());
    allDone(s, response, done);
  }

  virtual void Write(::google::protobuf::RpcController* controller,
                       const ::kvdb::WriteRequest* request,
                       ::kvdb::WriteResponse* response,
                       ::google::protobuf::Closure* done)
  {
    assert(0);
  }

 private:

  template<typename RESPONSE>
  void allDone(const leveldb::Status& status,
               RESPONSE* response,
               ::google::protobuf::Closure* done)
  {
    if (status.ok())
    {
      response->set_status(OK);
    }
    else
    {
      response->set_status(NOTFOUND);
    }
    done->Run();
  }

  leveldb::DB* db;
};

}

int main(int argc, char* argv[])
{
  evproto::EventLoop loop;
  evproto::RpcServer server(&loop, 12345);
  if (argc > 1)
  {
    int numThreads = atoi(argv[1]);
    server.setThreadNum(numThreads);
  }

  leveldb::Options options;
  options.create_if_missing = true;
  kvdb::LeveldbServiceImpl impl(options, "/tmp/testdb");
  server.registerService(&impl);

  // server.start();
  loop.loop();
}

