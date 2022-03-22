package simple

import (
	"log"
	"net"
	"time"
)

func printConn(c net.Conn, name, updown string) {
	log.Printf("%v: %v <- %v is %v\n",
		name, c.LocalAddr().String(), c.RemoteAddr().String(), updown)
}

type ServeTcpConn func(c net.Conn)

func serveTcp(l net.Listener, f ServeTcpConn, name string) error {
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
		printConn(conn, name, "UP")
		go f(conn)
	}
}
