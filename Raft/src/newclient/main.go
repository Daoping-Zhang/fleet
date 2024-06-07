package main

import (
	"fmt"
	"net"
	"strings"
)

type Host struct {
	Address       net.Addr
	IsFleetLeader bool
	IsRaftLeader  bool
	GroupID       int
	LogicalID     int
}

func main() {
	println("请输入目标IP地址：")
	ip := ""
	fmt.Scanln(&ip)
	println("请输入目标端口号：")
	port := ""
	fmt.Scanln(&port)
	host := Host{}
	var err error
	host.Address, err = net.ResolveTCPAddr("tcp", ip+":"+port)
	if err != nil {
		println("解析地址失败")
		return
	}
	println("请输入命令（set/del/get）：")
	command := ""
	fmt.Scanln(&command)

	opStr := strings.Split(command, " ")[0]
	var operation Command
	switch opStr {
	case "set":
		operation = SET
	case "del":
		operation = DEL
	case "get":
		operation = GET
	default:
		println("无效命令，请重新输入")
		return
	}
	// 从 command 中除去 operation
	command = strings.TrimPrefix(command, opStr+" ")
	resp := SendReceive(operation, command, host)
	println("返回结果：")
	println(resp)
}
