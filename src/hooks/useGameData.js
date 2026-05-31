import { useCallback, useEffect, useRef, useState } from 'react';
import { fetchGameData } from '../api';
import { OFFLINE_AFTER_FAILURES, POLL_INTERVAL_MS } from '../constants';

export function useGameData() {
  const [weight, setWeight] = useState(null);
  const [gameState, setGameState] = useState(0);
  const [leaderboard, setLeaderboard] = useState([]);
  const [connected, setConnected] = useState(false);
  const [timerMs, setTimerMs] = useState(0);
  const [drinkAmount, setDrinkAmount] = useState(0);

  const prevStateRef = useRef(0);
  const failCountRef = useRef(0);
  const pollingRef = useRef(false);
  const espDurationRef = useRef(0);
  const espDurationAtRef = useRef(0);

  const applyGameData = useCallback((weightData, stateData, leaderboardData) => {
    const state = stateData.state;
    const espDuration = stateData.duration ?? 0;
    const espAmount = stateData.amount ?? 0;

    setWeight(weightData.weight);
    setGameState(state);
    setLeaderboard(Array.isArray(leaderboardData) ? leaderboardData : []);

    if (state === 3) {
      if (espDuration > 0) {
        espDurationRef.current = espDuration;
        espDurationAtRef.current = Date.now();
        setTimerMs(espDuration);
      } else {
        espDurationRef.current = 0;
        setTimerMs(0);
      }
      setDrinkAmount(espAmount);
    } else {
      espDurationRef.current = 0;
      setTimerMs(0);
      setDrinkAmount(0);
    }

    prevStateRef.current = state;
  }, []);

  const poll = useCallback(async () => {
    if (pollingRef.current) return;

    pollingRef.current = true;
    try {
      const { weightData, stateData, leaderboardData } = await fetchGameData();

      failCountRef.current = 0;
      setConnected(true);
      applyGameData(weightData, stateData, leaderboardData);
    } catch {
      failCountRef.current += 1;
      if (failCountRef.current >= OFFLINE_AFTER_FAILURES) {
        setConnected(false);
      }
    } finally {
      pollingRef.current = false;
    }
  }, [applyGameData]);

  useEffect(() => {
    poll();
    const intervalId = setInterval(poll, POLL_INTERVAL_MS);
    return () => clearInterval(intervalId);
  }, [poll]);

  useEffect(() => {
    if (gameState !== 3 || !connected) return undefined;

    const tickId = setInterval(() => {
      if (espDurationRef.current > 0) {
        setTimerMs(espDurationRef.current + Date.now() - espDurationAtRef.current);
      }
    }, 50);

    return () => clearInterval(tickId);
  }, [gameState, connected]);

  return {
    weight,
    gameState,
    leaderboard,
    connected,
    timerMs,
    drinkAmount,
    refresh: poll,
  };
}
