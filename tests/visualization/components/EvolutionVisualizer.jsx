import React, { useState, useEffect } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from 'recharts';
import { fetchEvolutionData } from '../services/quantumService';

const EvolutionVisualizer = () => {
  const [evolutionData, setEvolutionData] = useState([]);
  const [currentGeneration, setCurrentGeneration] = useState(0);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [isRunning, setIsRunning] = useState(true);
  const [speed, setSpeed] = useState(1000); // Update interval in ms

  useEffect(() => {
    const loadData = async () => {
      try {
        setLoading(true);
        const data = await fetchEvolutionData();
        setEvolutionData(data);
        setError(null);
      } catch (err) {
        setError('Failed to fetch evolution data');
        console.error(err);
      } finally {
        setLoading(false);
      }
    };

    loadData();
    let interval;
    
    if (isRunning) {
      interval = setInterval(() => {
        setCurrentGeneration(g => (g + 1) % evolutionData.length);
      }, speed);
    }

    return () => clearInterval(interval);
  }, [isRunning, speed, evolutionData.length]);

  if (loading) return (
    <div className="flex justify-center items-center h-64">
      <div className="text-lg">Loading evolution data...</div>
    </div>
  );

  if (error) return (
    <div className="flex justify-center items-center h-64">
      <div className="text-red-600 text-lg">{error}</div>
    </div>
  );

  const renderControls = () => (
    <div className="flex items-center space-x-4 mb-4">
      <button
        className={`px-4 py-2 rounded ${isRunning ? 'bg-red-600 text-white' : 'bg-green-600 text-white'}`}
        onClick={() => setIsRunning(!isRunning)}
      >
        {isRunning ? 'Pause' : 'Play'}
      </button>
      <div className="flex items-center space-x-2">
        <span>Speed:</span>
        <select
          className="px-2 py-1 rounded border"
          value={speed}
          onChange={(e) => setSpeed(Number(e.target.value))}
        >
          <option value={2000}>0.5x</option>
          <option value={1000}>1x</option>
          <option value={500}>2x</option>
          <option value={250}>4x</option>
        </select>
      </div>
      <div className="flex items-center space-x-2">
        <span>Generation:</span>
        <input
          type="range"
          min={0}
          max={evolutionData.length - 1}
          value={currentGeneration}
          onChange={(e) => setCurrentGeneration(Number(e.target.value))}
          className="w-48"
        />
        <span>{currentGeneration}</span>
      </div>
    </div>
  );

  return (
    <div className="p-4 space-y-6">
      <div className="flex justify-between items-center">
        <h2 className="text-2xl font-bold">Quantum Evolution Visualization</h2>
        {renderControls()}
      </div>
      
      <div className="bg-white p-4 rounded-lg shadow">
        <h3 className="text-xl mb-2">Fitness Over Time</h3>
        <p className="text-gray-600 mb-4">Evolution progress with quantum mutation</p>
        <ResponsiveContainer width="100%" height={400}>
          <LineChart data={evolutionData}>
            <CartesianGrid strokeDasharray="3 3" />
            <XAxis 
              dataKey="generation" 
              label={{ value: 'Generation', position: 'bottom' }}
            />
            <YAxis domain={[0, 1]} label={{ value: 'Fitness', angle: -90, position: 'left' }} />
            <Tooltip />
            <Legend />
            <Line 
              type="monotone" 
              dataKey="avgFitness" 
              stroke="#8884d8" 
              name="Average Fitness"
              strokeWidth={2}
            />
            <Line 
              type="monotone" 
              dataKey="maxFitness" 
              stroke="#82ca9d" 
              name="Max Fitness"
              strokeWidth={2}
            />
            <Line 
              type="monotone" 
              dataKey="diversity" 
              stroke="#ffc658" 
              name="Population Diversity"
              strokeWidth={2}
            />
          </LineChart>
        </ResponsiveContainer>
      </div>

      <div className="grid grid-cols-3 gap-4">
        <div className="bg-white p-4 rounded-lg shadow">
          <h3 className="text-lg font-semibold mb-2">Current Generation</h3>
          <div className="grid grid-cols-2 gap-2">
            <div className="text-gray-600">Generation:</div>
            <div className="font-bold">{currentGeneration}</div>
            <div className="text-gray-600">Max Fitness:</div>
            <div className="font-bold">
              {evolutionData[currentGeneration]?.maxFitness.toFixed(4)}
            </div>
            <div className="text-gray-600">Avg Fitness:</div>
            <div className="font-bold">
              {evolutionData[currentGeneration]?.avgFitness.toFixed(4)}
            </div>
          </div>
        </div>

        <div className="bg-white p-4 rounded-lg shadow">
          <h3 className="text-lg font-semibold mb-2">Population Stats</h3>
          <div className="grid grid-cols-2 gap-2">
            <div className="text-gray-600">Diversity:</div>
            <div className="font-bold">
              {evolutionData[currentGeneration]?.diversity.toFixed(4)}
            </div>
            <div className="text-gray-600">Improvement:</div>
            <div className="font-bold">
              {currentGeneration > 0 ? 
                ((evolutionData[currentGeneration]?.maxFitness - evolutionData[0]?.maxFitness) * 100).toFixed(2) + '%' 
                : '0.00%'}
            </div>
          </div>
        </div>

        <div className="bg-white p-4 rounded-lg shadow">
          <h3 className="text-lg font-semibold mb-2">Quantum Effects</h3>
          <div className="grid grid-cols-2 gap-2">
            <div className="text-gray-600">Mutation Rate:</div>
            <div className="font-bold">
              {(0.1 * (1.0 - evolutionData[currentGeneration]?.maxFitness)).toFixed(4)}
            </div>
            <div className="text-gray-600">Entropy:</div>
            <div className="font-bold">
              {(evolutionData[currentGeneration]?.diversity * 64).toFixed(4)} bits
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default EvolutionVisualizer;
