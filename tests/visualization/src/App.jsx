import React from 'react';
import { BrowserRouter, Routes, Route, Link } from 'react-router-dom';
import QuantumVisualizer from './components/QuantumVis';
import TestVisualizer from './components/TestVisualizer';
import EvolutionVisualizer from './components/EvolutionVisualizer';

const App = () => {
  return (
    <BrowserRouter>
      <div className="min-h-screen bg-gray-100">
        <nav className="bg-white shadow-lg">
          <div className="max-w-7xl mx-auto px-4">
            <div className="flex justify-between h-16">
              <div className="flex space-x-8">
                <Link to="/" className="flex items-center text-gray-900 hover:text-blue-600">
                  Quantum RNG Dashboard
                </Link>
                <Link to="/tests" className="flex items-center text-gray-600 hover:text-blue-600">
                  Statistical Tests
                </Link>
                <Link to="/evolution" className="flex items-center text-gray-600 hover:text-blue-600">
                  Evolution Demo
                </Link>
              </div>
            </div>
          </div>
        </nav>

        <main className="max-w-7xl mx-auto py-6 sm:px-6 lg:px-8">
          <Routes>
            <Route path="/" element={<QuantumVisualizer />} />
            <Route path="/tests" element={<TestVisualizer />} />
            <Route path="/evolution" element={<EvolutionVisualizer />} />
          </Routes>
        </main>
      </div>
    </BrowserRouter>
  );
};

export default App;