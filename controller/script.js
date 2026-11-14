const DEFAULT_DEVICE_ADDRESS = '192.168.4.1';
let deviceUrl = `http://${DEFAULT_DEVICE_ADDRESS}`;
let currentState = 'CLOSED';
let currentTargetOpen = false;
let isMoving = false;
let isRefreshing = false;

const byId = (id) => document.getElementById(id);

function setConnectionStatus(connected, message) {
  const badge = byId('connectionBadge');
  badge.classList.toggle('online', connected);
  badge.classList.toggle('offline', !connected);
  badge.textContent = connected ? '● Connected' : `● ${message || 'Disconnected'}`;
}

function updateModeControls() {
  const manualMode = byId('manualOverride').checked;
  byId('autoDayNight').disabled = manualMode;
  byId('autoRain').disabled = manualMode;
}

function getDeviceUrl() {
  const value = byId('deviceAddress').value.trim();
  if (!value) {
    throw new Error('Enter the ESP32 IP address first.');
  }

  const url = new URL(value.includes('://') ? value : `http://${value}`);
  if (url.protocol !== 'http:' || url.pathname !== '/' || url.search || url.hash) {
    throw new Error('Enter the ESP32 IP address only (for example: 192.168.1.50).');
  }

  deviceUrl = url.origin;
  return deviceUrl;
}

async function requestDevice(path) {
  if (!deviceUrl) {
    getDeviceUrl();
  }

  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), 3000);
  try {
    const response = await fetch(`${deviceUrl}${path}`, {
      method: 'GET',
      signal: controller.signal
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || `ESP32 returned HTTP ${response.status}`);
    }
    return data;
  } finally {
    clearTimeout(timeoutId);
  }
}

function showDeviceState(data) {
  currentState = data.state;
  currentTargetOpen = data.targetOpen;
  isMoving = data.moving;

  const position = data.state === 'OPEN' ? 100 : 0;
  byId('windowState').textContent = isMoving
    ? (currentTargetOpen ? 'OPENING...' : 'CLOSING...')
    : currentState;
  byId('windowPercent').textContent = `${position}% Open (estimated)`;
  byId('progressBar').style.width = `${position}%`;

  byId('autoDayNight').checked = data.autoDayNight;
  byId('autoRain').checked = data.autoRain;
  byId('manualOverride').checked = data.manualOverride;
  updateModeControls();

  const manualButton = byId('manualToggleBtn');
  const protectedByRain = data.rainProtection;
  manualButton.disabled = !data.manualOverride || isMoving || protectedByRain;
  manualButton.textContent = currentTargetOpen || currentState === 'OPEN'
    ? 'CLOSE WINDOW'
    : 'OPEN WINDOW';
  manualButton.classList.toggle('btn-close-mode', currentTargetOpen || currentState === 'OPEN');

  byId('valLux').textContent = `${data.light} ADC`;
  byId('valRainSensor').textContent = data.rainWet ? 'WET' : 'DRY';
}

async function refreshDeviceState() {
  if (!deviceUrl || isRefreshing) return;

  isRefreshing = true;
  try {
    const data = await requestDevice('/api/state');
    showDeviceState(data);
    setConnectionStatus(true);
  } catch (error) {
    setConnectionStatus(false, 'Connection failed');
    console.error('ESP32 connection error:', error);
  } finally {
    isRefreshing = false;
  }
}

async function connectToDevice() {
  try {
    getDeviceUrl();
    await refreshDeviceState();
  } catch (error) {
    setConnectionStatus(false, error.message);
  }
}

async function updateModes() {
  updateModeControls();
  const params = new URLSearchParams({
    dayNight: byId('autoDayNight').checked ? '1' : '0',
    rain: byId('autoRain').checked ? '1' : '0',
    manual: byId('manualOverride').checked ? '1' : '0'
  });

  try {
    await requestDevice(`/api/mode?${params.toString()}`);
    await refreshDeviceState();
  } catch (error) {
    setConnectionStatus(false, 'Mode update failed');
    console.error('ESP32 mode update error:', error);
    await refreshDeviceState();
  }
}

async function toggleWindow() {
  if (isMoving || !byId('manualOverride').checked) return;

  const action = currentTargetOpen || currentState === 'OPEN' ? 'close' : 'open';
  try {
    await requestDevice(`/api/control?action=${action}`);
    await refreshDeviceState();
  } catch (error) {
    setConnectionStatus(false, 'Command failed');
    console.error('ESP32 window command error:', error);
  }
}

function updateLocation(userCity) {
  const locationBadge = byId('valLocation');
  if (locationBadge) {
    locationBadge.textContent = `📍 ${userCity}`;
  }
}

window.addEventListener('DOMContentLoaded', () => {
  byId('deviceAddress').value = DEFAULT_DEVICE_ADDRESS;
  byId('connectBtn').addEventListener('click', connectToDevice);
  byId('manualToggleBtn').addEventListener('click', toggleWindow);
  byId('autoDayNight').addEventListener('change', updateModes);
  byId('autoRain').addEventListener('change', updateModes);
  byId('manualOverride').addEventListener('change', updateModes);
  byId('deviceAddress').addEventListener('keydown', (event) => {
    if (event.key === 'Enter') connectToDevice();
  });

  updateLocation('Tarlac City');
  updateModeControls();
  setConnectionStatus(false, 'Connecting');
  refreshDeviceState();
  fetchLiveWeather();
  setInterval(fetchLiveWeather, 300000);
  setInterval(refreshDeviceState, 2000);
});
