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

## Documentation
This project includes a manual page `bam.1`.

### View Manual
To view the manual without installing:
```bash
man ./bam.1
```

### Install Manual
To install the manual system-wide (requires sudo):
```bash
sudo mkdir -p /usr/local/share/man/man1/  # If directory doesn't exist
sudo cp bam.1 /usr/local/share/man/man1/
sudo mandb
```
Then you can run `man bam` from anywhere.


## Todo
<ol>
<li>In `window.c` change it to firstly take the css file from `src/style.css` and if it doesn't exists take it from `~/.config/bam/style.css` and if this doesn't exist continue without styling</li>
<li>Finish the styling to my liking and create an explanation markdown how to create your own</li>
<li>Validate behavior on different resolutions</li>
<li>Add a button to the top bar so it's easy access to run the binary</li>
</ol>