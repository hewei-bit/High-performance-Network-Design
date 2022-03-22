package main

import (
	"github.com/chenshuo/muduo-examples-in-go/examples/simple"
)

func main() {
	timeServer := simple.NewTimeServer(":2037")
	timeServer.Serve()
}
