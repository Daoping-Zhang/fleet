package main

import "hash/fnv"

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
