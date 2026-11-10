function fetchLiveWeather() {
  const apiKey = 'ccd233194e212a4532f441b13a53694f'; // Palitan ng iyong OpenWeatherMap API key

  if (navigator.geolocation) {
    navigator.geolocation.getCurrentPosition(
      (position) => {
        const lat = position.coords.latitude;
        const lon = position.coords.longitude;
        const url = `https://api.openweathermap.org/data/2.5/weather?lat=${lat}&lon=${lon}&units=metric&appid=${apiKey}`;
        callOpenWeatherAPI(url);
      },
      (error) => {
        console.warn('GPS unavailable, using default city:', error.message);
        fallbackOpenWeather(apiKey);
      }
    );
  } else {
    fallbackOpenWeather(apiKey);
  }
}

function callOpenWeatherAPI(url) {
  fetch(url)
    .then(response => {
      if (!response.ok) throw new Error('Failed to fetch weather data');
      return response.json();
    })
    .then(data => {
      const tempC = Math.round(data.main.temp);
      const weatherDesc = data.weather[0].description;
      const humidity = data.main.humidity;

      document.getElementById('valTemp').innerText = `${tempC}°C`;
      document.getElementById('valCondition').innerText = capitalizeWords(weatherDesc);
      document.getElementById('valRain').innerText = `${humidity}%`;
    })
    .catch(error => {
      console.error('OpenWeatherMap Error:', error);
      document.getElementById('valCondition').innerText = 'Offline';
    });
}

function fallbackOpenWeather(apiKey) {
  const defaultCity = 'San Jose';
  const url = `https://api.openweathermap.org/data/2.5/weather?q=${defaultCity}&units=metric&appid=${apiKey}`;
  callOpenWeatherAPI(url);
}

function capitalizeWords(str) {
  return str.replace(/\b\w/g, l => l.toUpperCase());
}