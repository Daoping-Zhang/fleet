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
	groupLock.RLock()
	defer groupLock.RUnlock()
	return int(hash % uint64(len(groups)))
}

func SchedSendAndReceive(operation Command, content string) (success bool, msg string) {
	var key, value string
	if operation == SET {
		parts := strings.SplitN(content, " ", 2)
		key = parts[0]
		value = parts[1]
	} else {
		key = content
	}
	groupid := getGroup(key)
	host := getGroupLeader(groupid)

	req := ClientRequest{
		Key:     key,
		Value:   value,
		Method:  operation.String(),
		HashKey: hash(key),
		GroupID: groupid,
	}
	return JsonSendReceive(req, host)
}
