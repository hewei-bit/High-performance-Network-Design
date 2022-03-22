package main

import (
	"github.com/chenshuo/muduo-examples-in-go/examples/simple"
)

func main() {
	chargenServer := simple.NewChargenServer(":2019")
	chargenServer.ServeWithMeter()
}
