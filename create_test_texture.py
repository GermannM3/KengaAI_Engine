from PIL import Image
import numpy as np

# Create a simple crate-like texture
width, height = 256, 256
img = Image.new('RGB', (width, height), color=(139, 69, 19))  # Brown color

# Add some grid lines to make it look like a crate
pixels = img.load()
for i in range(width):
    for j in range(height):
        # Add grid pattern
        if i % 32 < 2 or j % 32 < 2:
            pixels[i, j] = (101, 67, 33)  # Darker brown for grid lines
        # Add some noise for texture
        elif (i + j) % 8 == 0:
            r, g, b = pixels[i, j]
            pixels[i, j] = (min(255, r + 20), min(255, g + 20), min(255, b + 20))

# Save the image
img.save('D:/KengaAI_Engine/assets/textures/crate.png')
print("Texture created successfully!")