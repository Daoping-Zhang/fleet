<script setup lang="ts">
import { ref, onMounted, onUnmounted, watch, getCurrentInstance, unref } from 'vue'
import { Line } from 'vue-chartjs'
import { Chart as ChartJS, Title, Tooltip, Legend, LineElement, PointElement, LinearScale, CategoryScale } from 'chart.js'
import { NH1, NH2, NIcon, NSelect, NButton, NInput } from 'naive-ui'

const baseURL = import.meta.env.VITE_API_BASE_URL;
const instance = getCurrentInstance();

// Register chart components
ChartJS.register(Title, Tooltip, Legend, LineElement, PointElement, LinearScale, CategoryScale)

interface Node {
  Address: string;
  IsUp: boolean;
}

interface Metrics {
  completionTime: number;
  throughput: number;
  otherData: string;
}

interface TestResult {
  totalBytes: number;
  successJobs: number;
  completeJobs: number;
  totalJobs: number;
  completed: boolean;
  submittedJobs: number;
  totalLatency: number;
}


// Define the nodes array using ref
const nodes = ref<Node[]>([
  {
    Address: '192.168.1.1:1234',
    IsUp: true,
  },
  {
    Address: '192.168.1.2:2432',
    IsUp: false,
  },
  // Add more nodes as needed
]);

// Define the method to get the node color
const getNodeColor = (node: Node): string => {
  return node.IsUp ? 'green' : 'red';
};

// Define options for the dropdown menus
const testCases = ref([
  { label: 'Test Case 1', value: 'TestCase1' },
  { label: 'Test Case 2', value: 'TestCase2' },
  // Add more test cases as needed
]);

const algorithms = ref([
  { label: 'Fleet', value: 'Fleet' },
  { label: 'Raft', value: 'Raft' },
]);

// Define the selected values
const selectedTestCase = ref<string | null>(null);
const selectedAlgorithm = ref<string | null>(null);
const fleetLeaderAddress = ref<string | null>(null);
const nodeFetched = ref<boolean>(false);
const secondElapsed = ref<number>(0);
const completeJobs = ref<number>(0);

const testResult = ref<TestResult>({
  totalBytes: 0,
  successJobs: 0,
  completeJobs: 0,
  totalJobs: 0,
  completed: false,
  submittedJobs: 0,
  totalLatency: 0,
});

const averageDelayData = ref<{ labels: string[], datasets: any[] }>({
  labels: [],
  datasets: [
    {
      label: 'Average Delay (ms)',
      data: [],
      borderColor: 'blue',
      fill: false,
    },
  ],
});

const taskNumData = ref<{ labels: string[], datasets: any[] }>({
  labels: [],
  datasets: [
    {
      label: 'Submitted Tasks',
      data: [],
      borderColor: 'red',
      fill: false,
    },
    {
      label: 'Completed Tasks',
      data: [],
      borderColor: 'green',
      fill: false,
    },
    {
      label: 'Success Tasks',
      data: [],
      borderColor: 'orange',
      fill: false,
    },
  ],
});

const updateCharts = () => {
  const labels = [...averageDelayData.value.labels];
  const lastLabel = labels.length > 0 ? parseInt(labels[labels.length - 1]) : 0;
  labels.push((lastLabel + 1).toString());

  const avgDelayData = [...averageDelayData.value.datasets[0].data];
  const lastSecondLatency = unref(testResult).totalLatency - (avgDelayData.slice(-1)[0] || 0);
  avgDelayData.push(lastSecondLatency / unref(testResult).completeJobs);

  const taskLabels = [...taskNumData.value.labels];
  const submittedTasks = [...taskNumData.value.datasets[0].data];
  const completedTasks = [...taskNumData.value.datasets[1].data];
  const successTasks = [...taskNumData.value.datasets[2].data];

  taskLabels.push((lastLabel + 1).toString());
  submittedTasks.push(unref(testResult).submittedJobs);
  completedTasks.push(unref(testResult).completeJobs);
  successTasks.push(unref(testResult).successJobs);

  averageDelayData.value = {
    labels,
    datasets: [{
      label: 'Average Delay (ms)',
      data: avgDelayData,
      borderColor: 'blue',
      fill: false,
    }]
  };

  taskNumData.value = {
    labels: taskLabels,
    datasets: [
      { label: 'Submitted Tasks', data: submittedTasks, borderColor: 'red', fill: false },
      { label: 'Completed Tasks', data: completedTasks, borderColor: 'green', fill: false },
      { label: 'Success Tasks', data: successTasks, borderColor: 'orange', fill: false },
    ]
  };

};


// Watch for updates in testResult and update charts accordingly
watch(() => testResult.value, (newValue, oldValue) => {
  if (newValue !== oldValue) {
    updateCharts();
  }
}, { deep: true, immediate: true });

const fetchNodeStatus = async () => {
  if (!nodeFetched.value) {
    return; // No need to fetch when fleet leader is not set!
  }
  try {
    const response = await fetch(baseURL + '/api/hosts');
    if (response.ok) {
      const data: Node[] = await response.json();
      nodes.value = data;
    } else {
      console.error('Failed to fetch node status:', response.statusText);
    }
  } catch (error) {
    console.error('Error fetching node status:', error);
  }
  console.log('Fetching node status', nodes.value);
};

// Function to fetch test cases from the API
const fetchTestCases = async () => {
  try {
    const response = await fetch(baseURL + '/api/tests');
    if (response.ok) {
      const data: string[] = await response.json();
      testCases.value = data.map(test => ({ label: test, value: test }));
    } else {
      console.error('Failed to fetch test cases:', response.statusText);
    }
  } catch (error) {
    console.error('Error fetching test cases:', error);
  }
  console.log('Fetching test cases', testCases.value);
};

