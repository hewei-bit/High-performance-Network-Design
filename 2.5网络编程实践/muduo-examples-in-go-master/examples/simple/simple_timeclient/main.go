package main

import (
	"encoding/binary"
	"fmt"
	"net"
	"os"
	"time"

	"github.com/chenshuo/muduo-examples-in-go/muduo"
)

func main() {
	if len(os.Args) != 2 {
		fmt.Printf("Usage: %s host\n", os.Args[0])
		return
	}
	host := os.Args[1]
	conn, err := net.Dial("tcp", net.JoinHostPort(host, "2037"))
	muduo.PanicOnError(err)
	defer conn.Close()

	var unixtime int32
	err = binary.Read(conn, binary.BigEndian, &unixtime)
	muduo.PanicOnError(err)

	println(time.Unix(int64(unixtime), 0).String())
}
