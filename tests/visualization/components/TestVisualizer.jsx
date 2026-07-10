import React, { useState, useEffect } from 'react';
import { 
  LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip,
  BarChart, Bar, Legend, ResponsiveContainer
} from 'recharts';
import { fetchTestResults, fetchStatistics } from '../services/quantumService';

const TestVisualizer = () => {
  const [testResults, setTestResults] = useState({
    entropy: [],
    patterns: [],
    performance: []
  });
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [selectedTest, setSelectedTest] = useState('entropy');

  useEffect(() => {
    const loadData = async () => {
      try {
        setLoading(true);
        const [results, stats] = await Promise.all([
          fetchTestResults(),
          fetchStatistics()
        ]);

        setTestResults({
          entropy: results.entropy,
          patterns: results.patterns,
          performance: stats.performance
        });
        setError(null);
      } catch (err) {
        setError('Failed to fetch test results');
        console.error(err);
      } finally {
        setLoading(false);
      }
    };

    loadData();
    const interval = setInterval(loadData, 10000); // Update every 10 seconds
    return () => clearInterval(interval);
  }, []);

  if (loading) return (
    <div className="flex justify-center items-center h-64">
      <div className="text-lg">Loading test results...</div>
    </div>
  );

  if (error) return (
    <div className="flex justify-center items-center h-64">
      <div className="text-red-600 text-lg">{error}</div>
    </div>
  );

  const renderTestSelector = () => (
    <div className="flex space-x-4 mb-4">
      <button
        className={`px-4 py-2 rounded ${selectedTest === 'entropy' ? 'bg-blue-600 text-white' : 'bg-gray-200'}`}
        onClick={() => setSelectedTest('entropy')}
      >
        Entropy Analysis
      </button>
      <button
        className={`px-4 py-2 rounded ${selectedTest === 'patterns' ? 'bg-blue-600 text-white' : 'bg-gray-200'}`}
        onClick={() => setSelectedTest('patterns')}
      >
        Pattern Distribution
      </button>
      <button
        className={`px-4 py-2 rounded ${selectedTest === 'performance' ? 'bg-blue-600 text-white' : 'bg-gray-200'}`}
        onClick={() => setSelectedTest('performance')}
      >
        Performance Metrics
      </button>
    </div>
  );

  const renderEntropyAnalysis = () => (
    <div className="bg-white p-4 rounded-lg shadow">
      <h3 className="text-xl font-bold mb-4">Entropy Analysis</h3>
      <p className="text-gray-600 mb-4">Measuring randomness quality over time</p>
      <ResponsiveContainer width="100%" height={400}>
        <LineChart data={testResults.entropy}>
          <CartesianGrid strokeDasharray="3 3" />
          <XAxis dataKey="sample" label={{ value: 'Sample', position: 'bottom' }} />
          <YAxis domain={[63.995, 64]} label={{ value: 'Entropy (bits)', angle: -90, position: 'left' }} />
          <Tooltip />
          <Line 
            type="monotone" 
            dataKey="entropy" 
            stroke="#8884d8" 
            strokeWidth={2}
            dot={false}
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );

  const renderPatternDistribution = () => (
    <div className="bg-white p-4 rounded-lg shadow">
      <h3 className="text-xl font-bold mb-4">Pattern Distribution</h3>
      <p className="text-gray-600 mb-4">Analyzing bit pattern frequencies</p>
      <ResponsiveContainer width="100%" height={400}>
        <BarChart data={testResults.patterns}>
          <CartesianGrid strokeDasharray="3 3" />
          <XAxis 
            dataKey="pattern" 
            label={{ value: 'Pattern', position: 'bottom' }}
            angle={-45}
            textAnchor="end"
            height={80}
          />
          <YAxis 
            domain={[0.06, 0.065]} 
            label={{ value: 'Frequency', angle: -90, position: 'left' }}
          />
          <Tooltip />
          <Bar dataKey="frequency" fill="#82ca9d" />
        </BarChart>
      </ResponsiveContainer>
    </div>
  );

  const renderPerformanceMetrics = () => (
    <div className="bg-white p-4 rounded-lg shadow">
      <h3 className="text-xl font-bold mb-4">Performance Metrics</h3>
      <p className="text-gray-600 mb-4">Throughput and entropy measurements</p>
      <ResponsiveContainer width="100%" height={400}>
        <LineChart data={testResults.performance}>
          <CartesianGrid strokeDasharray="3 3" />
          <XAxis dataKey="test" label={{ value: 'Test Run', position: 'bottom' }} />
          <YAxis 
            yAxisId="left" 
            domain={[100, 200]} 
            label={{ value: 'Throughput (MB/s)', angle: -90, position: 'left' }}
          />
          <YAxis 
            yAxisId="right" 
            orientation="right" 
            domain={[63.98, 64]} 
            label={{ value: 'Entropy (bits)', angle: 90, position: 'right' }}
          />
          <Tooltip />
          <Legend />
          <Line 
            yAxisId="left" 
            type="monotone" 
            dataKey="throughput" 
            stroke="#8884d8" 
            name="Throughput"
            strokeWidth={2}
          />
          <Line 
            yAxisId="right" 
            type="monotone" 
            dataKey="entropy" 
            stroke="#82ca9d" 
            name="Entropy"
            strokeWidth={2}
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );

  return (
    <div className="p-4 space-y-8">
      {renderTestSelector()}
      
      {selectedTest === 'entropy' && renderEntropyAnalysis()}
      {selectedTest === 'patterns' && renderPatternDistribution()}
      {selectedTest === 'performance' && renderPerformanceMetrics()}

      <div className="grid grid-cols-3 gap-4">
        <div className="bg-white p-4 rounded-lg shadow">
          <h4 className="text-lg font-semibold mb-2">Test Summary</h4>
          <div className="space-y-2">
            <p>Total Tests: {testResults.performance.length}</p>
            <p>Latest Entropy: {testResults.entropy[testResults.entropy.length - 1]?.entropy.toFixed(6)}</p>
            <p>Update Interval: 10s</p>
          </div>
        </div>
        <div className="bg-white p-4 rounded-lg shadow">
          <h4 className="text-lg font-semibold mb-2">Performance Summary</h4>
          <div className="space-y-2">
            <p>Avg Throughput: {(testResults.performance.reduce((acc, val) => acc + val.throughput, 0) / testResults.performance.length).toFixed(2)} MB/s</p>
            <p>Peak Throughput: {Math.max(...testResults.performance.map(p => p.throughput)).toFixed(2)} MB/s</p>
            <p>Min Throughput: {Math.min(...testResults.performance.map(p => p.throughput)).toFixed(2)} MB/s</p>
          </div>
        </div>
        <div className="bg-white p-4 rounded-lg shadow">
          <h4 className="text-lg font-semibold mb-2">Quality Metrics</h4>
          <div className="space-y-2">
            <p>Min Entropy: {Math.min(...testResults.entropy.map(e => e.entropy)).toFixed(6)}</p>
            <p>Max Entropy: {Math.max(...testResults.entropy.map(e => e.entropy)).toFixed(6)}</p>
            <p>Pattern Uniformity: {(1 - Math.max(...testResults.patterns.map(p => p.frequency)) + Math.min(...testResults.patterns.map(p => p.frequency))).toFixed(6)}</p>
          </div>
        </div>
      </div>
    </div>
  );
};

export default TestVisualizer;
