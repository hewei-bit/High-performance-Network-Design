package simple

import (
	"bytes"
	"fmt"
	"io"
	"log"
	"net"
	"sync/atomic"
	"time"

	"github.com/chenshuo/muduo-examples-in-go/muduo"
)

type ChargenServer struct {
	listener net.Listener
	total    int64
}

var message []byte
var repeatReader *RepeatReader

func NewChargenServer(listenAddr string) *ChargenServer {
	server := new(ChargenServer)
	server.listener = muduo.ListenTcpOrDie(listenAddr)
	return server
}

func init() {
	var buf bytes.Buffer
	for i := 33; i < 127; i++ {
		buf.WriteByte(byte(i))
	}
	l := buf.Len()
	n, _ := buf.Write(buf.Bytes())
	if l != n || buf.Len() != 2*l {
		panic("buf error")
	}
	var msg bytes.Buffer
	for i := 0; i < 127-33; i++ {
		msg.Write(buf.Bytes()[i : i+72])
		msg.WriteByte('\n')
	}
	message = msg.Bytes()
	if len(message) != 94*73 {
		panic("short message")
	}
	repeatReader = NewRepeatReader(message)
}

func chargen(c net.Conn, total *int64) {
	defer c.Close()
	for {
		n, err := c.Write(message)
		atomic.AddInt64(total, int64(n))
		if err != nil {
			log.Println("chargen:", err.Error())
			break
		}
		if n != len(message) {
			log.Println("chargen: short write", n, "out of", len(message))
		}
	}
	printConn(c, "chargen", "DOWN")
}

func (s *ChargenServer) ServeWithMeter() error {
	defer s.listener.Close()

	ticker := time.NewTicker(time.Second)
	defer ticker.Stop()
	go func() {
		start := time.Now()
		for t := range ticker.C {
			transferred := atomic.SwapInt64(&s.total, int64(0))
			elapsed := t.Sub(start).Seconds()
			fmt.Printf("%.3f MiB/s\n", float64(transferred)/elapsed/(1024.0*1024.0))
			start = t
		}
	}()

	for {
		conn, err := s.listener.Accept()
		if err != nil {
			return err
		}
		printConn(conn, "chargen", "UP")
		go chargen(conn, &s.total)
	}
}

/////////////////// RepeatReader

type RepeatReader struct {
	message []byte
}

func NewRepeatReader(m []byte) *RepeatReader {
	r := new(RepeatReader)
	r.message = m
	return r
}

func (r *RepeatReader) Read(p []byte) (n int, err error) {
	if len(p) < len(r.message) {
		panic("Short read")
	}
	copy(p, r.message)
	return len(r.message), nil
}

func chargenAdv(c net.Conn) {
	defer c.Close()
	total, err := io.Copy(c, repeatReader)
	if err != nil {
		log.Println("chargen:", err.Error())
	}
	log.Println("total", total)
	printConn(c, "chargen", "DOWN")
}

func (s *ChargenServer) Serve() error {
	return serveTcp(s.listener, chargenAdv, "chargen")
}
