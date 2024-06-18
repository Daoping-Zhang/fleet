package main

import "testing"

func TestSerializeSet(t *testing.T) {
	result := serializeSET("CS06142", "Cloud Computing")
	if result != "*4\r\n$3\r\nSET\r\n$7\r\nCS06142\r\n$5\r\nCloud\r\n$9\r\nComputing\r\n" {
		t.Errorf("Expected *4\r\n$3\r\nSET\r\n$7\r\nCS06142\r\n$5\r\nCloud\r\n$9\r\nComputing\r\n, got %s", result)
	}
}

func TestSerializeDel(t *testing.T) {
	result := serializeDEL("CS06142 CS162")
	if result != "*3\r\n$3\r\nDEL\r\n$7\r\nCS06142\r\n$5\r\nCS162\r\n" {
		t.Errorf("Expected *3\r\n$3\r\nDEL\r\n$7\r\nCS06142\r\n$5\r\nCS162\r\n, got %s", result)
	}
}

func TestSerializeGet(t *testing.T) {
	result := serializeGET("CS06142")
	if result != "*2\r\n$3\r\nGET\r\n$7\r\nCS06142\r\n" {
		t.Errorf("Expected *2\r\n$3\r\nGET\r\n$7\r\nCS06142\r\n, got %s", result)
	}
}

func TestDeserializeGetReply(t *testing.T) {
	status, value := DecodeResp("*2\r\n$5\r\nCloud\r\n$9\r\nComputing\r\n")
	if status != true {
		t.Errorf("Expected true, got %v", status)
	}
	if value != "Cloud Computing" {
		t.Errorf("Expected Cloud Computing, got %v", value)
	}
}

func TestDeserializeSetReply(t *testing.T) {
	status, value := DecodeResp("+OK\r\n")
	if status != true {
		t.Errorf("Expected true, got %v", status)
	}
	if value != "OK" {
		t.Errorf("Expected OK, got %v", value)
	}
}

func TestDeserializeDelReply(t *testing.T) {
	status, value := DecodeResp(":2\r\n")
	if status != true {
		t.Errorf("Expected true, got %v", status)
	}
	if value != "2" {
		t.Errorf("Expected 2, got %v", value)
	}
}

func TestDeserializeErrorReply(t *testing.T) {
	status, value := DecodeResp("-ERROR\r\n")
	if status != false {
		t.Errorf("Expected false, got %v", status)
	}
	if value != "ERROR" {
		t.Errorf("Expected ERROR, got %v", value)
	}
}
