import { formatDuration } from '../constants';

export default function Leaderboard({ entries }) {
  const sorted = [...entries].sort((a, b) => a.duration - b.duration);

  return (
    <section className="glass-card leaderboard">
      <p className="card-label">Leaderboard</p>

      {sorted.length === 0 ? (
        <p className="leaderboard__empty">No results yet</p>
      ) : (
        <div className="leaderboard__table-wrap">
          <table className="leaderboard__table">
            <thead>
              <tr>
                <th>#</th>
                <th>Player</th>
                <th>Time (s)</th>
                <th>Amount (g)</th>
              </tr>
            </thead>
            <tbody>
              {sorted.map((entry, index) => (
                <tr key={`${entry.username}-${entry.duration}-${index}`}>
                  <td>
                    <span className={`rank-badge rank-${index + 1}`}>
                      {index + 1}
                    </span>
                  </td>
                  <td className="leaderboard__player">{entry.username}</td>
                  <td>{formatDuration(entry.duration)}</td>
                  <td>{Math.round(entry.amount)}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </section>
  );
}
