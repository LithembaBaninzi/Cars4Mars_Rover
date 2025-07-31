// RC Car Controller JavaScript

// Get the video element
const video = document.getElementById("cameraFeed");
const toggleBtn = document.getElementById("toggleCamera");
let stream = null;

// Function to start webcam
function startCamera() {
  if (navigator.mediaDevices && navigator.mediaDevices.getUserMedia) {
    navigator.mediaDevices
      .getUserMedia({
        video: {
          width: { ideal: 400 },
          height: { ideal: 250 },
          facingMode: "user", // Ensures front camera on mobile
        },
      })
      .then(function (s) {
        stream = s;
        video.srcObject = stream;
        toggleBtn.textContent = "Turn Off";
        toggleBtn.classList.add("active");
      })
      .catch(function (error) {
        console.error("Camera error:", error);
        showCameraPlaceholder();
      });
  } else {
    showCameraPlaceholder();
  }
}

function stopCamera() {
  if (stream) {
    stream.getTracks().forEach((track) => track.stop());
    video.srcObject = null;
    toggleBtn.textContent = "Turn On";
    toggleBtn.classList.remove("active");
    showCameraPlaceholder();
  }
}

function showCameraPlaceholder() {
  video.poster =
    "data:image/svg+xml,%3Csvg width='400' height='300' xmlns='http://www.w3.org/2000/svg'%3E%3Crect width='100%25' height='100%25' fill='%23f0f0f0'/%3E%3Ctext x='50%25' y='50%25' font-family='Arial' font-size='16' fill='%23666' text-anchor='middle' dy='.3em'%3%3C/text%3E%3C/svg%3E";
}

// Toggle functionality for turn-on button
toggleBtn.addEventListener("click", function () {
  if (video.srcObject) {
    stopCamera();
  } else {
    startCamera();
  }
});

// Start with camera off
showCameraPlaceholder();

//Toggle functionality for expand button
document.querySelector(".expand-btn").addEventListener("click", function () {
  video.requestFullscreen().catch((err) => {
    console.error("Error attempting fullscreen:", err);
  });
});

class RCCarController {
  constructor() {
    this.currentDirection = "Stopped";
    this.currentSpeed = 50;
    this.sensorData = {
      front: 27,
      left: 16,
      right: 33,
      back: 33,
    };

    this.initializeElements();
    this.setupEventListeners();
    this.initializeSensors();
    this.startSensorSimulation();
  }

  initializeElements() {
    // Movement Controls
    this.moveButtons = {
      up: document.getElementById("moveUp"),
      down: document.getElementById("moveDown"),
      left: document.getElementById("moveLeft"),
      right: document.getElementById("moveRight"),
      stop: document.getElementById("stopBtn"),
    };

    this.directionStatus = document.getElementById("directionStatus");

    // Speed Controls
    this.speedSlider = document.getElementById("speedSlider");
    this.speedValue = document.getElementById("speedValue");
    this.presetButtons = document.querySelectorAll(".preset-btn");

    // Sensor elements
    this.sensors = {
      front: document.getElementById("sensorFront"),
      left: document.getElementById("sensorLeft"),
      right: document.getElementById("sensorRight"),
      back: document.getElementById("sensorBack"),
    };

    this.distances = {
      front: document.getElementById("frontDistance"),
      left: document.getElementById("leftDistance"),
      right: document.getElementById("rightDistance"),
      back: document.getElementById("backDistance"),
    };

    // Tab buttons
    this.tabButtons = document.querySelectorAll(".tab-btn");
  }

