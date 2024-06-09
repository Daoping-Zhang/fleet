package main

import (
	"embed"
	"encoding/json"
	"io/fs"
	"log"
	"net/http"
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
			getHosts(w, r)
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
	http.ListenAndServe(":8080", mux)
}

func getHosts(w http.ResponseWriter, _ *http.Request) {
	// Return the list of hosts as a JSON array
	w.Header().Set("Content-Type", "application/json")
	json, err := json.Marshal(Hosts)
	if err != nil {
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		return
	}
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
	w.Write(json)
}

func setFleetLeader(w http.ResponseWriter, r *http.Request) {
	var data struct {
		FleetLeaderAddress string `json:"fleetLeaderAddress"`
	}

	err := json.NewDecoder(r.Body).Decode(&data)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		log.Println("Error decoding request body:", err)
		return
	}

	fleetLeaderAddress = data.FleetLeaderAddress
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

	executeTest(testName)

	w.WriteHeader(http.StatusOK)
	w.Write([]byte("Test executed successfully"))
}
