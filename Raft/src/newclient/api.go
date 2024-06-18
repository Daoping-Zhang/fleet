package main

import (
	"embed"
	"encoding/json"
	"io/fs"
	"log/slog"
	"net/http"

	"github.com/rs/cors"
)

//go:embed frontend/dist/*
var FS embed.FS

func serve() {
	staticFiles, _ := fs.Sub(FS, "frontend/dist")
	fileserver := http.FileServer(http.FS(staticFiles))
	mux := http.NewServeMux()
	mux.Handle("/", fileserver)
	mux.HandleFunc("/api/hosts", func(w http.ResponseWriter, r *http.Request) {
		switch r.Method {
		case http.MethodGet:
			getNodes(w, r)
		default:
			http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		}
	})
	mux.HandleFunc("/api/tests", func(w http.ResponseWriter, r *http.Request) {
		switch r.Method {
		case http.MethodGet:
			getTests(w, r)
		default:
			http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		}
	})
	mux.HandleFunc("/api/fleet-leader", func(w http.ResponseWriter, r *http.Request) {
		switch r.Method {
		case http.MethodPost:
			setFleetLeader(w, r)
		default:
			http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		}
	})
	mux.HandleFunc("/api/run-test", func(w http.ResponseWriter, r *http.Request) {
		switch r.Method {
		case http.MethodGet:
			runTest(w, r)
		default:
			http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		}
	})
	mux.HandleFunc("/api/task-status", func(w http.ResponseWriter, r *http.Request) {
		switch r.Method {
		case http.MethodGet:
			getTaskStatus(w, r)
		default:
			http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		}
	})
	slog.Info("Serving at http://localhost:8080")
	c := cors.New(cors.Options{
		AllowedOrigins:   []string{"http://localhost:5173"},
		AllowedMethods:   []string{"GET", "POST", "PUT", "DELETE", "OPTIONS"},
		AllowedHeaders:   []string{"Authorization", "Content-Type"},
		AllowCredentials: true,
	})
	handler := c.Handler(mux)
	http.ListenAndServe(":8080", handler)
}

func getNodes(w http.ResponseWriter, _ *http.Request) {
	// Return the list of hosts as a JSON array
	w.Header().Set("Content-Type", "application/json")

	nodeLock.RLock()
	json, err := json.Marshal(nodes)
	nodeLock.RUnlock()
	if err != nil {
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		return
	}
	slog.Debug("Get nodes", "nodes", json)
	w.Write(json)
}

func getTests(w http.ResponseWriter, _ *http.Request) {
	// Return the list of tests as a JSON array
	w.Header().Set("Content-Type", "application/json")
	// the names of Tests
	var testNames []string
	if len(Tests) == 0 { // not loaded from file
		readTests()
	}
	for _, test := range Tests {
		testNames = append(testNames, test.Name)
	}
	json, err := json.Marshal(testNames)
	if err != nil {
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		return
	}
	slog.Info("Get tests", "test names", json)
	w.Write(json)
}

// This only sets fleet leader info of client, so as to get further information
func setFleetLeader(w http.ResponseWriter, r *http.Request) {
	var data struct {
		FleetLeaderAddress string `json:"fleetLeaderAddress"`
	}

	err := json.NewDecoder(r.Body).Decode(&data)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		slog.Error("Error decoding request body", "err", err)
		return
	}
	slog.Info("Set fleet leader", "address", data.FleetLeaderAddress)

	// Use a temporary node instance; this will be updated later
	groupLock.RLock()
	fleetLeader = &Node{Address: data.FleetLeaderAddress, IsUp: true}
	groupLock.RUnlock()
	// Do full fleet update
	updateFleet()
	w.WriteHeader(http.StatusOK)
	w.Write([]byte("Fleet leader address updated successfully"))
}

// GET /api/run-test?testName=<testName>
func runTest(w http.ResponseWriter, r *http.Request) {
	testName := r.URL.Query().Get("testName")
	if testName == "" {
		http.Error(w, "Missing test name", http.StatusBadRequest)
		return
	}

	go executeTest(testName)

	w.WriteHeader(http.StatusOK)
	w.Write([]byte("Test executed successfully"))
}

// GET /api/task-status
func getTaskStatus(w http.ResponseWriter, _ *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	TestResultLock.Lock()
	json, err := json.Marshal(TestResult)
	TestResultLock.Unlock()
	if err != nil {
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		return
	}
	slog.Debug("Task status", "status", json)
	w.Write(json)
}
