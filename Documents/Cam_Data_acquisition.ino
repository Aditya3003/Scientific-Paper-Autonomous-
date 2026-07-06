#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// ===== CONFIGURE THESE =====
const char* ap_ssid     = "Cam_data";
const char* ap_password = "12345678";

// ---- Fixed (manual) exposure per resolution ----
// Auto-exposure/auto-gain are DISABLED. The pixel histogram is allowed to
// shift dark as it gets dark outside — that's the experiment.
//
// Why three values? The OV2640 uses DIFFERENT sensor readout modes per
// framesize (subsampling at QVGA, 2x2 binning at SVGA, near-native at XGA).
// Binning combines sensor pixels = more light per output pixel = brighter.
// So one AEC value gives three different brightnesses. Three values lets
// you match daytime brightness across all sizes; the shift then comes
// purely from real ambient light.
//
// Set these ONCE here, before flashing. Never touch in the field.
// Range 0..1200 for AEC, 0..30 for gain.
#define AEC_VALUE_QVGA    100    // smaller framesize = more binning = brighter, so lower AEC
#define AEC_VALUE_SVGA    175
#define AEC_VALUE_XGA     250    // larger framesize = less binning = darker, so higher AEC
#define FIXED_AGC_GAIN      0    // keep at 0 for cleanest histogram (gain adds noise)
// ===========================

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

WebServer server(80);

// ---- Resolution sequence ----
struct ResStep {
  framesize_t res;
  const char* label;
  int         warmup;     // # of dummy frames to flush after switching to this size
  int         settleMs;   // ms to wait after set_framesize before grabbing
  int         aecValue;   // per-resolution exposure (sensor binning differs per size)
};
ResStep steps[] = {
  { FRAMESIZE_QVGA, "320x240",  2, 120, AEC_VALUE_QVGA },
  { FRAMESIZE_SVGA, "800x600",  2, 120, AEC_VALUE_SVGA },
  { FRAMESIZE_XGA,  "1024x768", 2, 150, AEC_VALUE_XGA }
};
const int TOTAL_STEPS = 3;

// ---- 3 frame buffers held simultaneously in PSRAM ----
struct Frame {
  uint8_t* buf;
  size_t   len;
  String   label;   // e.g. "tree_320x240"
  bool     ready;
};
Frame frames[TOTAL_STEPS];

// ---- Capture state ----
volatile bool capturing = false;
String        sessionName = "image";

// ------------------------------------------------------------------
void initCameraOnce() {
  camera_config_t config;
  config.ledc_channel  = LEDC_CHANNEL_0;
  config.ledc_timer    = LEDC_TIMER_0;
  config.pin_d0        = Y2_GPIO_NUM;
  config.pin_d1        = Y3_GPIO_NUM;
  config.pin_d2        = Y4_GPIO_NUM;
  config.pin_d3        = Y5_GPIO_NUM;
  config.pin_d4        = Y6_GPIO_NUM;
  config.pin_d5        = Y7_GPIO_NUM;
  config.pin_d6        = Y8_GPIO_NUM;
  config.pin_d7        = Y9_GPIO_NUM;
  config.pin_xclk      = XCLK_GPIO_NUM;
  config.pin_pclk      = PCLK_GPIO_NUM;
  config.pin_vsync     = VSYNC_GPIO_NUM;
  config.pin_href      = HREF_GPIO_NUM;
  config.pin_sscb_sda  = SIOD_GPIO_NUM;
  config.pin_sscb_scl  = SIOC_GPIO_NUM;
  config.pin_pwdn      = PWDN_GPIO_NUM;
  config.pin_reset     = RESET_GPIO_NUM;
  config.xclk_freq_hz  = 20000000;
  config.pixel_format  = PIXFORMAT_JPEG;
  config.frame_size    = FRAMESIZE_XGA;   // start at largest so buffers are big enough
  config.jpeg_quality  = 4;               // near-raw: minimal JPEG compression
  config.fb_count      = 2;               // double buffer for faster grabs
  config.fb_location   = CAMERA_FB_IN_PSRAM;
  config.grab_mode     = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return;
  }
  applySensorSettings(AEC_VALUE_QVGA);
  esp_camera_sensor_get()->set_framesize(esp_camera_sensor_get(), FRAMESIZE_QVGA);
}

// Apply all manual settings. Called at init AND after every framesize change,
// because set_framesize() resets several registers on the OV2640.
// aecValue is per-resolution to compensate for the sensor's different
// binning/subsampling modes at different framesizes.
void applySensorSettings(int aecValue) {
  sensor_t* s = esp_camera_sensor_get();
  // Exposure: fully locked (this is what makes the histogram shift with light)
  s->set_exposure_ctrl(s, 0);           // DISABLE auto-exposure
  s->set_aec2(s, 0);                    // disable DSP AE
  s->set_gain_ctrl(s, 0);               // DISABLE auto-gain
  s->set_aec_value(s, aecValue);        // per-resolution shutter
  s->set_agc_gain(s, FIXED_AGC_GAIN);   // fixed analog gain
  // White balance: KEEP ON. It doesn't affect overall brightness (so it doesn't
  // disturb the histogram-shift experiment) but it's needed for special_effect=2
  // to properly zero the chroma channels — otherwise images come out tinted,
  // especially at higher resolutions.
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  // No software brightness/contrast bias
  s->set_ae_level(s, 0);
  s->set_brightness(s, 0);
  s->set_contrast(s, 0);
  s->set_saturation(s, -2);             // clamp chroma to zero so greyscale stays clean
                                         // at quality=4 (high-quality JPEG keeps any residual colour)
  // Greyscale is applied LAST so it's the most recent register write before grab.
  s->set_special_effect(s, 2);          // greyscale (sensor-level, not software post)
}

