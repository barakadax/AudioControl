# Audio manager
Simple app to control audio in linux

## Build & Install

### Prerequisites
Ensure you have the necessary dependencies installed. The `Makefile` will verify this for you, but you can install them beforehand:
```bash
sudo apt install build-essential pkg-config libgtk-4-dev libpulse-dev libx11-dev
```

### Compile
Simply run:
```bash
make
```
This will check for dependencies and compile the `bam` binary.

## Usage
### Desktop Entry
1. Edit the file ending with `.desktop` in this directory.
2. Replace `Exec` & `Icon` paths with the correct absolute paths on your system.
3. Copy the modified file to your applications folder:
   ```bash
   cp org.barakadax.audioManager.desktop ~/.local/share/applications/
   ```
4. Update the desktop database to see immediate results:
   ```bash
   update-desktop-database ~/.local/share/applications/
   ```