const setFleetLeaderAddress = async () => {
  try {
    const response = await fetch(baseURL + '/api/fleet-leader', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ fleetLeaderAddress: fleetLeaderAddress.value }),
    });

    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }

    console.log('Fleet leader address set successfully');
    nodeFetched.value = true;
  } catch (error) {
    console.error('Error setting fleet leader address:', error);
  }

  console.log('Setting fleet leader address', fleetLeaderAddress);
};

// Define the method to run the test case
const runTestCase = async () => {
  console.log('Selected Test Case:', selectedTestCase.value);
  console.log('Selected Algorithm:', selectedAlgorithm.value);


  // GET /api/run-test?testName=<testname>
  // Don't care about the return value
  try {
    const response = await fetch(baseURL + `/api/run-test?testName=${selectedTestCase.value}`);
    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }
    console.log('Test case started successfully');
  } catch (error) {
    console.error('Error running test case:', error);
  }
  const startTime = Date.now();
  secondElapsed.value = 0;
  // testResult.value = { totalBytes: 0, successJobs: 0, completeJobs: 0, totalJobs: 0, completed: false, submittedJobs: 0, totalLatency: 0 };

  // After this, run GET /api/task-status every 1s before completed=true
  // Return format: {"totalBytes":0,"successJobs":0,"completeJobs":0,"totalJobs":0,"completed":false}
  const intervalId = setInterval(async () => {
    try {
      const response = await fetch(baseURL + '/api/task-status');
      if (response.ok) {
        const data: TestResult = await response.json();
        testResult.value = { ...data };
        // testResult.value.completeJobs = data.completeJobs;
        // testResult.value.successJobs = data.successJobs;
        // testResult.value.totalJobs = data.totalJobs;
        // testResult.value.completed = data.completed;
        // testResult.value.submittedJobs = data.submittedJobs;
        // testResult.value.totalLatency = data.totalLatency;
        // completeJobs.value = data.completeJobs;
        console.log("testResult", testResult.value);
        instance?.proxy?.$forceUpdate();
        // updateCharts();
        secondElapsed.value = (Date.now() - startTime) / 1000;
        if (data.completed) {
          clearInterval(intervalId);
          console.log('Test case completed:', data);
        }
      } else {
        console.error('Failed to fetch task status:', response.statusText);
      }
    } catch (error) {
      console.error('Error fetching task status:', error);
    }
  }, 1000);
};

// Set up interval to fetch node status every 2 seconds
onMounted(() => {
  fetchNodeStatus();
  const intervalId = setInterval(fetchNodeStatus, 2000);
  onUnmounted(() => {
    clearInterval(intervalId);
  });
});
</script>

<template>
  <header></header>
  <main>
    <div class="container mx-auto my-4">
      <n-h1>Fleet Consensus Algorithm</n-h1>
      <div class="flex items-center space-x-4 sm:w-1/2 md:w-1/3 w-full my-2">
        <n-input v-model:value="fleetLeaderAddress" placeholder="Fleet Leader Address" />
        <n-button @click="setFleetLeaderAddress">Set</n-button>
      </div>
      <n-h2 prefix="bar">Node Status</n-h2>
      <div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-4">
        <div v-for="node in nodes" :key="node.Address" class="flex items-center p-2 border rounded shadow">
          <n-icon :color="getNodeColor(node)" size="24">
            <template v-slot:default>
              <svg viewBox="0 0 24 24" class="w-6 h-6">
                <circle cx="12" cy="12" r="10"></circle>
              </svg>
            </template>
          </n-icon>
          <span class="ml-2">{{ node.Address }}</span>
        </div>
      </div>
      <n-h2 prefix="bar">Run Test Cases</n-h2>
      <div class="mb-4">
        <div class="flex items-center space-x-4 sm:w-1/2 md:w-1/3 w-full my-2">
          <n-select v-model:value="selectedTestCase" :options="testCases" placeholder="Select Test Case" />
          <n-button @click="fetchTestCases">Refresh</n-button>
        </div>
        <n-select v-model="selectedAlgorithm" :options="algorithms" label="Select Algorithm Mode"
          placeholder="Select Algorithm Mode" class="w-full sm:w-1/2 md:w-1/3 mb-4"></n-select>
        <n-button @click="runTestCase" type="primary">Run Test Case</n-button>
      </div>
      <div class="bg-gray-100 p-4 rounded shadow">
        <h3 class="text-xl font-semibold mb-4">Performance Metrics</h3>
        <div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 gap-4">
          <div class="p-2 border rounded">
            <h4 class="text-lg font-medium">Jobs Completed</h4>
            <p>{{ testResult.completeJobs }}/{{ testResult.totalJobs }}</p>
          </div>
          <div class="p-2 border rounded">
            <h4 class="text-lg font-medium">Success Rate</h4>
            <p>{{ testResult.successJobs / testResult.completeJobs }}</p>
          </div>
          <div class="p-2 border rounded">
            <h4 class="text-lg font-medium">Throughput</h4>
            <p>{{ testResult.completeJobs / secondElapsed }} ops/sec</p>
          </div>
        </div>
      </div>
      <div class="mt-4 flex space-x-4">
        <div class="w-1/2 p-2">
          <h3 class="text-xl font-semibold mb-4">Average Delay</h3>
          <Line :data="averageDelayData" />
        </div>
        <div class="w-1/2 p-2">
          <h3 class="text-xl font-semibold mb-4">Task Numbers</h3>
          <Line :data="taskNumData" />
        </div>
      </div>
    </div>
    <p>{{ testResult }}</p>
  </main>
</template>

<style scoped></style>
