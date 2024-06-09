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
