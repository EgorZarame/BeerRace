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

  const drinkingStartRef = useRef(null);
  const baselineWeightRef = useRef(null);
  const prevStateRef = useRef(0);
  const failCountRef = useRef(0);
  const pollingRef = useRef(false);

  const applyGameData = useCallback((weightData, stateData, leaderboardData) => {
    const state = stateData.state;
    const currentWeight = weightData.weight;
    const espDuration = stateData.duration;
    const espAmount = stateData.amount;

    setWeight(currentWeight);
    setGameState(state);
    setLeaderboard(Array.isArray(leaderboardData) ? leaderboardData : []);

    if (state === 3) {
      if (espDuration != null && espDuration > 0) {
        setTimerMs(espDuration);
      } else if (drinkingStartRef.current) {
        setTimerMs(Date.now() - drinkingStartRef.current);
      } else if (prevStateRef.current !== 3) {
        drinkingStartRef.current = Date.now();
        setTimerMs(0);
      } else {
        setTimerMs(0);
      }

      if (espAmount != null) {
        setDrinkAmount(espAmount);
      } else if (baselineWeightRef.current != null) {
        setDrinkAmount(Math.max(0, baselineWeightRef.current - currentWeight));
      }

      if (prevStateRef.current !== 3) {
        baselineWeightRef.current = currentWeight;
      }
    } else {
      drinkingStartRef.current = null;
      baselineWeightRef.current = null;
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
      if (drinkingStartRef.current) {
        setTimerMs(Date.now() - drinkingStartRef.current);
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
