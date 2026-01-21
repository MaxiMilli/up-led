Import("env")

import os

# Get the project directory from PlatformIO environment
project_dir = env["PROJECT_DIR"]
version_file = os.path.join(project_dir, "version.txt")

# Read current version
if os.path.exists(version_file):
    with open(version_file, "r") as f:
        version = int(f.read().strip())
else:
    version = 1
    with open(version_file, "w") as f:
        f.write(str(version))

# Add version as compile-time define
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", version)])

print(f"Building firmware version: {version}")
