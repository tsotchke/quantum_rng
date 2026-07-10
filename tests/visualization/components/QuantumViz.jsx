import React, { useState, useEffect } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip } from 'recharts';
import { fetchQuantumData } from '../services/quantumService';

const QuantumVisualizer = () => {
  const [data, setData] = useState([]);
  const [distribution, setDistribution] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  useEffect(() => {
    const loadData = async () => {
      try {
        setLoading(true);
        const numbers = await fetchQuantumData(1000);
        
        const timeSeriesData = numbers.map((val, idx) => ({
          time: idx,
          value: val
        }));

        const buckets = Array(20).fill(0);
        numbers.forEach(n => {
          const bucket = Math.floor(n * 20);
          buckets[bucket]++;
        });

        const distributionData = buckets.map((count, idx) => ({
          range: `${(idx/20).toFixed(2)}-${((idx+1)/20).toFixed(2)}`,
          count
        }));

        setData(timeSeriesData);
        setDistribution(distributionData);
        setError(null);
      } catch (err) {
        setError('Failed to fetch quantum data');
        console.error(err);
      } finally {
        setLoading(false);
      }
    };

    loadData();
    const interval = setInterval(loadData, 5000); // Update every 5 seconds
    return () => clearInterval(interval);
  }, []);

  if (loading) return (
    <div className="flex justify-center items-center h-64">
      <div className="text-lg">Loading quantum data...</div>
    </div>
  );

  if (error) return (
    <div className="flex justify-center items-center h-64">
      <div className="text-red-600 text-lg">{error}</div>
    </div>
  );

  return (
    <div className="p-4">
      <h2 className="text-2xl font-bold mb-4">Quantum RNG Visualization</h2>
      
      <div className="mb-8 bg-white p-4 rounded-lg shadow">
        <h3 className="text-xl mb-2">Time Series Analysis</h3>
        <p className="text-gray-600 mb-4">Real-time quantum random number generation</p>
        <LineChart width={800} height={300} data={data}>
          <CartesianGrid strokeDasharray="3 3" />
          <XAxis dataKey="time" label={{ value: 'Sample', position: 'bottom' }} />
          <YAxis domain={[0, 1]} label={{ value: 'Value', angle: -90, position: 'left' }} />
          <Tooltip />
          <Line 
            type="monotone" 
            dataKey="value" 
            stroke="#8884d8" 
            dot={false} 
            strokeWidth={2}
          />
        </LineChart>
      </div>

      <div className="bg-white p-4 rounded-lg shadow">
        <h3 className="text-xl mb-2">Distribution Analysis</h3>
        <p className="text-gray-600 mb-4">Quantum number distribution across ranges</p>
        <LineChart width={800} height={300} data={distribution}>
          <CartesianGrid strokeDasharray="3 3" />
          <XAxis 
            dataKey="range" 
            label={{ value: 'Value Range', position: 'bottom' }}
            angle={-45}
            textAnchor="end"
            height={80}
          />
          <YAxis label={{ value: 'Frequency', angle: -90, position: 'left' }} />
          <Tooltip />
          <Line 
            type="monotone" 
            dataKey="count" 
            stroke="#82ca9d" 
            strokeWidth={2}
          />
        </LineChart>
      </div>

      <div className="mt-8 grid grid-cols-2 gap-4">
        <div className="bg-white p-4 rounded-lg shadow">
          <h4 className="text-lg font-semibold mb-2">Statistics</h4>
          <div className="space-y-2">
            <p>Total Samples: {data.length}</p>
            <p>Update Interval: 5 seconds</p>
            <p>Bucket Size: {1/20}</p>
          </div>
        </div>
        <div className="bg-white p-4 rounded-lg shadow">
          <h4 className="text-lg font-semibold mb-2">Quality Metrics</h4>
          <div className="space-y-2">
            <p>Mean: {(data.reduce((acc, val) => acc + val.value, 0) / data.length).toFixed(4)}</p>
            <p>Expected: 0.5000</p>
            <p>Deviation: {(Math.abs(0.5 - data.reduce((acc, val) => acc + val.value, 0) / data.length)).toFixed(4)}</p>
          </div>
        </div>
      </div>
    </div>
  );
};

export default QuantumVisualizer;
