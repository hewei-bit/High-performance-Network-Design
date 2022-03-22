package main

import (
	"github.com/chenshuo/muduo-examples-in-go/examples/simple"
)

func main() {
	echoServer := simple.NewEchoServer(":2019")
	echoServer.Serve()
}
