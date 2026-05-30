export const GAME_STATES = {
  0: { label: 'Ready', className: 'state-ready', hint: 'Start → поставьте пустую кружку' },
  1: { label: 'Put Empty Cup', className: 'state-cup', hint: 'Пустая кружка на весах → Cup Placed' },
  2: { label: 'Pour Drink', className: 'state-start', hint: 'Налейте напиток → когда готовы, DRINK!' },
  3: { label: 'Drinking', className: 'state-drinking', hint: 'Таймер с −50 г. Финиш: пустая кружка ≤20 г' },
};

export const POLL_INTERVAL_MS = 500;
export const OFFLINE_AFTER_FAILURES = 5;

export function formatDuration(ms) {
  if (ms == null || Number.isNaN(ms)) return '0.00';
  return (ms / 1000).toFixed(2);
}

export function formatWeight(weight) {
  if (weight == null || Number.isNaN(weight)) return '—';
  return Number(weight).toFixed(1);
}
