import math
import struct
import zlib
import os

def write_png_gray(filename, width, height, data):
    # PNG Signature
    png = b'\x89PNG\r\n\x1a\n'
    
    # IHDR chunk: width (4 bytes), height (4 bytes), bit depth (1 byte, 8), color type (1 byte, 0 - grayscale), 
    # compression (1 byte, 0), filter (1 byte, 0), interlace (1 byte, 0)
    ihdr = struct.pack('>IIBBBBB', width, height, 8, 0, 0, 0, 0)
    png += struct.pack('>I', 13) + b'IHDR' + ihdr + struct.pack('>I', zlib.crc32(b'IHDR' + ihdr))
    
    # IDAT chunk
    scanlines = []
    for y in range(height):
        scanlines.append(b'\x00' + data[y*width : (y+1)*width])
    idat_data = zlib.compress(b''.join(scanlines))
    png += struct.pack('>I', len(idat_data)) + b'IDAT' + idat_data + struct.pack('>I', zlib.crc32(b'IDAT' + idat_data))
    
    # IEND chunk
    png += struct.pack('>I', 0) + b'IEND' + struct.pack('>I', zlib.crc32(b'IEND'))
    
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    with open(filename, 'wb') as f:
        f.write(png)
    print(f"Generated Gray PNG: {filename}")

def write_hdr(filename, width, height, pixels):
    rgbe_data = bytearray(width * height * 4)
    for i in range(width * height):
        r, g, b = pixels[i]
        val = max(r, g, b)
        if val < 1e-32:
            rgbe_data[i*4 : i*4+4] = (0, 0, 0, 0)
        else:
            mantissa, exponent = math.frexp(val)
            scale = mantissa * 256.0 / val
            rgbe = (
                min(255, max(0, int(r * scale))),
                min(255, max(0, int(g * scale))),
                min(255, max(0, int(b * scale))),
                min(255, max(0, int(exponent + 128)))
            )
            rgbe_data[i*4 : i*4+4] = rgbe
            
    header = f"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {height} +X {width}\n".encode('ascii')
    
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    with open(filename, 'wb') as f:
        f.write(header)
        f.write(rgbe_data)
    print(f"Generated HDR HDRI: {filename}")

def generate_seabed(filename):
    width, height = 256, 256
    data = bytearray(width * height)
    
    for y in range(height):
        for x in range(width):
            # Calculate overlapping sine waves to create gentle sand ripples / dunes
            # High frequency small ripples
            ripple = math.sin(x * 0.4) * math.cos(y * 0.4) * 4.0
            # Medium frequency ripples
            dunes_m = math.sin(x * 0.08 + y * 0.04) * math.cos(y * 0.08 - x * 0.04) * 15.0
            # Low frequency sand dunes
            dunes_l = math.sin(x * 0.02) * math.cos(y * 0.02) * 45.0
            
            val = 127.0 + dunes_l + dunes_m + ripple
            data[y * width + x] = min(255, max(0, int(val)))
            
    write_png_gray(filename, width, height, data)

def generate_crater(filename):
    width, height = 256, 256
    data = bytearray(width * height)
    
    # Generate background terrain (fractal noise using sine/cosine waves)
    for y in range(height):
        for x in range(width):
            val = 128.0
            # Layer 1: broad hills
            val += math.sin(x * 0.03) * math.cos(y * 0.03) * 30.0
            # Layer 2: smaller rocky ridges
            val += math.sin(x * 0.08 + y * 0.05) * 12.0
            # Layer 3: rough rocky noise
            val += math.sin(x * 0.25) * math.cos(y * 0.2) * 4.0
            data[y * width + x] = min(255, max(0, int(val)))
            
    # Carve craters (with depression and raised lips)
    def carve_crater(cx, cy, r_crater, depth):
        for y in range(height):
            for x in range(width):
                dx = x - cx
                dy = y - cy
                dist = math.sqrt(dx*dx + dy*dy)
                
                if dist < r_crater * 0.8:
                    # Inner depression
                    factor = 1.0 - (dist / (r_crater * 0.8))**2
                    idx = y * width + x
                    val = float(data[idx]) - depth * factor
                    data[idx] = min(255, max(0, int(val)))
                elif dist < r_crater * 1.25:
                    # Raised rim lip
                    # Peak of the rim is at r_crater
                    dist_from_rim = abs(dist - r_crater)
                    rim_width = r_crater * 0.25
                    factor = 1.0 - (dist_from_rim / rim_width)**2
                    if factor > 0:
                        idx = y * width + x
                        val = float(data[idx]) + depth * 0.35 * factor
                        data[idx] = min(255, max(0, int(val)))

    # Add craters of various sizes across the map
    carve_crater(128, 128, 45, 65) # Large center crater
    carve_crater(55, 65, 25, 45)   # Top-left crater
    carve_crater(195, 185, 32, 50) # Bottom-right crater
    carve_crater(185, 60, 20, 35)   # Top-right crater
    carve_crater(75, 190, 28, 40)   # Bottom-left crater
    carve_crater(120, 45, 12, 25)   # Small crater
    carve_crater(40, 140, 16, 30)   # Medium-small crater

    write_png_gray(filename, width, height, data)

