package main

import (
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net"
	"os"

	"github.com/chenshuo/muduo-examples-in-go/muduo"
)

func main() {
	if len(os.Args) != 2 {
		fmt.Printf("Usage: %s host\n", os.Args[0])
		return
	}
	host := os.Args[1]
	conn, err := net.Dial("tcp", net.JoinHostPort(host, "2019"))
	muduo.PanicOnError(err)
	defer conn.Close()

	total, err := io.Copy(ioutil.Discard, conn)
	if err != nil {
		log.Println("discardclient:", err.Error())
	}
	log.Println("total", total)
}
