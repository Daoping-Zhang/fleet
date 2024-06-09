package main

import (
	"bufio"
	"fmt"
	"net"
	"strings"
)

type Command int

const (
	SET Command = iota
	DEL
	GET
)

func serializeSET(key, value string) string {
	// Split the value into words to handle each as a separate bulk string
	valueParts := strings.Split(value, " ")
	// The number of bulk strings is 2 (for SET and key) plus the number of words in value
	numParts := 2 + len(valueParts)
	// Start constructing the RESP message
	message := fmt.Sprintf("*%d\r\n$3\r\nSET\r\n$%d\r\n%s\r\n", numParts, len(key), key)
	// Add each part of the value
	for _, part := range valueParts {
		message += fmt.Sprintf("$%d\r\n%s\r\n", len(part), part)
	}
	return message
}

func serializeDEL(keys string) string {
	// Split the keys into individual parts
	keyParts := strings.Split(keys, " ")
	// The number of bulk strings is 1 (for DEL) plus the number of keys
	numParts := 1 + len(keyParts)
	// Start constructing the RESP message
	message := fmt.Sprintf("*%d\r\n$3\r\nDEL\r\n", numParts)
	// Add each key part
	for _, part := range keyParts {
		message += fmt.Sprintf("$%d\r\n%s\r\n", len(part), part)
	}
	return message
}

func serializeGET(key string) string {
	return fmt.Sprintf("*2\r\n$3\r\nGET\r\n$%d\r\n%s\r\n", len(key), key)
}

func deserialize(response string) string {
	// Simplified deserialization assuming the response is a simple string.
	if strings.HasPrefix(response, "+") {
		return strings.TrimPrefix(response, "+")
	}
	if strings.HasPrefix(response, "-") {
		return strings.TrimPrefix(response, "-")
	}
	if strings.HasPrefix(response, ":") {
		return strings.TrimPrefix(response, ":")
	}
	if strings.HasPrefix(response, "$") {
		parts := strings.SplitN(response, "\r\n", 3)
		return parts[2]
	}
	return response
}

func SendReceive(operation Command, content string, host Host) (response string) {
	addr, err := net.ResolveTCPAddr("tcp", host.Address)
	if err != nil {
		fmt.Println("Error resolving address:", err)
		return
	}
	conn, err := net.DialTCP("tcp", nil, addr)
	if err != nil {
		fmt.Println("Error connecting to server:", err)
		return
	}
	defer conn.Close()

	var message string
	switch operation {
	case SET:
		parts := strings.SplitN(content, " ", 2)
		message = serializeSET(parts[0], parts[1])
	case DEL:
		message = serializeDEL(content)
	case GET:
		message = serializeGET(content)
	default:
		fmt.Println("Unknown command")
		return
	}

	_, err = conn.Write([]byte(message))
	if err != nil {
		fmt.Println("Error sending message:", err)
		return
	}

	respReader := bufio.NewReader(conn)
	resp, err := respReader.ReadString('\n')
	if err != nil {
		fmt.Println("Error reading response:", err)
		return
	}

	response = deserialize(resp)
	return
}
