# Fast Translator

<div align="center">

**⚡ Ultra-fast offline translation for Linux, powered by Argos Translate**

[![Latest Release](https://img.shields.io/github/v/release/reimen-cpu/Fast_translator?style=for-the-badge&label=Download&color=blue)](https://github.com/reimen-cpu/Fast_translator/releases/latest)

[**📥 Download Latest Release**](https://github.com/reimen-cpu/Fast_translator/releases/latest)

</div>

---

## 🖼️ Screenshots

<div align="center">
<table>
<tr>
<td width="50%">
<img src="https://raw.githubusercontent.com/reimen-cpu/Fast_translator/main/img/Screenshot_20260122_225326.png" alt="Installed Languages" style="max-width:100%;">
<p align="center"><em>Package Manager - Available Packages</em></p>
</td>
<td width="50%">
<img src="https://raw.githubusercontent.com/reimen-cpu/Fast_translator/main/img/Screenshot_20260122_225346.png" alt="Available Packages" style="max-width:100%;">
<p align="center"><em>Package Manager - Installed Languages</em></p>
</td>
</tr>
</table>
</div>

---

## ✨ Features

- 🚀 **Blazing Fast** - Native C++ implementation with CTranslate2
- 🔒 **100% Offline** - No internet required after downloading language packs
- 🌍 **60+ Languages** - Support for most major world languages
- 🔗 **Chain Translation** - Translate between any languages via intermediate steps
- ⌨️ **Keyboard Shortcut** - Set a global hotkey for instant translation
- 🔍 **Smart Search** - Filter language packs with predictive search
- 📦 **Easy Package Management** - Download and manage language packs with GUI

---

## 📋 Prerequisites

Before building, ensure you have these tools installed:

### Ubuntu/Debian
```bash
sudo apt update
sudo apt install -y build-essential cmake git curl wget \
    libwxgtk3.2-dev libcurl4-openssl-dev \
    xclip xdotool libnotify-bin
```

### Fedora
```bash
sudo dnf install -y gcc-c++ cmake git curl wget \
    wxGTK3-devel libcurl-devel \
    xclip xdotool libnotify
```

### Arch Linux
```bash
sudo pacman -S --needed base-devel cmake git curl wget \
    wxwidgets-gtk3 \
    xclip xdotool libnotify
```

### Additional Requirements

You also need **vcpkg** for the CTranslate2 dependency:

```bash
# Install vcpkg (if not already installed)
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg && ./bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg
```

---

## 🔨 Building from Source

```bash
# Clone the repository
git clone https://github.com/reimen-cpu/Fast_translator.git
cd Fast_translator

# Build (this will compile both CLI and GUI)
./build_native.sh
```

The executables will be in the `build/` directory:
- `Fast_translator` - Command-line translator
- `Fast_translator_manager` - GUI package manager

---

## 🚀 Usage

### CLI Translation

```bash
# English to Spanish (default)
./build/Fast_translator

# Spanish to English
./build/Fast_translator es

# Chain translation: Arabic to French (via English)
./build/Fast_translator ar:fr

# Explicit chain: Arabic → Spanish → French
./build/Fast_translator ar:es:fr
```

The CLI reads from the clipboard, translates, and outputs the result.

### GUI Package Manager

```bash
./build/Fast_translator_manager
```

1. **Installed Tab** - View and manage downloaded language packs
2. **Available Tab** - Browse and download new language packs
3. **Set Shortcut** - Configure a global keyboard shortcut
4. **Search** - Filter languages by typing in the search bar

---

## ⌨️ Setting Up a Keyboard Shortcut

1. Open the GUI and select a language pair
2. Click **"Set Shortcut"**
3. The command is copied to clipboard
4. System Settings will open automatically
5. Add a custom shortcut with the copied command

---

## 📂 Project Structure

```
Fast_translator/
├── src/
│   ├── main.cpp           # CLI translator
│   ├── gui_main.cpp       # GUI package manager
│   ├── translation.cpp    # Translation engine
│   ├── language_graph.cpp # Chain translation pathfinding
│   └── utils.cpp          # Clipboard & system utilities
├── packages/              # Downloaded language packs
├── build/                 # Compiled binaries
└── build_native.sh        # Build script
```

---

## 📄 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

- [Argos Translate](https://github.com/argosopentech/argos-translate) - Translation models
- [CTranslate2](https://github.com/OpenNMT/CTranslate2) - Fast inference engine
- [SentencePiece](https://github.com/google/sentencepiece) - Tokenization

---

<div align="center">

Made with ❤️ for the open-source community

</div>
