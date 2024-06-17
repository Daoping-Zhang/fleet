package main

import (
	"context"
	"encoding/json"
	"log/slog"
	"net"
	"strconv"
	"strings"
	"time"
)

type ClientRequest struct {
	Key     string `json:"key"`     // Key(s) of the request, separated by spaces
	Value   string `json:"value"`   // Only valid for SET
	Method  string `json:"method"`  // GET/SET/DEL
	HashKey uint64 `json:"hashKey"` // Hash key for the request
	GroupID int    `json:"groupId"` // Group ID for the request
}

type ClientResponse struct {
	Code    int    `json:"code"`  // Positive for success, negative for failure; for DEL, the number of keys deleted
	Message string `json:"value"` // Response message, only valid for GET
}

// Similar with SendReceive in client.go, but in new JSON format;
// Also, the wrapping of request is done in schedule
func JsonSendReceive(req ClientRequest, host *Node) (success bool, msg string) {
	if host == nil {
		slog.Error("Error resolving address", "err", "host is nil")
		return false, "host is nil"
	}
	nodeLock.RLock()
	dialer := net.Dialer{Timeout: time.Second}
	conn, err := dialer.Dial("tcp", host.Address)
	nodeLock.RUnlock()
	if err != nil {
		slog.Error("Error connecting to server, trying to update fleet info", "err", err, `endpoint`, host.Address)
		go updateFleet() // maybe the node is physically down
		return false, err.Error()
	}
	defer conn.Close()

	// Send the request
	reqString, err := json.Marshal(req)
	if err != nil {
		slog.Error("Error marshalling request", "err", err, `endpoint`, host.Address)
		go updateFleet() // maybe the node is physically down
		return false, err.Error()
	}
	_, err = conn.Write(reqString)
	if err != nil {
		slog.Error("Error sending message, trying to update fleet info", "err", err, `endpoint`, host.Address)
		go updateFleet() // maybe the node is physically down
		return false, err.Error()
	}

	// Receive the response
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	respReader := json.NewDecoder(conn)
	var resp ClientResponse
	ch := make(chan error, 1)
	go func() {
		ch <- respReader.Decode(&resp)
	}()
	select {
	case <-ctx.Done():
		slog.Error("Timeout while receiving response, closing connection", `endpoint`, host.Address)
		return false, "timeout while receiving response"
	case err = <-ch:
		if err != nil {
			slog.Error("Error receiving response, trying to update fleet info", "err", err, `endpoint`, host.Address)
			return false, err.Error()
		}
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
