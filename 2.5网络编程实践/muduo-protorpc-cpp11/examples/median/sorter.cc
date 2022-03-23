#include <examples/median/median.pb.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/protorpc2/RpcServer.h>

#include <algorithm>
#include <iostream>

using namespace muduo;
using namespace muduo::net;

bool kDebug = true;

namespace median
{

class SorterImpl : public Sorter
{
 public:
  SorterImpl()
  {
    xsubi_[0] = static_cast<uint16_t>(getpid());
    xsubi_[1] = static_cast<uint16_t>(gethostid());
    xsubi_[2] = static_cast<uint16_t>(Timestamp::now().microSecondsSinceEpoch());
  }

  virtual void Query(const ::rpc2::EmptyPtr& request,
                     const QueryResponse* responsePrototype,
                     const RpcDoneCallback& done)
  {
    LOG_INFO << "Query";
    QueryResponse resp;
    resp.set_count(data_.size());
    if (data_.size() > 0)
    {
      resp.set_min(data_[0]);
      resp.set_max(data_.back());
    }
    done(&resp);
  }

  virtual void Search(const SearchRequestPtr& request,
                      const SearchResponse* responsePrototype,
                      const RpcDoneCallback& done)
  {
    int64_t guess = request->guess();
    LOG_INFO << "Search " << guess;
    SearchResponse resp;

    std::vector<int64_t>::iterator it = std::lower_bound(data_.begin(), data_.end(), guess);
    resp.set_smaller(it - data_.begin());
    resp.set_same(std::upper_bound(data_.begin(), data_.end(), guess) - it);
    done(&resp);
  }

  virtual void Generate(const GenerateRequestPtr& request,
                        const ::rpc2::Empty* responsePrototype,
                        const RpcDoneCallback& done)
  {
    LOG_INFO << "Generate ";
    data_.clear();
    for (int64_t i = 0; i < request->count(); ++i)
    {
      int64_t range = request->max() - request->min();
      int64_t value = request->min();
      if (range > 1)
      {
        value += nrand48(xsubi_) % range;
      }
      data_.push_back(value);
    }
    std::sort(data_.begin(), data_.end());
    if (kDebug)
    {
      std::copy(data_.begin(), data_.end(), std::ostream_iterator<int64_t>(std::cout, " "));
      std::cout << std::endl;
    }
    done(responsePrototype);
  }

 private:
  std::vector<int64_t> data_;
  unsigned short xsubi_[3];
};

}

int main(int argc, char* argv[])
{
  EventLoop loop;
  int port = argc > 1 ? atoi(argv[1]) : 5555;
  InetAddress listenAddr(static_cast<uint16_t>(port));
  median::SorterImpl impl;
  RpcServer server(&loop, listenAddr);
  server.registerService(&impl);
  server.start();
  loop.loop();
  google::protobuf::ShutdownProtobufLibrary();
}

