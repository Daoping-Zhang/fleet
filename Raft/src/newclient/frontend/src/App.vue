<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'
import { NH1, NH2, NIcon, NSelect, NButton } from 'naive-ui'

interface Host {
  Address: string;
  IsFleetLeader: boolean;
  IsRaftLeader: boolean;
  GroupID: number;
  LogicalID: number;
  IsUp: boolean;
}

interface Metrics {
  completionTime: number;
  throughput: number;
  otherData: string;
}

// Define the nodes array using ref
const nodes = ref<Host[]>([
  {
    Address: '192.168.1.1:1234',
    IsFleetLeader: true,
    IsRaftLeader: false,
    GroupID: 1,
    LogicalID: 1,
    IsUp: true,
  },
  {
    Address: '192.168.1.2:2432',
    IsFleetLeader: false,
    IsRaftLeader: true,
    GroupID: 1,
    LogicalID: 2,
    IsUp: false,
  },
  // Add more nodes as needed
]);

// Define the method to get the node color
const getNodeColor = (node: Host): string => {
  return node.IsUp ? 'green' : 'red';
};

// Define options for the dropdown menus
const testCases = ref([
  { label: 'Test Case 1', value: 'TestCase1' },
  { label: 'Test Case 2', value: 'TestCase2' },
  // Add more test cases as needed
]);

const algorithms = ref([
  { label: 'Algorithm Mode 1', value: 'AlgorithmMode1' },
  { label: 'Algorithm Mode 2', value: 'AlgorithmMode2' },
  // Add more algorithm modes as needed
]);

// Define the selected values
const selectedTestCase = ref<string | null>(null);
const selectedAlgorithm = ref<string | null>(null);

// Define the method to run the test case
const runTestCase = () => {
  console.log('Selected Test Case:', selectedTestCase.value);
  console.log('Selected Algorithm:', selectedAlgorithm.value);
  // Implement the logic to run the test case
};

// Define the metrics
const metrics = ref<Metrics>({
  completionTime: 0,
  throughput: 0,
  otherData: ''
});

// Function to fetch nodes status from the API
const fetchNodeStatus = async () => {
  try {
    const response = await fetch('/api/hosts');
    if (response.ok) {
      const data: Host[] = await response.json();
      nodes.value = data;
    } else {
      console.error('Failed to fetch node status:', response.statusText);
    }
  } catch (error) {
    console.error('Error fetching node status:', error);
  }
  console.log('Fetching node status', nodes.value);
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
  <header>
  </header>
  <main>
    <div class="container mx-auto my-4">
      <n-h1>Fleet Consensus Algorithm</n-h1>
      <n-h2 prefix="bar">Node Status</n-h2>
      <div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-4">
        <div v-for="node in nodes" :key="node.LogicalID" class="flex items-center p-2 border rounded shadow">
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
        <n-select v-model="selectedTestCase" :options="testCases" label="Select Test Case"
          placeholder="Select Test Case" class="w-full sm:w-1/2 md:w-1/3 mb-4"></n-select>
        <n-select v-model="selectedAlgorithm" :options="algorithms" label="Select Algorithm Mode"
          placeholder="Select Algorithm Mode" class="w-full sm:w-1/2 md:w-1/3 mb-4"></n-select>
        <n-button @click="runTestCase" type="primary">Run Test Case</n-button>
      </div>
      <div class="bg-gray-100 p-4 rounded shadow">
        <h3 class="text-xl font-semibold mb-4">Performance Metrics</h3>
        <div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 gap-4">
          <div class="p-2 border rounded">
            <h4 class="text-lg font-medium">Completion Time</h4>
            <p>{{ metrics.completionTime }} seconds</p>
          </div>
          <div class="p-2 border rounded">
            <h4 class="text-lg font-medium">Throughput</h4>
            <p>{{ metrics.throughput }} ops/sec</p>
          </div>
          <div class="p-2 border rounded">
            <h4 class="text-lg font-medium">Other Data</h4>
            <p>{{ metrics.otherData }}</p>
          </div>
        </div>
      </div>
    </div>
  </main>
</template>

<style scoped></style>
