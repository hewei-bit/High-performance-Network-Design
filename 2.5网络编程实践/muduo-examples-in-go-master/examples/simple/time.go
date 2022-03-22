package simple

import (
	"encoding/binary"
	"log"
	"net"
	"time"

	"github.com/chenshuo/muduo-examples-in-go/muduo"
)

type TimeServer struct {
	listener net.Listener
}

func NewTimeServer(listenAddr string) *TimeServer {
	server := new(TimeServer)
	server.listener = muduo.ListenTcpOrDie(listenAddr)
	return server
}

func (s *TimeServer) Serve() {
	defer s.listener.Close()
	for {
		conn, err := s.listener.Accept()
		if err == nil {
			printConn(conn, "time", "UP")
			var now int32 = int32(time.Now().Unix())
			binary.Write(conn, binary.BigEndian, &now)
			printConn(conn, "time", "DOWN")
			conn.Close()
		} else {
			log.Println("time:", err.Error())
			// TODO: break if ! temporary
		}
	}
}
