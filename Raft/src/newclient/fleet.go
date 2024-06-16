package main

import (
	"encoding/json"
	"log/slog"
	"sync"
)

type Node struct {
	Address string
	IsUp    bool
}

type Group struct {
	ID       int
	LeaderID int
	Nodes    []int
}

type fleetUpdateMsg struct {
	Nodes []struct {
		ID []int  `json:"id"`
		IP string `json:"ip"` // actually IP:port
	} `json:"nodes"`
	FleetLeader int `json:"fleetLeader"`
	Groups      []struct {
		ID     int   `json:"id"`
		Leader int   `json:"leader"`
		Nodes  []int `json:"nodes"`
	} `json:"groups"`
}

var nodes []Node = []Node{
	{
		Address: "192.168.0.1:1001",
		IsUp:    true,
	},
	{
		Address: "192.168.0.2:1001",
		IsUp:    true,
	},
	{
		Address: "192.168.0.3:1001",
		IsUp:    true,
	},
}

// Find the node with the given id.
// A node may have multiple IDs
var nodeID = map[int]*Node{
	1: &nodes[0],
	4: &nodes[0],
	7: &nodes[0],
	2: &nodes[1],
	5: &nodes[1],
	8: &nodes[1],
	3: &nodes[2],
	6: &nodes[2],
	9: &nodes[2],
}

var groups []Group = []Group{
	{
		ID:       1,
		LeaderID: 1,
		Nodes:    []int{1, 2},
	},
	{
		ID:       2,
		LeaderID: 4,
		Nodes:    []int{4, 3},
	},
	{
		ID:       3,
		LeaderID: 5,
		Nodes:    []int{5, 6},
	},
}

var fleetLeader *Node

// Lock nodes and nodeID
var nodeLock = sync.RWMutex{}

// Lock groups and fleetLeader
var groupLock = sync.RWMutex{}

func getGroupLeader(id int) *Node {
	groupLock.RLock()
	for _, group := range groups {
		if group.ID == id {
			leader := group.ID
			nodeLock.RLock()
			defer nodeLock.RUnlock()
			return nodeID[leader]
		}
	}
	groupLock.RUnlock()
	slog.Warn("No group leader found", `id`, id)
	return nil
}

func updateFleet() {
	// Cannot use SchedSendReceive here, fleet info has not been updated yet
	req := ClientRequest{Key: "fleet_info", Method: GET.String()}
	ok, msg := JsonSendReceive(req, fleetLeader)
	if !ok {
		slog.Error("Error getting fleet info", "msg", msg)
		return
	}
	fleetMsg := parseUpdateFleetResponse(msg)
	if fleetMsg == nil {
		slog.Error("Error parsing fleet info", "msg", msg)
		return
	}

	// Update nodes
	nodeLock.Lock()
	currentNodesMap := make(map[string]*Node)
	for _, node := range nodes {
		currentNodesMap[node.Address] = &node
	}

	for _, node := range fleetMsg.Nodes {
		if _, exists := currentNodesMap[node.IP]; !exists {
			// If node exists locally, respect local status
			// If node does not exist locally, add it
			newNode := Node{Address: node.IP, IsUp: true}
			currentNodesMap[node.IP] = &newNode
		}
	}

	// Rebuild the nodes slice
	nodes = []Node{}
	nodeID = make(map[int]*Node)
	for _, nodeMsg := range fleetMsg.Nodes {
		node := currentNodesMap[nodeMsg.IP]
		nodes = append(nodes, *node)
		for _, id := range nodeMsg.ID {
			nodeID[id] = node
		}
	}
	nodeLock.Unlock()

	// Update groups
	groupLock.Lock()
	groups = []Group{}
	for _, group := range fleetMsg.Groups {
		newGroup := Group{
			ID:       group.ID,
			LeaderID: group.Leader,
			Nodes:    group.Nodes,
		}
		groups = append(groups, newGroup)
	}
	nodeLock.RLock()
	fleetLeader = nodeID[fleetMsg.FleetLeader]
	nodeLock.RUnlock()
	groupLock.Unlock()
}

func parseUpdateFleetResponse(resp string) *fleetUpdateMsg {
	var fleetMsg fleetUpdateMsg
	err := json.Unmarshal([]byte(resp), &fleetMsg)
	if err != nil {
		slog.Error("Error unmarshalling fleet info", `msg`, err, `resp`, resp)
		return nil
	}
	return &fleetMsg
}

func getFirstAliveNode() *Node {
	nodeLock.RLock()
	defer nodeLock.RUnlock()
	for _, node := range nodes {
		if node.IsUp {
			return &node
		}
	}
	return nil
}

func getFirstDeadNode() *Node {
	nodeLock.RLock()
	defer nodeLock.RUnlock()
	for _, node := range nodes {
		if !node.IsUp {
			return &node
		}
	}
	return nil
}
