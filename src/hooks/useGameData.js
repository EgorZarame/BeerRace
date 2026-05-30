import { useCallback, useEffect, useRef, useState } from 'react';
import { getLeaderboard, getState, getWeight } from '../api';
import { POLL_INTERVAL_MS } from '../constants';

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

  const poll = useCallback(async () => {
    try {
      const [weightData, stateData, leaderboardData] = await Promise.all([
        getWeight(),
        getState(),
        getLeaderboard(),
      ]);

      const state = stateData.state;
      const currentWeight = weightData.weight;

      setConnected(true);
      setWeight(currentWeight);
      setGameState(state);
      setLeaderboard(Array.isArray(leaderboardData) ? leaderboardData : []);

      if (state === 3 && prevStateRef.current !== 3) {
        drinkingStartRef.current = Date.now();
        baselineWeightRef.current = currentWeight;
      }

      if (state === 3 && drinkingStartRef.current) {
        setTimerMs(Date.now() - drinkingStartRef.current);

        if (baselineWeightRef.current != null) {
          setDrinkAmount(Math.max(0, baselineWeightRef.current - currentWeight));
        }
      } else {
        drinkingStartRef.current = null;
        baselineWeightRef.current = null;
        setTimerMs(0);
        setDrinkAmount(0);
      }

      prevStateRef.current = state;
    } catch {
      setConnected(false);
    }
  }, []);

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
