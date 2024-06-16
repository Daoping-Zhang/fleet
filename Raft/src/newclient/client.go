package main

import (
	"bufio"
	"bytes"
	"fmt"
	"net"
	"strconv"
	"strings"
)

type Command int

const (
	SET Command = iota
	DEL
	GET
	POWERON  // Not really a command, but via DEL
	POWEROFF // Not really a command, but via GET
)

func (c Command) String() string {
	return [...]string{"SET", "DEL", "GET", "POWERON", "POWEROFF"}[c]
}

func CommandFromString(s string) Command {
	switch strings.ToUpper(s) {
	case "SET":
		return SET
	case "DEL":
		return DEL
	case "GET":
		return GET
	case "POWERON":
		return POWERON
	case "POWEROFF":
		return POWEROFF
	default:
		return -1
	}
}

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

func SendReceive(operation Command, content string, host Node) (response string) {
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
	response, err = respReader.ReadString('\n')
	if err != nil {
		fmt.Println("Error reading response:", err)
		return
	}

	return
}

func DecodeResp(rawResp string) (bool, string) {
	reader := bufio.NewReader(bytes.NewReader([]byte(rawResp)))
	line, err := reader.ReadString('\n')
	if err != nil {
		return false, fmt.Sprintf("Error reading response: %v", err)
	}
	line = strings.TrimSuffix(line, "\r\n")

	switch line[0] {
	case '+': // Simple Strings
		return true, line[1:]
	case '-': // Errors
		return false, line[1:]
	case ':': // Integers
		return true, line[1:]
	case '$': // Bulk Strings
		length, err := strconv.Atoi(line[1:])
		if err != nil {
			return false, fmt.Sprintf("Error parsing bulk string length: %v", err)
		}
		if length == -1 {
			return false, "Null Bulk String"
		}
		buf := make([]byte, length+2)
		_, err = reader.Read(buf)
		if err != nil {
			return false, fmt.Sprintf("Error reading bulk string: %v", err)
		}
		return true, string(buf[:length])
	case '*': // Arrays
		length, err := strconv.Atoi(line[1:])
		if err != nil {
			return false, fmt.Sprintf("Error parsing array length: %v", err)
		}
		var array []string
		for i := 0; i < length; i++ {
			partType, err := reader.ReadString('\n')
			if err != nil {
				return false, fmt.Sprintf("Error reading array element: %v", err)
			}
			partType = strings.TrimSuffix(partType, "\r\n")

			if partType[0] != '$' {
				return false, fmt.Sprintf("Unexpected array element type: %c", partType[0])
			}

			partLength, err := strconv.Atoi(partType[1:])
			if err != nil {
				return false, fmt.Sprintf("Error parsing bulk string length in array: %v", err)
			}

			buf := make([]byte, partLength+2)
			_, err = reader.Read(buf)
			if err != nil {
				return false, fmt.Sprintf("Error reading bulk string in array: %v", err)
			}
			array = append(array, string(buf[:partLength]))
		}
		return true, strings.Join(array, " ")
	default:
		return false, fmt.Sprintf("Unknown message type: %c", line[0])
	}
}
