import os
import numpy as np
import matplotlib.pyplot as plt
from PIL import Image

# ===== CONFIGURE THIS =====
FOLDER   = "images_esp"
PER_PAGE = 6
COLS     = 3
# ==========================

extensions = (".jpg", ".jpeg", ".JPG", ".JPEG")
image_files = sorted([
    f for f in os.listdir(FOLDER)
    if f.endswith(extensions)
])

if not image_files:
    print(f"No images found in '{FOLDER}' folder.")
    exit()

total = len(image_files)
print(f"Found {total} images in '{FOLDER}'")

rows      = (PER_PAGE + COLS - 1) // COLS
pages     = (total + PER_PAGE - 1) // PER_PAGE
page_data = [image_files[i * PER_PAGE:(i + 1) * PER_PAGE] for i in range(pages)]

state = {"page": 0}

def draw_page(fig, page_idx):
    fig.clear()
    chunk = page_data[page_idx]
    n     = len(chunk)

    axes      = fig.subplots(rows, COLS)
    axes_flat = np.array(axes).flatten()

    fig.suptitle(
        f"Greyscale Pixel Intensity — Page {page_idx + 1}/{pages}  "
        f"(images {page_idx * PER_PAGE + 1}–{page_idx * PER_PAGE + n} of {total})",
        fontsize=14, fontweight="bold"
    )

    for i, filename in enumerate(chunk):
        path = os.path.join(FOLDER, filename)
        ax   = axes_flat[i]

        img  = Image.open(path).convert("L")
        arr  = np.array(img).flatten()
        w, h = img.size

        mean   = np.mean(arr)
        median = np.median(arr)
        std    = np.std(arr)
        dark   = np.sum(arr < 85)                      / len(arr) * 100
        mid    = np.sum((arr >= 85) & (arr < 170))     / len(arr) * 100
        bright = np.sum(arr >= 170)                    / len(arr) * 100

        bar_color = "#5B9BD5" if mean < 80 else "#F4A261"

        ax.hist(arr, bins=256, range=(0, 255),
                color=bar_color, alpha=0.85, edgecolor="none")

        ax.axvline(mean,   color="black", linewidth=1.5,
                   linestyle="--", label=f"Mean {mean:.1f}")
        ax.axvline(median, color="grey",  linewidth=1.5,
                   linestyle=":",  label=f"Median {median:.1f}")

        ax.axvspan(0,   85,  alpha=0.06, color="blue")
        ax.axvspan(85,  170, alpha=0.06, color="green")
        ax.axvspan(170, 255, alpha=0.06, color="yellow")

        ax.set_title(f"{filename}  ({w}×{h})", fontsize=9, fontweight="bold")
        ax.set_xlabel("")
        ax.set_ylabel("")
        ax.set_xlim(0, 255)
        ax.legend(fontsize=8)
        ax.grid(axis="y", alpha=0.3)

    for j in range(n, len(axes_flat)):
        axes_flat[j].set_visible(False)

    fig.text(0.5,  0.01, "Pixel Intensity  (0 = black → 255 = white)",
             ha="center", fontsize=11, fontweight="bold")
    fig.text(0.01, 0.5,  "Number of Pixels",
             va="center", rotation="vertical", fontsize=11, fontweight="bold")

    ax_prev = fig.add_axes([0.35, 0.94, 0.12, 0.04])
    ax_next = fig.add_axes([0.53, 0.94, 0.12, 0.04])

    from matplotlib.widgets import Button
    btn_prev = Button(ax_prev, "◀ Prev")
    btn_next = Button(ax_next, "Next ▶")

    def go_prev(event):
        if state["page"] > 0:
            state["page"] -= 1
            draw_page(fig, state["page"])
            fig.canvas.draw_idle()

    def go_next(event):
        if state["page"] < pages - 1:
            state["page"] += 1
            draw_page(fig, state["page"])
            fig.canvas.draw_idle()

    btn_prev.on_clicked(go_prev)
    btn_next.on_clicked(go_next)

    fig._btn_prev = btn_prev
    fig._btn_next = btn_next

    plt.tight_layout(rect=[0.03, 0.04, 1.0, 0.93])
    plt.subplots_adjust(hspace=0.55, wspace=0.3)

fig = plt.figure(figsize=(6 * COLS, 5 * rows))
draw_page(fig, 0)
plt.show()