  setupEventListeners() {
    // Movement button handlers
    Object.keys(this.moveButtons).forEach((direction) => {
      this.moveButtons[direction].addEventListener("mousedown", (e) => {
        this.handleMovement(direction, e);
      });

      // Add keyboard support
      this.moveButtons[direction].addEventListener("keydown", (e) => {
        if (e.key === "Enter" || e.key === " ") {
          e.preventDefault();
          this.handleMovement(direction);
        }
      });
    });

    // Speed control handlers
    let speedDebounce;
    this.speedSlider.addEventListener("input", (e) => {
      clearTimeout(speedDebounce);
      speedDebounce = setTimeout(() => {
        const speed = parseInt(e.target.value);
        this.updateSpeed(speed);
      }, 100); // Send only after 100ms of inactivity
    });

    this.presetButtons.forEach((btn) => {
      btn.addEventListener("click", () => {
        const speed = parseInt(btn.dataset.speed);
        this.updateSpeed(speed);
        this.speedSlider.value = speed;
      });
    });

    // Tab functionality
    this.tabButtons.forEach((btn) => {
      btn.addEventListener("click", () => {
        this.switchTab(btn);
      });
    });

    // Keyboard controls
    document.addEventListener("keydown", (e) => {
      this.handleKeyboardInput(e);
    });

    // Prevent default drag behavior
    document.addEventListener("dragstart", (e) => {
      e.preventDefault();
    });
  }

  handleMovement(direction, event) {
    if (event) event.preventDefault(); // 👈 stops page navigation
    // Check if movement is blocked by obstacles
    if (this.isMovementBlocked(direction)) {
      this.showObstacleWarning(direction);
      return;
    }

    if (direction === "stop") {
      this.currentDirection = "Stopped";
      this.resetButtonStates();
    } else {
      this.currentDirection =
        direction.charAt(0).toUpperCase() + direction.slice(1);
      this.resetButtonStates();
      this.moveButtons[direction].classList.add("active");
    }

    this.directionStatus.textContent = this.currentDirection;

    // Send movement command to ESP32
    fetch(`/${direction}`, { method: "GET" })
      .then((response) => response.text())
      .then((data) => console.log("ESP32 Response:", data))
      .catch((error) => console.error("Command failed:", error));

    this.logMovement(direction);
  }

  isMovementBlocked(direction) {
    const dangerThreshold = 10;

    switch (direction) {
      case "up":
        return this.sensorData.front <= dangerThreshold;
      case "down":
        return this.sensorData.back <= dangerThreshold;
      case "left":
        return this.sensorData.left <= dangerThreshold;
      case "right":
        return this.sensorData.right <= dangerThreshold;
      default:
        return false;
    }
  }

