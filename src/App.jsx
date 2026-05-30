import ConnectionStatus from './components/ConnectionStatus';
import ControlPanel from './components/ControlPanel';
import Leaderboard from './components/Leaderboard';
import WeightCard from './components/WeightCard';
import { GAME_STATES, formatDuration } from './constants';
import { useGameData } from './hooks/useGameData';
import './App.css';

export default function App() {
  const {
    weight,
    gameState,
    leaderboard,
    connected,
    timerMs,
    drinkAmount,
    refresh,
  } = useGameData();

  const stateInfo = GAME_STATES[gameState] ?? {
    label: 'Unknown',
    className: 'state-unknown',
  };

  return (
    <div className="app">
      <div className="app__bg" aria-hidden="true">
        <div className="orb orb-cyan" />
        <div className="orb orb-purple" />
        <div className="orb orb-pink" />
      </div>

      <header className="app__header">
        <div className="app__title-block">
          <h1 className="app__title">Beer Race</h1>
          <p className="app__subtitle">ESP32 Live Arena</p>
        </div>
        <ConnectionStatus connected={connected} />
      </header>

      {!connected && (
        <p className="offline-banner" role="alert">
          Device Offline
        </p>
      )}

      <main className="app__main">
        <div className="dashboard-top">
          <WeightCard weight={weight} />

          <section className={`glass-card status-card ${stateInfo.className}`}>
            <p className="card-label">Game State</p>
            <p className="status-card__state">{stateInfo.label}</p>

            <div className="status-card__metrics">
              <div className="metric">
                <span className="metric__label">Timer</span>
                <span className="metric__value timer-value">
                  {formatDuration(timerMs)}
                  <small>s</small>
                </span>
              </div>
              <div className="metric">
                <span className="metric__label">Drank</span>
                <span className="metric__value amount-value">
                  {Math.round(drinkAmount)}
                  <small>g</small>
                </span>
              </div>
            </div>
          </section>
        </div>

        <ControlPanel
          gameState={gameState}
          connected={connected}
          onAction={refresh}
        />

        <Leaderboard entries={leaderboard} />
      </main>
    </div>
  );
}
