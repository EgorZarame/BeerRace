import { formatWeight } from '../constants';

export default function WeightCard({ weight }) {
  return (
    <section className="glass-card weight-card">
      <p className="card-label">Current Weight</p>
      <div className="weight-card__value">
        <span className="weight-card__number">{formatWeight(weight)}</span>
        <span className="weight-card__unit">g</span>
      </div>
      <div className="weight-card__glow" aria-hidden="true" />
    </section>
  );
}
