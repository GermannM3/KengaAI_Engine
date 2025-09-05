from PIL import Image
import numpy as np

# Create a simple grid texture for the floor
width, height = 256, 256
img = Image.new('RGB', (width, height), color=(50, 50, 60))  # Dark blue-gray color

# Add grid lines
pixels = img.load()
for i in range(width):
    for j in range(height):
        # Add grid pattern
        if i % 32 < 2 or j % 32 < 2:
            pixels[i, j] = (80, 80, 90)  # Lighter grid lines

# Save the image
img.save('D:/KengaAI_Engine/assets/textures/floor.png')
print("Floor texture created successfully!")