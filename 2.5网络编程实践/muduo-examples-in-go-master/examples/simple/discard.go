package simple

import (
	"io"
	"io/ioutil"
	"log"
	"net"

	"github.com/chenshuo/muduo-examples-in-go/muduo"
)

type DiscardServer struct {
	listener net.Listener
}

func NewDiscardServer(listenAddr string) *DiscardServer {
	server := new(DiscardServer)
	server.listener = muduo.ListenTcpOrDie(listenAddr)
	return server
}

func discard(c net.Conn) {
	defer c.Close()
	data := make([]byte, 4096)
	var total int64
	for {
		n, err := c.Read(data)
		total += int64(n)
		if err != nil {
			log.Println("discard:", err.Error())
			break
		}
	}
	log.Println("total", total)
	printConn(c, "discard", "DOWN")
}

func discardAdv(c net.Conn) {
	defer c.Close()
	total, err := io.Copy(ioutil.Discard, c)
	if err != nil {
		log.Println("discard:", err.Error())
	}
	log.Println("total", total)
	printConn(c, "discard", "DOWN")
}

func (s *DiscardServer) Serve() {
	serveTcp(s.listener, discardAdv, "discard")
}
