package main

type Node struct {
	Address string
	IsUp    bool
}

type Group struct {
	ID       int
	LeaderID int
	Nodes    []int
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

func getGroupLeader(id int) *Node {
	for _, group := range groups {
		if group.ID == id {
			leader := group.ID
			return nodeID[leader]
		}
	}
	return nil
}
