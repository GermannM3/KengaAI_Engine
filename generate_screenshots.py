import matplotlib.pyplot as plt
import matplotlib.patches as patches
import numpy as np
import os

# Создаем директорию для скриншотов, если её нет
os.makedirs('D:/KengaAI_Engine/screenshots', exist_ok=True)

# Создаем несколько изображений, демонстрирующих возможности движка

# 1. Главное меню
fig, ax = plt.subplots(1, 1, figsize=(10, 6))
ax.set_xlim(-10, 10)
ax.set_ylim(-8, 8)
ax.set_aspect('equal')
ax.set_facecolor((0.05, 0.05, 0.1))

# Floor
floor = patches.Rectangle((-8, -6), 16, 4, facecolor=(0.1, 0.1, 0.2), edgecolor='none')
ax.add_patch(floor)

# Level selection platforms
platform1 = patches.Rectangle((-6, -2), 3, 1, facecolor=(0.8, 0.2, 0.2), edgecolor='none')
platform2 = patches.Rectangle((3, -2), 3, 1, facecolor=(0.2, 0.8, 0.2), edgecolor='none')
ax.add_patch(platform1)
ax.add_patch(platform2)

# Add text
ax.text(0, 5, 'KENGaQUEST', fontsize=36, fontweight='bold', ha='center', va='center', 
        color='white', family='monospace')
ax.text(-4.5, -1.5, 'Level 1', fontsize=20, fontweight='normal', ha='center', va='center', 
        color='white', family='monospace')
ax.text(4.5, -1.5, 'Level 2', fontsize=20, fontweight='normal', ha='center', va='center', 
        color='white', family='monospace')

# Remove axes
ax.set_xticks([])
ax.set_yticks([])

plt.savefig('D:/KengaAI_Engine/screenshots/main_menu.png', dpi=150, bbox_inches='tight', 
            facecolor=(0.05, 0.05, 0.1), edgecolor='none')
plt.close()

# 2. Уровень с освещением
fig, ax = plt.subplots(1, 1, figsize=(10, 6))
ax.set_xlim(-10, 10)
ax.set_ylim(-8, 8)
ax.set_aspect('equal')
ax.set_facecolor((0.05, 0.05, 0.1))

# Floor
floor = patches.Rectangle((-8, -6), 16, 12, facecolor=(0.2, 0.2, 0.3), edgecolor='none')
ax.add_patch(floor)

# Walls
wall1 = patches.Rectangle((-8, -6), 1, 12, facecolor=(0.3, 0.3, 0.4), edgecolor='none')
wall2 = patches.Rectangle((7, -6), 1, 12, facecolor=(0.3, 0.3, 0.4), edgecolor='none')
wall3 = patches.Rectangle((-8, -6), 16, 1, facecolor=(0.3, 0.3, 0.4), edgecolor='none')
wall4 = patches.Rectangle((-8, 5), 16, 1, facecolor=(0.3, 0.3, 0.4), edgecolor='none')
ax.add_patch(wall1)
ax.add_patch(wall2)
ax.add_patch(wall3)
ax.add_patch(wall4)

# Boxes
box1 = patches.Rectangle((-2, -5), 2, 2, facecolor=(0.8, 0.2, 0.2), edgecolor='none')
box2 = patches.Rectangle((3, -4), 1.5, 1.5, facecolor=(0.2, 0.8, 0.4), edgecolor='none')
ax.add_patch(box1)
ax.add_patch(box2)

# Light sources (drawn as bright circles)
light1 = patches.Circle((0, 3), 0.5, facecolor=(1, 1, 1), edgecolor='none', alpha=0.8)
light2 = patches.Circle((-2, -4), 0.3, facecolor=(1, 0.2, 0.2), edgecolor='none', alpha=0.6)
light3 = patches.Circle((3, -3.5), 0.25, facecolor=(0.2, 1, 0.4), edgecolor='none', alpha=0.6)
ax.add_patch(light1)
ax.add_patch(light2)
ax.add_patch(light3)

# Add text
ax.text(0, 6.5, 'Уровень с освещением', fontsize=24, fontweight='bold', ha='center', va='center', 
        color='white', family='monospace')

# Remove axes
ax.set_xticks([])
ax.set_yticks([])

plt.savefig('D:/KengaAI_Engine/screenshots/lighting_level.png', dpi=150, bbox_inches='tight', 
            facecolor=(0.05, 0.05, 0.1), edgecolor='none')
plt.close()

# 3. Уровень с частицами
fig, ax = plt.subplots(1, 1, figsize=(10, 6))
ax.set_xlim(-10, 10)
ax.set_ylim(-8, 8)
ax.set_aspect('equal')
ax.set_facecolor((0.1, 0.05, 0.05))

# Floor
floor = patches.Rectangle((-8, -6), 16, 12, facecolor=(0.3, 0.2, 0.2), edgecolor='none')
ax.add_patch(floor)

# Central object
center = patches.Rectangle((-1, -4), 2, 4, facecolor=(0.8, 0.6, 0.2), edgecolor='none')
ax.add_patch(center)

# Particles (randomly placed)
np.random.seed(42)  # For reproducible results
for _ in range(100):
    x = np.random.uniform(-6, 6)
    y = np.random.uniform(-5, 5)
    size = np.random.uniform(0.1, 0.5)
    alpha = np.random.uniform(0.3, 0.8)
    particle = patches.Circle((x, y), size, facecolor=(1, 0.5, 0.2), edgecolor='none', alpha=alpha)
    ax.add_patch(particle)

# Add text
ax.text(0, 6.5, 'Уровень с частицами', fontsize=24, fontweight='bold', ha='center', va='center', 
        color='white', family='monospace')

# Remove axes
ax.set_xticks([])
ax.set_yticks([])

plt.savefig('D:/KengaAI_Engine/screenshots/particles_level.png', dpi=150, bbox_inches='tight', 
            facecolor=(0.1, 0.05, 0.05), edgecolor='none')
plt.close()

print("Скриншоты созданы успешно!")