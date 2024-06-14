package main

import (
	"encoding/json"
	"log/slog"
	"net"
	"strconv"
	"strings"
)

type ClientRequest struct {
	Key     string `json:"key"`     // Key(s) of the request, separated by spaces
	Value   string `json:"value"`   // Only valid for SET
	Method  string `json:"method"`  // GET/SET/DEL
	HashKey uint64 `json:"hashKey"` // Hash key for the request
	GroupID int    `json:"groupId"` // Group ID for the request
}

type ClientResponse struct {
	Code    int    `json:"code"`    // Positive for success, negative for failure; for DEL, the number of keys deleted
	Message string `json:"message"` // Response message, only valid for GET
}

// Similar with SendReceive in client.go, but in new JSON format;
// Also, the wrapping of request is done in schedule
func JsonSendReceive(req ClientRequest, host *Node) (success bool, msg string) {
	addr, err := net.ResolveTCPAddr("tcp", host.Address)
	if err != nil {
		slog.Error("Error resolving address: %v", err)
		return false, err.Error()
	}
	conn, err := net.DialTCP("tcp", nil, addr)
	if err != nil {
		slog.Error("Error connecting to server: %v", err)
		return false, err.Error()
	}
	defer conn.Close()

	// Send the request
	reqString, err := json.Marshal(req)
	if err != nil {
		slog.Error("Error marshalling request: %v", err)
		return false, err.Error()
	}
	_, err = conn.Write(reqString)
	if err != nil {
		slog.Error("Error sending message: %v", err)
		return false, err.Error()
	}

	// Receive the response
	respReader := json.NewDecoder(conn)
	var resp ClientResponse
	err = respReader.Decode(&resp)
	if err != nil {
		slog.Error("Error receiving response: %v", err)
		return false, err.Error()
	}

	// automatic handle errors
	if resp.Code <= 0 {
		go func() {
			// just group leader change, no full update
			if strings.HasPrefix(resp.Message, "leader: ") {
				groupLock.Lock()
				defer groupLock.Unlock()
				groupid := req.GroupID
				for i, group := range groups {
					if group.ID == groupid {
						leader := strings.TrimPrefix(resp.Message, "leader: ")
						leaderID, _ := strconv.Atoi(leader) // Hope there would be no error
						groups[i].LeaderID = leaderID
						break
					}
				}
			}
			// other failure, full update
			updateFleet()
		}()
	}
	return resp.Code > 0, resp.Message
}
