let currentState = "CLOSED";
let currentPercent = 0;
let isMoving = false;

function handleModeChange() {
  const dayNightEl = document.getElementById('autoDayNight');
  const rainEl = document.getElementById('autoRain');
  const manualEl = document.getElementById('manualOverride');
  
  const manualToggleBtn = document.getElementById('manualToggleBtn');
  
  if (manualEl.checked) {
    dayNightEl.checked = false;
    rainEl.checked = false;
    dayNightEl.disabled = true;
    rainEl.disabled = true;
    
    manualToggleBtn.disabled = false;
  } else {
    dayNightEl.checked = true;
    rainEl.checked = true;
    dayNightEl.disabled = false;
    rainEl.disabled = false;
    
    manualToggleBtn.disabled = true;
  }
}

// Function na nagpapasya kung bubuksan o isasara base sa current percentage/state
function toggleWindow() {
  if (isMoving) return;
  
  // Kung 0% o nakasara, gawing OPEN; kung may laman o bukas, gawing CLOSE
  if (currentPercent === 0) {
    controlWindow("OPEN");
  } else {
    controlWindow("CLOSE");
  }
}

function controlWindow(action) {
  if (isMoving) return;

  const windowStateEl = document.getElementById("windowState");
  const windowPercentEl = document.getElementById("windowPercent");
  const progressBarEl = document.getElementById("progressBar");
  const manualToggleBtn = document.getElementById('manualToggleBtn');

  if (action === "OPEN") {
    if (currentPercent === 100) return;
    isMoving = true;
    windowStateEl.innerText = "OPENING...";
    if (manualToggleBtn) manualToggleBtn.disabled = true; // Disable habang gumagalaw

    let interval = setInterval(() => {
      if (currentPercent < 100) {
        currentPercent += 20;
        windowPercentEl.innerText = currentPercent + "% Open";
        progressBarEl.style.width = currentPercent + "%";
      } else {
        clearInterval(interval);
        currentState = "OPEN";
        windowStateEl.innerText = "OPEN";
        isMoving = false;
        
        // Pagkatapos bumukas, palitan ang button text maging "CLOSE WINDOW"
        if (manualToggleBtn) {
          manualToggleBtn.disabled = false;
          manualToggleBtn.innerText = "CLOSE WINDOW";
          manualToggleBtn.classList.add("btn-close-mode");
        }
      }
    }, 300);

  } else if (action === "CLOSE") {
    if (currentPercent === 0) return;
    isMoving = true;
    windowStateEl.innerText = "CLOSING...";
    if (manualToggleBtn) manualToggleBtn.disabled = true; // Disable habang gumagalaw

    let interval = setInterval(() => {
      if (currentPercent > 0) {
        currentPercent -= 20;
        windowPercentEl.innerText = currentPercent + "% Open";
        progressBarEl.style.width = currentPercent + "%";
      } else {
        clearInterval(interval);
        currentState = "CLOSED";
        windowStateEl.innerText = "CLOSED";
        isMoving = false;
        
        // Pagkatapos magsara, palitan ang button text maging "OPEN WINDOW"
        if (manualToggleBtn) {
          manualToggleBtn.disabled = false;
          manualToggleBtn.innerText = "OPEN WINDOW";
          manualToggleBtn.classList.remove("btn-close-mode");
        }
      }
    }, 300);
  }
}

function updateLocation(userCity) {
  const locationBadge = document.getElementById('valLocation');
  if (locationBadge) {
    locationBadge.textContent = `📍 ${userCity}`;
  }
}

// Example usage:
updateLocation('Tarlac City');

window.onload = function() {
  document.getElementById('autoDayNight').addEventListener('change', handleModeChange);
  document.getElementById('autoRain').addEventListener('change', handleModeChange);
  document.getElementById('manualOverride').addEventListener('change', handleModeChange);
  handleModeChange();
  
  // Tatawagin nito ang function mula sa weatherApi.js
  fetchLiveWeather();
  setInterval(fetchLiveWeather, 300000); 
};