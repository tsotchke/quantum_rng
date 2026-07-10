// Quantum RNG data service
const BACKEND_URL = 'http://localhost:3001';

export const fetchQuantumData = async (samples = 1000) => {
    const response = await fetch(`${BACKEND_URL}/quantum/random?samples=${samples}`);
    const data = await response.json();
    return data.numbers;
};

export const fetchTestResults = async () => {
    const response = await fetch(`${BACKEND_URL}/quantum/tests`);
    return await response.json();
};

export const fetchEvolutionData = async () => {
    const response = await fetch(`${BACKEND_URL}/quantum/evolution`);
    return await response.json();
};

export const fetchStatistics = async () => {
    const response = await fetch(`${BACKEND_URL}/quantum/stats`);
    return await response.json();
};