def generate_underwater_hdr(filename):
    width, height = 512, 256
    pixels = []
    
    # Sun direction in equirectangular space
    # Matches MainLight direction in config: [-0.1, -0.98, -0.1]
    # In skybox coordinates, light comes FROM: [0.1, 0.98, 0.1]
    sun_dir = (0.1, 0.98, 0.1)
    # Normalize sun direction
    sun_len = math.sqrt(sun_dir[0]**2 + sun_dir[1]**2 + sun_dir[2]**2)
    sun_dir = (sun_dir[0]/sun_len, sun_dir[1]/sun_len, sun_dir[2]/sun_len)
    
    for y in range(height):
        # theta goes from +pi/2 (top, Y=0) to -pi/2 (bottom, Y=height-1)
        theta = (1.0 - 2.0 * y / (height - 1)) * (math.pi * 0.5)
        sin_theta = math.sin(theta)
        cos_theta = math.cos(theta)
        
        for x in range(width):
            # phi goes from 0 to 2*pi
            phi = (x / (width - 1)) * (math.pi * 2.0)
            
            # Pixel direction vector (3D sphere)
            px = cos_theta * math.cos(phi)
            py = sin_theta
            pz = cos_theta * math.sin(phi)
            
            # 1. Base gradient
            # Deepest color at bottom: deep dark blue-black [0.005, 0.02, 0.05]
            # Medium color at horizon (theta=0): teal blue [0.03, 0.15, 0.28]
            # Brightest color at top (theta=pi/2): cyan/turquoise [0.1, 0.5, 0.65]
            if py < 0:
                # Interpolate bottom to horizon
                t = py + 1.0 # [0, 1]
                r = 0.005 + t * (0.03 - 0.005)
                g = 0.02 + t * (0.15 - 0.02)
                b = 0.05 + t * (0.28 - 0.05)
            else:
                # Interpolate horizon to top
                t = py # [0, 1]
                r = 0.03 + t * (0.12 - 0.03)
                g = 0.15 + t * (0.58 - 0.15)
                b = 0.28 + t * (0.78 - 0.28)
                
            # 2. Add sunburst / volumetric glow from the sun direction
            dot = px * sun_dir[0] + py * sun_dir[1] + pz * sun_dir[2]
            if dot > 0.0:
                # Sun core (very intense)
                sun_core = math.pow(dot, 120) * 12.0
                # Sun halo (wide soft glow)
                sun_halo = math.pow(dot, 6) * 1.8
                
                # Sunlight color is slightly yellow-greenish under water [0.8, 1.0, 0.9]
                r += (sun_core + sun_halo) * 0.55
                g += (sun_core + sun_halo) * 0.95
                b += (sun_core + sun_halo) * 0.90
                
            pixels.append((r, g, b))
            
    write_hdr(filename, width, height, pixels)

if __name__ == "__main__":
    print("Generating procedural assets...")
    
    # Generate Heightmaps
    generate_seabed("resources/textures/heightmaps/seabed_heightmap.png")
    generate_crater("resources/textures/heightmaps/crater_heightmap.png")
    
    # Generate Skybox HDR
    generate_underwater_hdr("resources/textures/skybox_hdr/underwater_hdr.hdr")
    
    print("Procedural assets generation complete!")