  showObstacleWarning(direction) {
    const warningElement = document.createElement("div");
    warningElement.className = "obstacle-warning";
    warningElement.innerHTML = `⚠️ Cannot move ${direction}: Obstacle detected!`;
    warningElement.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            background: #dc3545;
            color: white;
            padding: 12px 20px;
            border-radius: 8px;
            font-weight: 500;
            z-index: 1000;
            animation: slideIn 0.3s ease-out;
        `;

    document.body.appendChild(warningElement);

    setTimeout(() => {
      warningElement.remove();
    }, 3000);
  }

  resetButtonStates() {
    Object.values(this.moveButtons).forEach((btn) => {
      btn.classList.remove("active");
    });
  }

  updateSpeed(speed) {
    this.currentSpeed = speed;
    this.speedValue.textContent = speed + "%";
    this.updatePresetButtons(speed);
    this.logSpeedChange(speed);

    // Create the URL with the speed parameter
    const url = `/speed?value=${speed}`;

    // Debug output
    console.log("Sending speed:", speed, "URL:", url);

    // Send the request to ESP32
    fetch(url)
      .then((response) => {
        if (!response.ok) {
          throw new Error("Network response was not ok");
        }
        return response.text();
      })
      .then((data) => {
        console.log("Server response:", data);
      })
      .catch((error) => {
        console.error("Error:", error);
      });
  }

  updatePresetButtons(currentSpeed) {
    this.presetButtons.forEach((btn) => {
      btn.classList.remove("active");
      if (parseInt(btn.dataset.speed) === currentSpeed) {
        btn.classList.add("active");
      }
    });
  }

  switchTab(activeTab) {
    this.tabButtons.forEach((tab) => tab.classList.remove("active"));
    activeTab.classList.add("active");
    console.log(`Switched to tab: ${activeTab.textContent}`);
  }

  handleKeyboardInput(e) {
    // Prevent default behavior for arrow keys
    if (
      ["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "Space"].includes(
        e.code
      )
    ) {
      e.preventDefault();
    }

    switch (e.code) {
      case "ArrowUp":
      case "KeyW":
        this.handleMovement("up");
        break;
      case "ArrowDown":
      case "KeyS":
        this.handleMovement("down");
        break;
      case "ArrowLeft":
      case "KeyA":
        this.handleMovement("left");
        break;
      case "ArrowRight":
      case "KeyD":
        this.handleMovement("right");
        break;
      case "Space":
      case "KeyX":
        this.handleMovement("stop");
        break;
    }
  }

  // Sensor Management
  initializeSensors() {
    Object.keys(this.sensorData).forEach((sensor) => {
      this.updateSensorDisplay(sensor, this.sensorData[sensor]);
    });
  }

  updateSensorDisplay(sensorName, distance) {
    const sensorElement = this.sensors[sensorName];
    const distanceElement = this.distances[sensorName];

    if (!sensorElement || !distanceElement) return;

    distanceElement.textContent = distance + "cm";
    this.updateSensorColor(sensorElement, distance);

    // Store the updated value
    this.sensorData[sensorName] = distance;

    // Check for auto-stop conditions
    this.checkAutoStop();
  }

  updateSensorColor(sensorElement, distance) {
    sensorElement.classList.remove("safe", "warning", "danger");

    if (distance <= 20) {
      sensorElement.classList.add("danger");
    } else if (distance <= 30) {
      sensorElement.classList.add("warning");
    } else {
      sensorElement.classList.add("safe");
    }
  }

  checkAutoStop() {
    const dangerThreshold = 5;
    const hasDangerousObstacle = Object.values(this.sensorData).some(
      (distance) => distance <= dangerThreshold
    );

    if (hasDangerousObstacle && this.currentDirection !== "Stopped") {
      // Auto-stop due to obstacle
      this.currentDirection = "Stopped";
      this.resetButtonStates();
      this.directionStatus.textContent = "Stopped";
      this.showAutoStopMessage();
    }
  }

  showAutoStopMessage() {
    const messageElement = document.createElement("div");
    messageElement.className = "auto-stop-message";
    messageElement.innerHTML = `🛑 Auto-stop activated: Obstacle too close!`;
    messageElement.style.cssText = `
            position: fixed;
            top: 70px;
            right: 20px;
            background: #ffc107;
            color: #212529;
            padding: 12px 20px;
            border-radius: 8px;
            font-weight: 500;
            z-index: 1000;
            animation: slideIn 0.3s ease-out;
        `;

    document.body.appendChild(messageElement);

    setTimeout(() => {
      messageElement.remove();
    }, 4000);
  }

  startSensorSimulation() {
    // Simulate realistic sensor data changes
    setInterval(() => {
      Object.keys(this.sensorData).forEach((sensor) => {
        // Generate more realistic sensor readings
        const currentDistance = this.sensorData[sensor];
        const change = (Math.random() - 0.5) * 10; // Random change between -5 and +5
        let newDistance = Math.max(10, Math.min(50, currentDistance + change));

        // Occasionally simulate obstacles
        if (Math.random() < 0.1) {
          // 10% chance
          newDistance =
            Math.random() < 0.5
              ? Math.floor(Math.random() * 15) + 8 // Close obstacle (8-22cm)
              : Math.floor(Math.random() * 20) + 30; // Far obstacle (30-49cm)
        }

        this.updateSensorDisplay(sensor, Math.round(newDistance));
      });
    }, 2000); // Update every 2 seconds
  }

  // Logging functions
  logMovement(direction) {
    console.log(
      `Movement: ${direction.toUpperCase()} at ${this.currentSpeed}% speed`
    );
  }

  logSpeedChange(speed) {
    console.log(`Speed updated to: ${speed}%`);
  }

  // Utility functions
  getBatteryLevel() {
    // Simulate battery level decrease
    const baseLevel = 68;
    const variation = Math.floor(Math.random() * 5);
    return Math.max(10, baseLevel - variation);
  }

  getSignalStrength() {
    // Simulate signal strength variations
    const baseStrength = -42;
    const variation = Math.floor(Math.random() * 10) - 5;
    return baseStrength + variation;
  }

  updateSystemStats() {
    // Update battery and signal strength periodically
    setInterval(() => {
      fetch("/status")
        .then((res) => res.json())
        .then((status) => {
          const batteryElement = document.querySelector(".battery .runtime");
          const signalValueEl = document.querySelector(
            ".signal-strength .signal"
          );
          const signalLabelEl = document.querySelector(
            ".signal-strength .signal.strength"
          );

          if (batteryElement) {
            batteryElement.textContent = `🔋 ${
              status.uptime ? 100 - Math.floor(status.uptime / 60) : 68
            }%`;
          }

          if (signalValueEl && signalLabelEl) {
            const rssi = status.signal;
            signalValueEl.textContent = `${rssi} dB`;

            let label = "Unknown";
            let color = "#6c757d"; // default gray

            if (rssi >= -50) {
              label = "Excellent";
              color = "#28a745"; // green
            } else if (rssi >= -60) {
              label = "Good";
              color = "#ffc107"; // yellow
            } else if (rssi >= -70) {
              label = "Fair";
              color = "#fd7e14"; // orange
            } else if (rssi >= -80) {
              label = "Weak";
              color = "#dc3545"; // red
            } else {
              label = "No Signal";
              color = "#6c757d"; // gray
            }

            signalLabelEl.textContent = label;
            signalLabelEl.style.color = color;
          }
        })
        .catch((err) => console.error("Failed to update system stats:", err));
    }, 5000); // Update every 5 seconds
  }

  // Public API methods
  emergencyStop() {
    this.handleMovement("stop");
    console.log("Emergency stop activated!");
  }

  setSpeed(speed) {
    if (speed >= 0 && speed <= 100) {
      this.speedSlider.value = speed;
      this.updateSpeed(speed);
    }
  }

  getSensorData() {
    return { ...this.sensorData };
  }

  getSystemStatus() {
    return fetch("/status")
      .then((res) => res.json())
      .catch((err) => {
        console.error("Failed to fetch status:", err);
        return null;
      });
  }
}

// Initialize the RC Car Controller when the page loads
document.addEventListener("DOMContentLoaded", () => {
  // Add CSS animations
  const style = document.createElement("style");
  style.textContent = `
        @keyframes slideIn {
            from {
                transform: translateX(100%);
                opacity: 0;
            }
            to {
                transform: translateX(0);
                opacity: 1;
            }
        }
        
        .obstacle-warning, .auto-stop-message {
            animation: slideIn 0.3s ease-out;
        }
        
        .d-btn:active {
            transform: translateY(0px) scale(0.95);
        }
        
        .sensor.danger {
            animation: dangerPulse 0.8s infinite;
        }
        
        @keyframes dangerPulse {
            0%, 100% { 
                background-color: #dc3545; 
                transform: scale(1);
            }
            50% { 
                background-color: #ff6b7a; 
                transform: scale(1.05);
            }
        }
    `;
  document.head.appendChild(style);

  // Initialize the controller
  window.rcController = new RCCarController();

  // Add global keyboard shortcut info
  console.log("RC Car Controller initialized!");
  console.log("Keyboard shortcuts:");
  console.log("  Arrow keys or WASD: Move");
  console.log("  Space or X: Stop");
  console.log("Available methods:");
  console.log("  rcController.emergencyStop()");
  console.log("  rcController.setSpeed(0-100)");
  console.log("  rcController.getSensorData()");
  console.log("  rcController.getSystemStatus()");
});
