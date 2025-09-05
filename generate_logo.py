import json
import matplotlib.pyplot as plt
import matplotlib.patches as patches
import numpy as np

# Load the logo scene
with open('D:/KengaAI_Engine/assets/levels/kengaai_logo.json', 'r') as f:
    scene = json.load(f)

# Create a figure and axis
fig, ax = plt.subplots(1, 1, figsize=(10, 6))
ax.set_xlim(-8, 8)
ax.set_ylim(-6, 6)
ax.set_aspect('equal')
ax.set_facecolor((0.05, 0.05, 0.1))

# Draw the floor
floor = scene['level']['boxes'][0]
floor_rect = patches.Rectangle(
    (floor['pos'][0] - floor['size'][0]/2, floor['pos'][2] - floor['size'][2]/2),
    floor['size'][0], floor['size'][2],
    facecolor=(0.1, 0.1, 0.2),
    edgecolor='none'
)
ax.add_patch(floor_rect)

# Draw the vertical bars
for i in range(1, 5):
    box = scene['level']['boxes'][i]
    bar = patches.Rectangle(
        (box['pos'][0] - box['size'][0]/2, box['pos'][2] - box['size'][2]/2),
        box['size'][0], box['size'][2],
        facecolor=box['color'][:3],  # RGB only
        edgecolor='none'
    )
    ax.add_patch(bar)

# Draw the top connector
connector = scene['level']['boxes'][5]
connector_rect = patches.Rectangle(
    (connector['pos'][0] - connector['size'][0]/2, connector['pos'][2] - connector['size'][2]/2),
    connector['size'][0], connector['size'][2],
    facecolor=connector['color'][:3],  # RGB only
    edgecolor='none'
)
ax.add_patch(connector_rect)

# Add text
ax.text(0, -4, 'KENGA', fontsize=36, fontweight='bold', ha='center', va='center', 
        color='white', family='monospace')
ax.text(0, -5, 'AI ENGINE', fontsize=24, fontweight='normal', ha='center', va='center', 
        color=(0.8, 0.8, 0.8), family='monospace')

# Remove axes
ax.set_xticks([])
ax.set_yticks([])

# Save the logo
plt.savefig('D:/KengaAI_Engine/assets/logo.png', dpi=300, bbox_inches='tight', 
            facecolor=(0.05, 0.05, 0.1), edgecolor='none')
plt.close()

print("Logo created successfully!")