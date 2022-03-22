package muduo

import (
	"log"
	"net"
	"time"
)

func Check(v bool, msg string) {
	if !v {
		panic(msg)
	}
}

func PanicOnError(err error) {
	if err != nil {
		panic(err)
	}
}

func ListenTcpOrDie(listenAddr string) net.Listener {
	listener, err := net.Listen("tcp", listenAddr)
	PanicOnError(err)
	return listener
}

type TcpServer interface {
	ServeConn(net.Conn)
}

func ServeTcp(l net.Listener, server TcpServer, name string) error {
	defer l.Close()
	var tempDelay time.Duration // how long to sleep on accept failure
	for {
		conn, err := l.Accept()
		if err != nil {
			if ne, ok := err.(net.Error); ok && ne.Temporary() {
				if tempDelay == 0 {
					tempDelay = 5 * time.Millisecond
				} else {
					tempDelay *= 2
				}
				if max := 1 * time.Second; tempDelay > max {
					tempDelay = max
				}
				log.Printf("%v: Accept error: %v; retrying in %v", name, err, tempDelay)
				time.Sleep(tempDelay)
				continue
			}
			return err
		}
		tempDelay = 0
		go server.ServeConn(conn)
	}
}
