package main

import (
	"encoding/binary"
	"flag"
	"fmt"
	"io"
	"net"
	"strconv"
	"time"

	"github.com/chenshuo/muduo-examples-in-go/muduo"
)

type options struct {
	port     int
	length   int
	number   int
	transmit bool
	receive  bool
	nodelay  bool
	host     string
}

var opt options

type SessionMessage struct {
	Number, Length int32
}

func init() {
	flag.IntVar(&opt.port, "p", 5001, "TCP port")
	flag.IntVar(&opt.length, "l", 65536, "Buffer length")
	flag.IntVar(&opt.number, "n", 8192, "Number of buffers")
	flag.BoolVar(&opt.receive, "r", false, "Receive")
	flag.StringVar(&opt.host, "t", "", "Transmit")
	muduo.Check(binary.Size(SessionMessage{}) == 8, "packed struct")
}

func transmit() {
	sessionMessage := SessionMessage{int32(opt.number), int32(opt.length)}
	fmt.Printf("buffer length = %d\nnumber of buffers = %d\n",
		sessionMessage.Length, sessionMessage.Number)
	total_mb := float64(sessionMessage.Number) * float64(sessionMessage.Length) / 1024.0 / 1024.0
	fmt.Printf("%.3f MiB in total\n", total_mb)

	conn, err := net.Dial("tcp", net.JoinHostPort(opt.host, strconv.Itoa(opt.port)))
	muduo.PanicOnError(err)
	// t := conn.(*net.TCPConn)
	// t.SetNoDelay(false)
	defer conn.Close()

	start := time.Now()
	err = binary.Write(conn, binary.BigEndian, &sessionMessage)
	muduo.PanicOnError(err)

	total_len := 4 + opt.length // binary.Size(int32(0)) == 4
	// println(total_len)
	payload := make([]byte, total_len)
	binary.BigEndian.PutUint32(payload, uint32(opt.length))
	for i := 0; i < opt.length; i++ {
		payload[4+i] = "0123456789ABCDEF"[i%16]
	}

	for i := 0; i < opt.number; i++ {
		var n int
		n, err = conn.Write(payload)
		muduo.PanicOnError(err)
		muduo.Check(n == len(payload), "write payload")

		var ack int32
		err = binary.Read(conn, binary.BigEndian, &ack)
		muduo.PanicOnError(err)
		muduo.Check(ack == int32(opt.length), "ack")
	}

	elapsed := time.Since(start).Seconds()
	fmt.Printf("%.3f seconds\n%.3f MiB/s\n", elapsed, total_mb/elapsed)
}

func receive() {
	listener := muduo.ListenTcpOrDie(fmt.Sprintf(":%d", opt.port))
	defer listener.Close()
	println("Accepting", listener.Addr().String())
	conn, err := listener.Accept()
	muduo.PanicOnError(err)
	defer conn.Close()

	// Read header
	var sessionMessage SessionMessage
	err = binary.Read(conn, binary.BigEndian, &sessionMessage)
	muduo.PanicOnError(err)

	fmt.Printf("receive buffer length = %d\nreceive number of buffers = %d\n",
		sessionMessage.Length, sessionMessage.Number)
	total_mb := float64(sessionMessage.Number) * float64(sessionMessage.Length) / 1024.0 / 1024.0
	fmt.Printf("%.3f MiB in total\n", total_mb)

	payload := make([]byte, sessionMessage.Length)
	start := time.Now()
	for i := 0; i < int(sessionMessage.Number); i++ {
		var length int32
		err = binary.Read(conn, binary.BigEndian, &length)
		muduo.PanicOnError(err)
		muduo.Check(length == sessionMessage.Length, "read length")

		var n int
		n, err = io.ReadFull(conn, payload)
		muduo.PanicOnError(err)
		muduo.Check(n == len(payload), "read payload")

		// ack
		err = binary.Write(conn, binary.BigEndian, &length)
		muduo.PanicOnError(err)
	}

	elapsed := time.Since(start).Seconds()
	fmt.Printf("%.3f seconds\n%.3f MiB/s\n", elapsed, total_mb/elapsed)
}

func main() {
	flag.Parse()
	opt.transmit = opt.host != ""
	if opt.transmit == opt.receive {
		println("Either -r or -t must be specified.")
		return
	}

	if opt.transmit {
		transmit()
	} else if opt.receive {
		receive()
	} else {
		panic("unknown")
	}
}
