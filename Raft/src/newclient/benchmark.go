package main

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"log/slog"
	"os"
	"sync"
	"time"
)

type TestAction struct {
	Time   int // in microseconds
	Action string
	Repeat int
}

type Test struct {
	Name    string
	Actions []TestAction // Actions should be ascending by time
}

// the result of a single test action
type TestActionResult struct {
	Action  Command
	Success bool
	Latency time.Duration
	Bytes   int // Bytes sent during the operation, raw text instead of real message
}

// the result of a test (all actions)
var TestResult struct {
	TotalBytes   int  `json:"totalBytes"`
	SuccessJobs  int  `json:"successJobs"`
	CompleteJobs int  `json:"completeJobs"`
	TotalJobs    int  `json:"totalJobs"`
	Completed    bool `json:"completed"`
}

var TestResultLock = &sync.RWMutex{}

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

	// Clear job info
	TestResultLock.Lock()
	TestResult.TotalJobs = 0
	TestResult.CompleteJobs = 0
	TestResult.SuccessJobs = 0
	TestResult.TotalBytes = 0
	TestResult.Completed = false
	TestResultLock.Unlock()

	// Get total job count, and create channels
	for _, action := range test.Actions {
		TestResult.TotalJobs += action.Repeat
	}
	jobs := make(chan Command, TestResult.TotalJobs)
	results := make(chan TestActionResult, TestResult.TotalJobs)

	// create min(100, totalJobs) workers
	for currWorkers < min(TestResult.TotalJobs, 100) {
		go testWorker(jobs, results)
		currWorkers++
	}

	// add jobs to queue
	start := time.Now()
	for _, action := range test.Actions {
		elapsed := time.Since(start)
		if elapsed < time.Microsecond*time.Duration(action.Time) {
			time.Sleep(time.Microsecond*time.Duration(action.Time) - elapsed)
		}
		for i := 0; i < action.Repeat; i++ {
			jobs <- CommandFromString(action.Action)
		}
	}

	// read results
	for i := 0; i < TestResult.TotalJobs; i++ {
		result := <-results
		TestResultLock.Lock()
		TestResult.TotalBytes += result.Bytes
		TestResult.CompleteJobs++
		if result.Success {
			TestResult.SuccessJobs++
		}
		TestResultLock.Unlock()
	}
	TestResultLock.Lock()
	TestResult.Completed = true
	TestResultLock.Unlock()
}

func testWorker(jobs <-chan Command, results chan<- TestActionResult) {
	for job := range jobs {
		result := TestActionResult{Action: job}
		start := time.Now()
	JOBSWITCH:
		switch job {
		case SET:
			key := GenerateRandomKey("test", 10)
			value := GenerateRandomValue(10)
			ok, _ := SchedSendReceive(SET, key+" "+value)
			if ok {
				result.Success = true
				keys[key] = true
			}
			slog.Info("SET", `key`, key, `value`, value, `ok`, ok)
			result.Bytes = len(key) + len(value) + 1
		case DEL:
			key := getRandExistingKey()
			ok, _ := SchedSendReceive(DEL, key)
			if ok {
				result.Success = true
				keys[key] = false
				result.Bytes = len(key)
			}
			slog.Info("DEL", `key`, key, `ok`, ok)
		case GET:
			key := getRandExistingKey()
			ok, resp := SchedSendReceive(GET, key)
			if ok {
				result.Success = true
				result.Bytes = len(resp)
			}
			slog.Info("GET", `key`, key, `ok`, ok, `value`, resp)
		case POWEROFF: // POWEROFF and POWERON doesn't follow traditional per-group scheduling
			node := getFirstAliveNode() // TODO: Use random alive node?
			if node == nil {
				slog.Error("No alive node found")
				break JOBSWITCH
			}
			req := ClientRequest{Key: "node_active", Method: DEL.String()}
			ok, _ := JsonSendReceive(req, node)
			if ok {
				result.Success = true
			}
			nodeLock.Lock()
			node.IsUp = false
			nodeLock.Unlock()
			slog.Info("Poweroff command", `node`, node)
		case POWERON:
			node := getFirstDeadNode()
			if node == nil {
				slog.Error("No dead node found")
				break JOBSWITCH
			}
			req := ClientRequest{Key: "node_active", Method: GET.String()}
			ok, _ := JsonSendReceive(req, node)
			if ok {
				result.Success = true
			}
			nodeLock.Lock()
			node.IsUp = true
			nodeLock.Unlock()
			slog.Info("Poweron command", `node`, node)
		}
		result.Latency = time.Since(start)
		results <- result
	}
	currWorkers--
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
		if v {
			return k
		}
	}
	return ""
}
