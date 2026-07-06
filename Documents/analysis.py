import os
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
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

# Pre-compute all data
global_max = 0
all_data   = []
for filename in image_files:
    path = os.path.join(FOLDER, filename)
    img  = Image.open(path).convert("L")
    arr  = np.array(img).flatten()
    w, h = img.size
    hist, _ = np.histogram(arr, bins=256, range=(0, 255))
    global_max = max(global_max, hist.max())

    mean   = np.mean(arr)
    median = np.median(arr)
    std    = np.std(arr)
    dark   = np.sum(arr < 85)                  / len(arr) * 100
    mid    = np.sum((arr >= 85) & (arr < 170)) / len(arr) * 100
    bright = np.sum(arr >= 170)                / len(arr) * 100

    all_data.append({
        "filename": filename,
        "arr":    arr,
        "w":      w,
        "h":      h,
        "mean":   mean,
        "median": median,
        "std":    std,
        "dark":   dark,
        "mid":    mid,
        "bright": bright,
    })

# Save individual graphs
graphs_dir = os.path.join(FOLDER, "graphs")
os.makedirs(graphs_dir, exist_ok=True)
print(f"Saving individual graphs to '{graphs_dir}'...")

for d in all_data:
    arr       = d["arr"]
    bar_color = "#5B9BD5" if d["mean"] < 0 else "#F4A261"

    fig_single, ax_single = plt.subplots(figsize=(6, 4))

    ax_single.hist(arr, bins=256, range=(0, 255),
                   color=bar_color, alpha=0.85, edgecolor="none")

    ax_single.axvline(d["mean"],   color="black", linewidth=1.5, linestyle="--")
    ax_single.axvline(d["median"], color="grey",  linewidth=1.5, linestyle=":" )

    ax_single.axvspan(0,   85,  alpha=0.06, color="blue")
    ax_single.axvspan(85,  170, alpha=0.06, color="green")
    ax_single.axvspan(170, 255, alpha=0.06, color="yellow")

    ax_single.set_title(f"{d['filename']}  ({d['w']}×{d['h']})",
                        fontsize=10, fontweight="bold")
    ax_single.set_xlim(0, 255)
    #ax_single.set_ylim(0, global_max * 1.05)
    ax_single.set_xlabel("Pixel Intensity  (0 = black → 255 = white)", fontsize=9)
    ax_single.set_ylabel("Number of Pixels", fontsize=9)
    ax_single.grid(axis="y", alpha=0.3)

    stats_text = (
        f"Std:    {d['std']:>6.1f}\n"
        f"Dark:   {d['dark']:>5.1f}%\n"
        f"Mid:    {d['mid']:>5.1f}%\n"
        f"Bright: {d['bright']:>5.1f}%"
    )
    ax_single.text(0.98, 0.97, stats_text,
                   transform=ax_single.transAxes,
                   fontsize=8, verticalalignment="top",
                   horizontalalignment="right",
                   fontfamily="monospace",
                   bbox=dict(boxstyle="round,pad=0.3",
                             facecolor="white", alpha=0.8, edgecolor="#cccccc"))

    ax_single.legend(handles=[
        Line2D([0], [0], color="black", linewidth=1.5, linestyle="--", label="Mean"),
        Line2D([0], [0], color="grey",  linewidth=1.5, linestyle=":",  label="Median"),
    ], fontsize=8, loc="upper left")

    save_name = os.path.splitext(d['filename'])[0] + "_graph.png"
    save_path = os.path.join(graphs_dir, save_name)
    fig_single.tight_layout()
    fig_single.savefig(save_path, dpi=150, bbox_inches="tight")
    plt.close(fig_single)
    print(f"  Saved: {save_name}")

print(f"All individual graphs saved.\n")

# Split into pages
rows      = (PER_PAGE + COLS - 1) // COLS
pages     = (total + PER_PAGE - 1) // PER_PAGE
page_data = [all_data[i * PER_PAGE:(i + 1) * PER_PAGE] for i in range(pages)]

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

    for i, d in enumerate(chunk):
        ax        = axes_flat[i]
        arr       = d["arr"]
        bar_color = "#5B9BD5" if d["mean"] < 0 else "#F4A261"

        ax.hist(arr, bins=256, range=(0, 255),
                color=bar_color, alpha=0.85, edgecolor="none")

        ax.axvline(d["mean"],   color="black", linewidth=1.5, linestyle="--")
        ax.axvline(d["median"], color="grey",  linewidth=1.5, linestyle=":" )

        ax.axvspan(0,   85,  alpha=0.06, color="blue")
        ax.axvspan(85,  170, alpha=0.06, color="green")
        ax.axvspan(170, 255, alpha=0.06, color="yellow")

        ax.set_title(f"{d['filename']}  ({d['w']}×{d['h']})",
                     fontsize=9, fontweight="bold")
        ax.set_xlim(0, 255)
        #ax.set_ylim(0, global_max * 1.05)
        ax.set_xlabel("")
        ax.set_ylabel("")
        ax.grid(axis="y", alpha=0.3)

        stats_text = (
            f"Std:    {d['std']:>6.1f}\n"
            f"Dark:   {d['dark']:>5.1f}%\n"
            f"Mid:    {d['mid']:>5.1f}%\n"
            f"Bright: {d['bright']:>5.1f}%"
        )
        ax.text(0.98, 0.97, stats_text,
                transform=ax.transAxes,
                fontsize=7, verticalalignment="top",
                horizontalalignment="right",
                fontfamily="monospace",
                bbox=dict(boxstyle="round,pad=0.3",
                          facecolor="white", alpha=0.8, edgecolor="#cccccc"))

        ax.legend(handles=[
            Line2D([0], [0], color="black", linewidth=1.5, linestyle="--", label="Mean"),
            Line2D([0], [0], color="grey",  linewidth=1.5, linestyle=":",  label="Median"),
        ], fontsize=7, loc="upper left")

    # Hide unused slots
    for j in range(n, len(axes_flat)):
        axes_flat[j].set_visible(False)

    # Shared axis labels
    fig.text(0.5,  0.01, "Pixel Intensity  (0 = black → 255 = white)",
             ha="center", fontsize=11, fontweight="bold")
    fig.text(0.01, 0.5,  "Number of Pixels",
             va="center", rotation="vertical", fontsize=11, fontweight="bold")

    # Navigation buttons
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

# Print stats to terminal
print("=" * 90)
print(f"{'Filename':<30} | {'Size':<12} | {'Std':>6} | {'Dark':>7} | {'Mid':>7} | {'Bright':>7}")
print("=" * 90)
for d in all_data:
    print(f"{d['filename']:<30} | {d['w']}×{d['h']:<9} | "
          f"{d['std']:>6.1f} | {d['dark']:>6.1f}% | "
          f"{d['mid']:>6.1f}% | {d['bright']:>6.1f}%")
print("=" * 90)

fig = plt.figure(figsize=(6 * COLS, 5 * rows))
draw_page(fig, 0)
plt.show()