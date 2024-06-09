package main

import (
	"hash/fnv"
	"strings"
)

// Find the appropriate group to send the request

// hash uses the FNV-1a algorithm to hash a string
func hash(key string) uint64 {
	h := fnv.New64a()
	h.Write([]byte(key))
	return h.Sum64()
}

func getGroup(key string) int {
	hash := hash(key)
	return int(hash % uint64(groupNum))
}

func SchedSendAndReceive(operation Command, content string) (response string) {
	var key string
	if operation == SET {
		key = strings.Split(content, " ")[0]
	} else {
		key = content
	}
	_ = getGroup(key)
	// TODO: get group leader
	host := Hosts[0]
	return SendReceive(operation, content, host)
}
