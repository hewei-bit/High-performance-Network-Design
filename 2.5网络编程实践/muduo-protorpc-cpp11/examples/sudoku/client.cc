#include <examples/sudoku/sudoku.pb.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpConnection.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

class RpcClient : noncopyable
{
 public:
  RpcClient(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop),
      client_(loop, serverAddr, "RpcClient"),
      channel_(new RpcChannel),
      stub_(get_pointer(channel_))
  {
    client_.setConnectionCallback(
        std::bind(&RpcClient::onConnection, this, _1));
    client_.setMessageCallback(
        std::bind(&RpcChannel::onMessage, get_pointer(channel_), _1, _2, _3));
    // client_.enableRetry();
  }

  void connect()
  {
    client_.connect();
  }

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    if (conn->connected())
    {
      //channel_.reset(new RpcChannel(conn));
      channel_->setConnection(conn);
      sudoku::SudokuRequest request;
      request.set_checkerboard("001010");

      stub_.Solve(request, bind(&RpcClient::solved, this, _1));
    }
  }

  void solved(const sudoku::SudokuResponsePtr& resp)
  {
    LOG_INFO << "solved:\n" << resp->DebugString();
    loop_->quit();
  }

  EventLoop* loop_;
  TcpClient client_;
  RpcChannelPtr channel_;
  sudoku::SudokuService::Stub stub_;
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 1)
  {
    EventLoop loop;
    InetAddress serverAddr(argv[1], 9981);

    RpcClient rpcClient(&loop, serverAddr);
    rpcClient.connect();
    loop.loop();
    google::protobuf::ShutdownProtobufLibrary();
  }
  else
  {
    printf("Usage: %s host_ip\n", argv[0]);
  }
}

