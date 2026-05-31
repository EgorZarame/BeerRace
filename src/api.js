export const API_URL = 'http://192.168.0.194';

const REQUEST_TIMEOUT = 4000;

async function fetchJson(path, options = {}) {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), REQUEST_TIMEOUT);
  const isGet = !options.method || options.method === 'GET';

  try {
    const response = await fetch(`${API_URL}${path}`, {
      ...options,
      signal: controller.signal,
      headers: isGet
        ? { ...options.headers }
        : { 'Content-Type': 'application/json', ...options.headers },
    });

    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }

    return await response.json();
  } finally {
    clearTimeout(timeoutId);
  }
}

export async function fetchGameData() {
  const weightData = await fetchJson('/weight');
  const stateData = await fetchJson('/state');
  const leaderboardData = await fetchJson('/leaderboard');

  return { weightData, stateData, leaderboardData };
}

export async function getWeight() {
  return fetchJson('/weight');
}

export async function getState() {
  return fetchJson('/state');
}

export async function getLeaderboard() {
  return fetchJson('/leaderboard');
}

export async function postStart() {
  return fetchJson('/api/start', { method: 'POST' });
}

export async function postCup() {
  return fetchJson('/api/cup', { method: 'POST' });
}

export async function postDrink() {
  return fetchJson('/api/drink', { method: 'POST' });
}

export async function postCancel() {
  return fetchJson('/api/cancel', { method: 'POST' });
}
