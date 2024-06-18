package main

import (
	"testing"
)

func TestParseUpdateFleetResponse(t *testing.T) {
	jsonStr := `{
		"nodes": [
			{
				"id": [1,4,7],
				"ip": "192.168.0.1:1001"
			},
			{
				"id": [2,5,8],
				"ip": "192.168.0.2:1001"
			},
			{
				"id": [3,6,9],
				"ip": "192.168.0.3:1001"
			}
		],
		"fleetLeader": 1,
		"groups": [
			{
				"id": 1,
				"leader": 1,
				"nodes": [1, 2]
			},
			{
				"id": 2,
				"leader": 4,
				"nodes": [4, 3]
			},
			{
				"id": 3,
				"leader": 5,
				"nodes": [5, 6]
			}
		]
	}`
	msg := parseUpdateFleetResponse(jsonStr)
	if len(msg.Nodes) != 3 {
		t.Errorf("Expected 3, got %v", len(msg.Nodes))
	}
	if msg.FleetLeader != 1 {
		t.Errorf("Expected 1, got %v", msg.FleetLeader)
	}
	if len(msg.Groups) != 3 {
		t.Errorf("Expected 3, got %v", len(msg.Groups))
	}
}
