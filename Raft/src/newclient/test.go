package main

type TestAction struct {
	Time   int
	Host   string
	Action string
}

type Test struct {
	Name    string
	Actions []TestAction
}

var Tests []Test = []Test{
	{
		Name:    "test1",
		Actions: []TestAction{},
	},
	{
		Name:    "test2",
		Actions: []TestAction{},
	},
}
