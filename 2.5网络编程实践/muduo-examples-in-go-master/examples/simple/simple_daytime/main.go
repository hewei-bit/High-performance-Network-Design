package main

import (
	"github.com/chenshuo/muduo-examples-in-go/examples/simple"
)

func main() {
	daytimeServer := simple.NewDaytimeServer(":2013")
	daytimeServer.Serve()
}
