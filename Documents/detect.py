import os
import cv2
import numpy as np
from ultralytics import YOLO
from PIL import Image

# ===== CONFIGURE THIS =====
FOLDER     = "images_esp"
OUTPUT     = "images_esp_detected"
MODEL      = "yolo11n.pt"          # downloads automatically on first run
CONFIDENCE = 0.3                    # lower = detect more, higher = stricter

# ---- Box appearance ----
BOX_COLOR     = (0, 255, 255)   # BGR: (B, G, R)  — black
                                    # red   = (0, 0, 255)
                                    # blue  = (255, 0, 0)
                                    # yellow= (0, 255, 255)
                                    # cyan  = (255, 255, 0)
                                    # white = (255, 255, 255)
BOX_THICKNESS = 2                   # line thickness in pixels
# ==========================

os.makedirs(OUTPUT, exist_ok=True)

extensions = (".jpg", ".jpeg", ".JPG", ".JPEG")
image_files = sorted([
    f for f in os.listdir(FOLDER)
    if f.endswith(extensions)
])

if not image_files:
    print(f"No images found in '{FOLDER}'")
    exit()

total = len(image_files)
print(f"Found {total} images — loading YOLOv8 model...")

model = YOLO(MODEL)

# Person class ID in COCO dataset is 0
PERSON_CLASS = 0

summary = []

for filename in image_files:
    path    = os.path.join(FOLDER, filename)

    # Convert greyscale JPEG to RGB for YOLO (YOLO expects 3 channels)
    img_pil = Image.open(path).convert("RGB")
    img_np  = np.array(img_pil)

    results = model.predict(
        source  = img_np,
        classes = [PERSON_CLASS],
        conf    = CONFIDENCE,
        verbose = False
    )[0]

    detections = len(results.boxes)

    # Draw ONLY the boxes — no labels, no confidence scores, custom colour
    annotated = cv2.cvtColor(img_np, cv2.COLOR_RGB2BGR)   # RGB → BGR for cv2
    for box in results.boxes.xyxy.tolist():
        x1, y1, x2, y2 = map(int, box)
        cv2.rectangle(annotated, (x1, y1), (x2, y2),
                      BOX_COLOR, BOX_THICKNESS)

    out_path = os.path.join(OUTPUT, filename)
    cv2.imwrite(out_path, annotated)

    status = f"{'OK ' if detections > 0 else '-- '} {filename:<30} — {detections} person(s) detected"
    print(status)
    summary.append({
        "file":       filename,
        "detections": detections,
        "boxes":      results.boxes.conf.tolist() if detections > 0 else []
    })

# Summary
print("\n" + "="*60)
detected     = sum(1 for s in summary if s["detections"] > 0)
not_detected = total - detected
print(f"Total images  : {total}")
print(f"Person found  : {detected}")
print(f"No person     : {not_detected}")
print(f"Output saved  : {OUTPUT}/")
print("="*60)

# Show confidence scores per image
print("\nConfidence scores per detection:")
for s in summary:
    if s["boxes"]:
        scores = ", ".join(f"{c:.2f}" for c in s["boxes"])
        print(f"  {s['file']:<30} → [{scores}]")
