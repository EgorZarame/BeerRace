import { useState } from 'react';
import { postCancel, postCup, postDrink, postStart } from '../api';

const ACTIONS = [
  {
    id: 'start',
    label: 'Start Game',
    endpoint: postStart,
    states: [0],
    className: 'btn-start',
  },
  {
    id: 'cup',
    label: 'Cup Placed',
    endpoint: postCup,
    states: [1],
    className: 'btn-cup',
  },
  {
    id: 'drink',
    label: 'DRINK!',
    endpoint: postDrink,
    states: [2],
    className: 'btn-drink',
  },
  {
    id: 'cancel',
    label: 'Cancel',
    endpoint: postCancel,
    states: [0, 1, 2, 3],
    className: 'btn-cancel',
  },
];

export default function ControlPanel({ gameState, connected, onAction }) {
  const [loadingId, setLoadingId] = useState(null);

  async function handleClick(action) {
    if (!connected || loadingId) return;

    setLoadingId(action.id);
    try {
      await action.endpoint();
      onAction?.();
    } catch {
      /* offline handled by connection indicator */
    } finally {
      setLoadingId(null);
    }
  }

  return (
    <section className="glass-card control-panel">
      <p className="card-label">Controls</p>
      <div className="control-panel__buttons">
        {ACTIONS.map((action) => {
          const enabled =
            connected && action.states.includes(gameState) && !loadingId;

          return (
            <button
              key={action.id}
              type="button"
              className={`game-btn ${action.className}`}
              disabled={!enabled}
              onClick={() => handleClick(action)}
            >
              {loadingId === action.id ? '…' : action.label}
            </button>
          );
        })}
      </div>
    </section>
  );
}