// ------------------------------------------------------------------
// Free any old buffer, allocate new one in PSRAM, copy frame
bool storeFrame(int slot, camera_fb_t* fb, const char* resLabel) {
  if (frames[slot].buf) {
    free(frames[slot].buf);
    frames[slot].buf = nullptr;
  }
  uint8_t* newBuf = (uint8_t*) ps_malloc(fb->len);
  if (!newBuf) {
    // fall back to regular heap
    newBuf = (uint8_t*) malloc(fb->len);
    if (!newBuf) {
      Serial.println("malloc failed!");
      return false;
    }
  }
  memcpy(newBuf, fb->buf, fb->len);
  frames[slot].buf   = newBuf;
  frames[slot].len   = fb->len;
  frames[slot].label = sessionName + "_" + resLabel;
  frames[slot].ready = true;
  return true;
}

// ------------------------------------------------------------------
// Capture all 3 resolutions back-to-back, no artificial delays
void captureAll() {
  sensor_t* s = esp_camera_sensor_get();
  unsigned long t0 = millis();

  for (int i = 0; i < TOTAL_STEPS; i++) {
    s->set_framesize(s, steps[i].res);
    applySensorSettings(steps[i].aecValue);   // re-lock greyscale + per-res exposure
    delay(steps[i].settleMs);

    // Per-resolution dummy frame flush so AE/AGC catch up.
    // XGA needs the most: largest crop = longest convergence.
    for (int k = 0; k < steps[i].warmup; k++) {
      camera_fb_t* dummy = esp_camera_fb_get();
      if (dummy) esp_camera_fb_return(dummy);
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      Serial.printf("fb_get failed at step %d\n", i);
      continue;
    }
    storeFrame(i, fb, steps[i].label);
    esp_camera_fb_return(fb);

    Serial.printf("Step %d (%s): %u bytes\n",
                  i, steps[i].label, (unsigned)frames[i].len);
  }

  Serial.printf("All 3 captured in %lu ms\n", millis() - t0);
}

