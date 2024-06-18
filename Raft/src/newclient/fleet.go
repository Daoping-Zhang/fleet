package main

import (
	"encoding/json"
	"log/slog"
	"sync"
	"time"
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

var nodes []*Node = []*Node{
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
	1: nodes[0],
	4: nodes[0],
	7: nodes[0],
	2: nodes[1],
	5: nodes[1],
	8: nodes[1],
	3: nodes[2],
	6: nodes[2],
	9: nodes[2],
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

// Used to indicate whether the fleet info needs to be updated
var updateFleetCh chan bool

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
	// Not running, start goroutine
	if updateFleetCh == nil {
		updateFleetCh = make(chan bool, 100)
		go updateFleetWorker()
	}
	updateFleetCh <- true
}

// Limit updateFleet frequency
func updateFleetWorker() {
	// Leader may be down, so we can choose another target
	updateTarget := fleetLeader
	skipWait := true

	for range updateFleetCh {
		// Update limited to once/3s
		if !skipWait {
			<-time.After(3 * time.Second)
			skipWait = false
		}

		// Cannot use SchedSendReceive here, fleet info has not been updated yet
		req := ClientRequest{Key: "fleet_info", Method: GET.String()}
		groupLock.RLock()
		ok, msg := JsonSendReceive(req, updateTarget)
		groupLock.RUnlock()
		if !ok {
			updateTarget = getRandAliveNode()
			skipWait = true // Changed target node, no need to wait
			slog.Warn("Getting fleet info failed, changing target", "msg", msg, `newTarget`, updateTarget.Address)
			continue
		}
		fleetMsg := parseUpdateFleetResponse(msg)
		if fleetMsg == nil {
			slog.Error("Error parsing fleet info", "msg", msg)
			continue
		}

		// Update nodes
		nodeLock.Lock()
		currentNodesMap := make(map[string]*Node)
		for _, node := range nodes {
			node.IsUp = false
			currentNodesMap[node.Address] = node
		}

		for _, node := range fleetMsg.Nodes {
			if currnode, exists := currentNodesMap[node.IP]; exists {
				// only nodes reported by backend are online
				currnode.IsUp = true
			} else {
				// If node does not exist locally, add it
				newNode := Node{Address: node.IP, IsUp: true}
				currentNodesMap[node.IP] = &newNode
			}
		}

		// Rebuild the nodes slice
		nodes = []*Node{}
		nodeID = make(map[int]*Node)
		for _, nodeMsg := range fleetMsg.Nodes {
			node := currentNodesMap[nodeMsg.IP]
			nodes = append(nodes, node)
			for _, id := range nodeMsg.ID {
				nodeID[id] = node
			}
		}
		nodeLock.Unlock()

		// Update groups
		groupLock.Lock()
		groups = []Group{}
		for _, group := range fleetMsg.Groups {
			if len(group.Nodes) == 0 {
				continue
			}
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

	updateFleetCh = nil
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
			return node
		}
	}
	slog.Warn("No node is alive")
	return nil
}

func getFirstDeadNode() *Node {
	nodeLock.RLock()
	defer nodeLock.RUnlock()
	for _, node := range nodes {
		if !node.IsUp {
			return node
		}
	}
	slog.Warn("No node is dead")
	return nil
}

func getRandAliveNode() *Node {
	for _, node := range nodeID {
		if node.IsUp {
			return node
		}
	}
	slog.Warn("No node is alive")
	return nil
}
