package main

import (
	"log"

	"github.com/chenshuo/muduo-examples-in-go/examples/asio/chat"
)

func main() {
	log.SetFlags(log.LstdFlags | log.Lmicroseconds | log.Lshortfile)

	server := chat.NewChatServer(":3399")
	server.Run()
}
