package main

import (
	"encoding/json"
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
	Bytes   int
}

var Tests []Test = []Test{}

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
			// Do something
		case DEL:
			// Do something
		case GET:
			// Do something

		}
		result.Latency = time.Since(start)
		results <- result
	}
}
