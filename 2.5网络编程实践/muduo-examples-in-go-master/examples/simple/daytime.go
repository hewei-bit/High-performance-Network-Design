package simple

import (
	"fmt"
	"log"
	"net"
	"time"

	"github.com/chenshuo/muduo-examples-in-go/muduo"
)

type DaytimeServer struct {
	listener net.Listener
}

func NewDaytimeServer(listenAddr string) *DaytimeServer {
	server := new(DaytimeServer)
	server.listener = muduo.ListenTcpOrDie(listenAddr)
	return server
}

func (s *DaytimeServer) Serve() {
	defer s.listener.Close()
	for {
		conn, err := s.listener.Accept()
		if err == nil {
			printConn(conn, "daytime", "UP")
			str := fmt.Sprintf("%v\n", time.Now())
			conn.Write([]byte(str))
			printConn(conn, "daytime", "DOWN")
			conn.Close()
		} else {
			log.Println("daytime:", err.Error())
			// TODO: break if ! temporary
		}
	}
}
