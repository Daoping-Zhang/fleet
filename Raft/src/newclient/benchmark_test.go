package main

import "testing"

func TestReadTests(t *testing.T) {
	readTests()
	if len(Tests) == 0 {
		t.Errorf("Expected non-empty Tests, got %v", Tests)
	} else {
		t.Logf("Tests: %v", Tests)
	}
}