// ------------------------------------------------------------------
void handleRoot() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32-CAM Fast Capture</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: Arial, sans-serif; background: #f0f2f5;
           display: flex; justify-content: center; padding: 24px 16px; min-height: 100vh; }
    .card { background: #fff; border-radius: 12px; box-shadow: 0 2px 12px rgba(0,0,0,0.1);
            padding: 24px; width: 100%; max-width: 440px; height: fit-content; }
    h2 { font-size: 20px; color: #222; margin-bottom: 20px; }
    label { font-size: 13px; font-weight: bold; color: #555; display: block; margin-bottom: 6px; }
    input[type=text] { width: 100%; padding: 10px 12px; font-size: 15px;
                       border: 1px solid #ddd; border-radius: 8px;
                       background: #fafafa; margin-bottom: 16px; }
    button { width: 100%; padding: 12px; font-size: 15px; font-weight: bold;
             border: none; border-radius: 8px; cursor: pointer; margin-bottom: 10px; }
    #captureBtn { background: #2196F3; color: #fff; }
    #captureBtn:disabled { background: #90caf9; cursor: not-allowed; }
    #statusBox { background: #f8f9fa; border: 1px solid #e0e0e0;
                 border-radius: 8px; padding: 14px; margin-top: 4px; }
    .badge { display: inline-block; padding: 4px 12px; border-radius: 20px;
             font-size: 13px; font-weight: bold; margin-bottom: 8px; }
    .idle    { background: #eee;     color: #666; }
    .running { background: #c8e6c9; color: #2e7d32; }
    .done    { background: #bbdefb; color: #1565c0; }
    .error   { background: #ffcdd2; color: #b71c1c; }
    #log { margin-top: 8px; font-size: 12px; color: #888;
           max-height: 140px; overflow-y: auto; }
    #log div { padding: 2px 0; border-bottom: 1px solid #f0f0f0; }
    .progress-wrap { height: 6px; background: #e0e0e0; border-radius: 3px;
                     margin-top: 10px; display: none; }
    .progress-bar  { height: 6px; background: #2196F3; border-radius: 3px;
                     width: 0%; transition: width 0.3s; }
  </style>
</head>
<body>
<div class="card">
  <h2>ESP32-CAM Fast Capture</h2>

  <label for="nameInput">Image name (e.g. d10m, d15m, d20m)</label>
  <input type="text" id="nameInput" placeholder="d10m" maxlength="30">

  <button id="captureBtn" onclick="startCapture()">Capture + Download (3 resolutions)</button>

  <div id="statusBox">
    <div class="badge idle">Idle</div>
    <div class="progress-wrap" id="progressWrap">
      <div class="progress-bar" id="progressBar"></div>
    </div>
    <div id="log"></div>
  </div>
</div>

<script>
  const LABELS = ['320x240', '800x600', '1024x768'];

  async function startCapture() {
    const name = document.getElementById('nameInput').value.trim();
    if (!name) { setBadge('error', 'Enter a name first'); return; }

    document.getElementById('captureBtn').disabled = true;
    document.getElementById('log').innerHTML = '';
    document.getElementById('progressWrap').style.display = 'block';
    setProgress(0);
    setBadge('running', 'Capturing all 3 resolutions...');

    try {
      const t0 = Date.now();
      const r  = await fetch('/capture?name=' + encodeURIComponent(name));
      const t  = await r.text();
      logLine('Captured in ' + (Date.now() - t0) + ' ms (' + t + ')');
      setProgress(20);

      // Download all 3 sequentially as soon as capture is done
      for (let i = 0; i < 3; i++) {
        setBadge('running', 'Downloading ' + LABELS[i] + '...');
        const dt0 = Date.now();
        await downloadFrame(i, name + '_' + LABELS[i] + '.jpg');
        logLine('Downloaded ' + LABELS[i] + ' in ' + (Date.now() - dt0) + ' ms');
        setProgress(20 + ((i + 1) / 3) * 80);
      }

      setBadge('done', 'Done in ' + (Date.now() - t0) + ' ms');
    } catch (e) {
      setBadge('error', 'Failed: ' + e.message);
    } finally {
      document.getElementById('captureBtn').disabled = false;
    }
  }

  async function downloadFrame(slot, filename) {
    const resp = await fetch('/frame?slot=' + slot + '&t=' + Date.now());
    if (!resp.ok) throw new Error('frame ' + slot + ' http ' + resp.status);
    const blob = await resp.blob();
    const url  = URL.createObjectURL(blob);
    const a    = document.createElement('a');
    a.href = url; a.download = filename;
    document.body.appendChild(a); a.click(); document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  function setBadge(cls, msg) {
    const box = document.getElementById('statusBox');
    let b = box.querySelector('.badge');
    if (!b) { b = document.createElement('div'); box.prepend(b); }
    b.className = 'badge ' + cls;
    b.textContent = msg;
  }
  function logLine(msg) {
    const log = document.getElementById('log');
    const d   = document.createElement('div');
    d.textContent = msg;
    log.appendChild(d);
    log.scrollTop = log.scrollHeight;
  }
  function setProgress(pct) {
    document.getElementById('progressBar').style.width = pct + '%';
  }
</script>
</body>
</html>
)rawhtml";
  server.send(200, "text/html", html);
}

// ------------------------------------------------------------------
// Synchronous capture: blocks until all 3 frames are in PSRAM, then returns.
// Browser knows it's safe to start downloading.
void handleCapture() {
  if (capturing) {
    server.send(429, "text/plain", "busy");
    return;
  }
  if (server.hasArg("name")) {
    sessionName = server.arg("name");
    sessionName.replace(" ", "_");
    sessionName.replace("\"", "");
    sessionName.replace("/", "_");
  }

  capturing = true;
  for (int i = 0; i < TOTAL_STEPS; i++) frames[i].ready = false;

  captureAll();

  capturing = false;
  server.send(200, "text/plain", sessionName);
}

// ------------------------------------------------------------------
void handleFrame() {
  int slot = server.hasArg("slot") ? server.arg("slot").toInt() : 0;
  if (slot < 0 || slot >= TOTAL_STEPS) {
    server.send(400, "text/plain", "bad slot");
    return;
  }
  Frame& f = frames[slot];
  if (!f.ready || f.buf == nullptr || f.len == 0) {
    server.send(404, "text/plain", "no frame");
    return;
  }
  server.sendHeader("Content-Disposition",
                    "attachment; filename=" + f.label + ".jpg");
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "image/jpeg", (const char*)f.buf, f.len);
}

// ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  for (int i = 0; i < TOTAL_STEPS; i++) {
    frames[i].buf   = nullptr;
    frames[i].len   = 0;
    frames[i].ready = false;
  }

  initCameraOnce();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);

  Serial.println("=================================");
  Serial.println("AP started");
  Serial.print  ("SSID     : "); Serial.println(ap_ssid);
  Serial.print  ("Password : "); Serial.println(ap_password);
  Serial.print  ("Open     : http://"); Serial.println(WiFi.softAPIP());
  Serial.println("=================================");

  server.on("/",        handleRoot);
  server.on("/capture", handleCapture);
  server.on("/frame",   handleFrame);
  server.begin();
}

// ------------------------------------------------------------------
void loop() {
  server.handleClient();
}
