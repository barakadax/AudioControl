# Audio manager
Simple app to control audio in linux

## Build, Install & Run

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

### Run
```bash
./bam
```

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

### Styling
To style the app, create a file called `style.css` in the `~/.config/bam/` directory. The app will look for this file and apply the styles to the window.
There is an example file `style_template.css`

## Todo
<ol>
<li>Add a button to the OS top bar so it's easy access to run the binary, use gs_logo</li>
<li>Validate behavior on different resolutions</li>
<li>Make sure always in the center of the screen</li>
<li>Fix so logo will be also in <code>~/.config/bam/</code> and the <code>.desktop</code> file will point to it or un/install scripts</li>
<li>Make sure to use the best practices of gtk and pulseaudio</li>
<li>Redesign to use design pattern and enforce better memory handling and memory leaks than runtime performance</li>
<li>Rewrite, make sure naming of structures, functions and variables are self explanatory</li>
<li>Make sure clean code standards are followed, functions no longer than 20 lines and no comments in code</li>
<li>GitHub actions</li>
</ol>
