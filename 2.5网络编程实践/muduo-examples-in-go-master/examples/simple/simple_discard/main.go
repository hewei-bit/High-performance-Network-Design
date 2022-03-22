package main

import (
	"github.com/chenshuo/muduo-examples-in-go/examples/simple"
)

func main() {
	discardServer := simple.NewDiscardServer(":2009")
	discardServer.Serve()
}
