
// Initialize sliders
const leftSlider = document.getElementById("leftSlider");
const rightSlider = document.getElementById("rightSlider");
const stopBtn = document.getElementById("stopBtn");

// Track last values to prevent flooding
let lastLeftValue = 0;
let lastRightValue = 0;
let lastSendTime = 0;



let updateTimeout;
let pendingLeftValue = 0;
let pendingRightValue = 0;

function updateMotor(side, value) {
  // Constrain value to -100 to 100 range
  value = Math.max(-100, Math.min(100, parseInt(value)));
  
  // Store the latest values
  if (side === "L") pendingLeftValue = value;
  if (side === "R") pendingRightValue = value;
  
  // Debounce the HTTP request
  clearTimeout(updateTimeout);
  updateTimeout = setTimeout(sendMotorCommand, 50); // 50ms delay
  
  displaySliderValue(value, side === "L" ? "leftSliderValue" : "rightSliderValue");
}

function sendMotorCommand() {
  fetch(`/motors?left=${pendingLeftValue}&right=${pendingRightValue}`)
    .then(response => response.text())
    .then(data => console.log(data))
    .catch(err => console.error('Error:', err));

  console.log(`Motors: L${pendingLeftValue}% R${pendingRightValue}%`);
}

function displaySliderValue(value, elementId) {
  document.getElementById(elementId).textContent = value;
}

function stopMotors() {
  leftSlider.value = 0;
  rightSlider.value = 0;
  displaySliderValue(rightSlider.value, "rightSliderValue");
  displaySliderValue(leftSlider.value, "leftSliderValue");
  
  // Changed from WebSocket to HTTP GET
  fetch('/stop')
    .then(response => response.text())
    .then(data => console.log(data))
    .catch(err => console.error('Error:', err));
  
  console.log("Motors stopped");
}

// Handle touch events for better mobile control
let isDragging = false;

// Modified slider event listeners
[leftSlider, rightSlider].forEach(slider => {
  // Touch events
  slider.addEventListener('touchstart', () => {
    isDragging = true;
  });
  
  slider.addEventListener('touchend', () => {
    isDragging = false;
  });
  
  slider.addEventListener('touchmove', (e) => {
    if (!isDragging) return;
    e.preventDefault();
    //const touch = e.touches[0];
    const rect = slider.getBoundingClientRect();
    const touchY = Math.max(rect.top, Math.min(rect.bottom, e.touches[0].clientY));
    const percent = (touchY - rect.top) / rect.height;
    const value = Math.round((1 - percent) * 200 - 100); // -100 to 100
    
    // Constrain to slider steps
    const step = 25;
    // Add this clamping:
    const steppedValue = Math.max(-100, Math.min(100, 
    Math.round(value / step) * step
    ));
    slider.value = steppedValue;
    updateMotor(slider.id === "leftSlider" ? "L" : "R", steppedValue);
    displaySliderValue(steppedValue, slider.id + "Value");
  });
  
  // Mouse events (still needed for desktop)
  slider.addEventListener('input', (e) => {
    if (!isDragging) { // Skip if touch event already handled
      updateMotor(e.target.id === "leftSlider" ? "L" : "R", e.target.value);
    }
  });
});

// Prevent default touch behavior
document.addEventListener(
  "touchmove",
  function (e) {
    if (e.target.classList.contains("slider")) {
      e.preventDefault();
    }
  },
  { passive: false }
);
