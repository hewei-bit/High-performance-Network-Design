// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPCLIENT_H
#define MUDUO_NET_TCPCLIENT_H

#include <boost/noncopyable.hpp>

#include <muduo/base/Mutex.h>
#include <muduo/net/TcpConnection.h>

namespace muduo
{
namespace net
{

class Connector;
typedef boost::shared_ptr<Connector> ConnectorPtr;

class TcpClient : boost::noncopyable
{
 public:
  // TcpClient(EventLoop* loop);
  // TcpClient(EventLoop* loop, const string& host, uint16_t port);
  TcpClient(EventLoop* loop,
            const InetAddress& serverAddr,
            const string& name);
  ~TcpClient();  // force out-line dtor, for scoped_ptr members.

  void connect();
  void disconnect();
  void stop();

  TcpConnectionPtr connection() const
  {
    MutexLockGuard lock(mutex_);
    return connection_;
  }

  EventLoop* getLoop() const { return loop_; }
  bool retry() const;
  void enableRetry() { retry_ = true; }

  /// Set connection callback.
  /// Not thread safe.
  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }

  /// Set message callback.
  /// Not thread safe.
  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }

  /// Set write complete callback.
  /// Not thread safe.
  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }

#ifdef __GXX_EXPERIMENTAL_CXX0X__
  void setConnectionCallback(ConnectionCallback&& cb)
  { connectionCallback_ = std::move(cb); }
  void setMessageCallback(MessageCallback&& cb)
  { messageCallback_ = std::move(cb); }
  void setWriteCompleteCallback(WriteCompleteCallback&& cb)
  { writeCompleteCallback_ = std::move(cb); }
#endif

 private:
  /// Not thread safe, but in loop
  void newConnection(int sockfd);
  /// Not thread safe, but in loop
  void removeConnection(const TcpConnectionPtr& conn);

  EventLoop* loop_;
  ConnectorPtr connector_; // avoid revealing Connector
  const string name_;
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  bool retry_;   // atomic
  bool connect_; // atomic
  // always in loop thread
  int nextConnId_;
  mutable MutexLock mutex_;
  TcpConnectionPtr connection_; // @GuardedBy mutex_
};

}
}

#endif  // MUDUO_NET_TCPCLIENT_H
