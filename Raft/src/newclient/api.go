package main

import (
	"embed"
	"encoding/json"
	"io/fs"
	"net/http"
)

//go:embed frontend/dist/*
var FS embed.FS

func serve() {
	staticFiles, _ := fs.Sub(FS, "frontend/dist")
	fileserver := http.FileServer(http.FS(staticFiles))
	mux := http.NewServeMux()
	mux.Handle("/", fileserver)
	mux.HandleFunc("/api/hosts", getHosts)
	mux.HandleFunc("/api/tests", getTests)
	http.ListenAndServe(":8080", mux)
}

func getHosts(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	// Return the list of hosts as a JSON array
	w.Header().Set("Content-Type", "application/json")
	json, err := json.Marshal(Hosts)
	if err != nil {
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		return
	}
	w.Write(json)
}

func getTests(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	// Return the list of tests as a JSON array
	w.Header().Set("Content-Type", "application/json")
	// the names of Tests
	var testNames []string
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
