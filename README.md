# filetoggler

A dual-mode (GUI/CLI) file toggler for quickly enabling/disabling files by moving them to a disabled directory.

**Alias:** `ft` is available as a convenient shorthand for `filetoggler`

## Features

- **Dual mode**: GUI (wxWidgets) when no file arguments, CLI otherwise
- **Auto-detect TTY**: Forces GUI mode when not running in a terminal
- **Flexible disabled file storage**:
  - Custom disabled directory (default: `.disabled.d`)
  - Optional prefix/suffix for disabled filenames
- **Bash completion**: Completes options, filenames, and original names of disabled files
- **GUI features**:
  - Directory tree + file list view
  - Sort by column (name, last-modified, size, type)
  - Multi-select with Ctrl/Shift
  - Keyboard shortcuts (Enter/Delete/Space with Shift variants)
  - Type-to-find
  - Disabled files shown in gray
  - Automatic sudo elevation on permission errors

## Installation

### Build from source

```bash
meson setup builddir
meson compile -C builddir
meson test -C builddir
sudo meson install -C builddir
```

### Dependencies

- C++20 compiler
- wxWidgets 3.0+
- Meson build system

## Usage

### CLI mode

```bash
# Toggle files (default action)
filetoggler file1.txt file2.txt
# or using the alias:
ft file1.txt file2.txt

# Explicitly enable/disable
ft -e file1.txt
ft -d file2.txt

# Change working directory
ft -C /path/to/dir -d demo.txt

# Custom disabled directory and prefix/suffix
ft -D .backup -p "~" -s ".bak" -d file.txt
# Creates: .backup/~file.txt.bak

# Dry run
ft -n -d file.txt
```

### GUI mode

```bash
# Launch GUI (no file arguments)
filetoggler
# or:
ft

# Or explicitly with directory
ft -C /path/to/dir
```

#### GUI keyboard shortcuts

**File Operations:**
- **Enter**: Enable selected files, select next
- **Shift+Enter**: Enable selected files, select previous
- **Delete**: Disable selected files, select next
- **Shift+Delete**: Disable selected files, select previous
- **Space**: Toggle selected files, select next
- **Shift+Space**: Toggle selected files, select previous

**Navigation:**
- **Alt+Left**: Go back in directory history
- **Alt+Right**: Go forward in directory history
- **Alt+Up**: Go to parent directory
- **Alt+Down**: Open selected directory

**Search:**
- **Alphanumeric**: Type-to-find (prefix search)

**Selection:**
- **Ctrl+Click**: Multi-select
- **Shift+Click**: Range select

#### GUI features

- **Menubar**: File (Select Folder, Exit), Edit (Enable, Disable, Toggle), Help (Keyboard Shortcuts, About)
- **Statusbar**: Shows selected file info (name, size, count, state)
- **Column sorting**: Click column headers to sort (Name, Last Modified, Size, Type) with visual indicators (▲/▼)
- **File icons**: Unicode folder 📁 and file 📄 icons
- **Disabled files**: Shown with gray background (selection remains visible)
- **Formatted sizes**: Human-readable units (B, KB, MB, GB, TB) with thousands separators
- **View modes**: List (detailed columns) and Icon views

### Options

```
-C/--chdir DIR               Change working directory
-D/--disabled-dir DIR        Disabled directory (default: .disabled.d)
-p/--disabled-prefix PREFIX  Prefix for disabled filenames
-s/--disabled-suffix SUFFIX  Suffix for disabled filenames
-e/--enable                  Enable specified files
-d/--disable                 Disable specified files
-t/--toggle                  Toggle files (default action)
-n/--dry-run                 Show what would be done
-v/--verbose                 Verbose output
-q/--quiet                   Suppress output
-h/--help                    Show help
--version                    Show version
```

## Bash completion

Completion is automatically installed to `/usr/share/bash-completion/completions/filetoggler`.

To enable manually:
```bash
source /usr/share/bash-completion/completions/filetoggler
```

## Examples

### Disable a file

```bash
$ ls
demo.txt  other.txt

$ ft -d demo.txt

$ ls
.disabled.d/  other.txt

$ ls .disabled.d/
demo.txt
```

### Enable it back

```bash
$ ft -e demo.txt

$ ls
demo.txt  other.txt
```

### With prefix/suffix

```bash
$ ft -p "__" -s "~" -d demo.txt

$ ls .disabled.d/
__demo.txt~

$ ft -p "__" -s "~" -e demo.txt
# Restores to: demo.txt
```

## Author

Lenik <filetoggler@bodz.net>

## License

GNU General Public License v3.0 or later (GPL-3.0-or-later)

Copyright © 2026 Lenik

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

See the [LICENSE](LICENSE) file for details.

## Repository

https://github.com/lenik/filetoggler
