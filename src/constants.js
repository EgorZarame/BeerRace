export const GAME_STATES = {
  0: { label: 'Ready', className: 'state-ready' },
  1: { label: 'Put Cup', className: 'state-cup' },
  2: { label: 'Ready To Start', className: 'state-start' },
  3: { label: 'Drinking', className: 'state-drinking' },
};

export const POLL_INTERVAL_MS = 200;

export function formatDuration(ms) {
  if (ms == null || Number.isNaN(ms)) return '0.00';
  return (ms / 1000).toFixed(2);
}

export function formatWeight(weight) {
  if (weight == null || Number.isNaN(weight)) return '—';
  return Number(weight).toFixed(1);
}
