package main

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"os"
	"time"
)

type TestAction struct {
	Time   int
	Action string
	Repeat int
}

type Test struct {
	Name    string
	Actions []TestAction // Actions should be ascending by time
}

type TestResult struct {
	Action  Command
	Success bool
	Latency time.Duration
	Bytes   int // Bytes sent during the operation, raw text instead of real message
}

var Tests []Test = []Test{}

// The keys that have been written to the fleet
var keys = map[string]bool{}

// Goroutines for processing requests
var currWorkers = 0

// Read test cases from file
func readTests() {
	data, err := os.ReadFile("tests.json")
	if err != nil {
		panic(err)
	}

	err = json.Unmarshal(data, &Tests)
	if err != nil {
		panic(err)
	}
}

func findTest(testName string) *Test {
	for _, test := range Tests {
		if test.Name == testName {
			return &test
		}
	}
	return nil
}

func executeTest(testName string) {
	test := findTest(testName)
	if test == nil { // Test not found
		return
	}

	// Get total job count, and create channels
	totalJobs := 0
	for _, action := range test.Actions {
		totalJobs += action.Repeat
	}
	jobs := make(chan Command, totalJobs)
	results := make(chan TestResult, totalJobs)

	// create min(100, totalJobs) workers
	for i := 0; i < min(totalJobs, 100); i++ {
		go testWorker(jobs, results)
	}

	for _, action := range test.Actions {
		for i := 0; i < action.Repeat; i++ {
			jobs <- CommandFromString(action.Action)
		}
	}

	for i := 0; i < totalJobs; i++ {
		_ = <-results
		// Do something with the result
	}

}

func testWorker(jobs <-chan Command, results chan<- TestResult) {
	for job := range jobs {
		result := TestResult{Action: job}
		start := time.Now()
		switch job {
		case SET:
			key := GenerateRandomKey("test", 10)
			value := GenerateRandomValue(10)
			resp := SchedSendAndReceive(SET, key+" "+value)
			// TODO: check success
			if resp == "success" {
				result.Success = true
				keys[key] = true
			}
			result.Bytes = len(key) + len(value) + 1
		case DEL:
			key := getRandExistingKey()
			resp := SchedSendAndReceive(DEL, key)
			if resp=="success" {
				result.Success = true
				keys[key] = false
			}
		case GET:
			key := getRandExistingKey()
			resp := SchedSendAndReceive(GET, key)
			if resp=="success" {
				result.Success = true
			}
		}
		result.Latency = time.Since(start)
		results <- result
	}
}

// GenerateRandomKey generates a random key with a given prefix and length.
func GenerateRandomKey(prefix string, length int) string {
	b := make([]byte, length)
	rand.Read(b)
	return fmt.Sprintf("%s-%s", prefix, hex.EncodeToString(b))
}

// GenerateRandomValue generates a random value with a specified length.
func GenerateRandomValue(length int) string {
	b := make([]byte, length)
	rand.Read(b)
	return hex.EncodeToString(b)
}

func getRandExistingKey() string {
	for k, v := range keys {
		if v == true {
			return k
		}
	}
	return ""
}
