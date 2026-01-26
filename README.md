# Fast Translator

<div align="center">

**⚡ Ultra-fast offline translation & AI Assistant at your fingertips**

[![Latest Release](https://img.shields.io/github/v/release/reimen-cpu/Fast_translator?style=for-the-badge&label=Download&color=blue)](https://github.com/reimen-cpu/Fast_translator/releases/latest)

[**📥 Download Latest .DEB**](https://github.com/reimen-cpu/Fast_translator/releases/latest)

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

- 🤖 **AI Powered** - Get instant context-aware explanations via local **Ollama** LLMs at your fingertips!
- 🚀 **Blazing Fast** - Native C++ implementation with CTranslate2.
- 🔒 **100% Offline** - No internet required for translation.
- 🌍 **60+ Languages** - Support for most major world languages.
- ⌨️ **Global Shortcut** - Select text and press your custom hotkey to translate instantly.
- 🔗 **Chain Translation** - Translate between any languages via intermediate steps.
- 📦 **Smart Manager** - Easy GUI to manage language packs and models.

---

## 📥 Installation (Recommended)

The easiest way to install is using the `.deb` package for Debian/Ubuntu based systems.

1. **Download** the latest `.deb` file from the [**Releases Page**](https://github.com/reimen-cpu/Fast_translator/releases/latest).
2. **Install** via terminal:
   ```bash
   sudo apt install ./fast-translator_*.deb
   ```
   *(Or simply double-click the file to install with your software center)*

---

## 🚀 Usage Guide

Once installed, the application is designed to be invisible and ready at your fingertips.

### 1️⃣ Set your Shortcut (First Run)
Open the manager to configure your languages and shortcut:
```bash
fast-translator-manager
```
1. Go to the **Settings** or **Shortcut** tab.
2. Click **"Set Shortcut"**.
3. Use your system settings to bind that command to a key (e.g., `Ctrl+Alt+T`).

### 2️⃣ Translate & Ask AI
1. **Select any text** on your screen (browser, document, generic text).
2. **Press your configured Shortcut**.
3. A notification will appear with:
   - The **Translation** of the selected text.
   - An **AI Generated Explanation** or context (if Ollama is running).

### 3️⃣ AI Configuration (Ollama)
To enable the AI features:
1. Ensure [Ollama](https://ollama.com/) is installed and running (`ollama serve`).
2. The translator will automatically detect it and use it to enhance your translations.

---

## 🛠️ Building from Source (Advanced)

If you are a developer or using a non-Debian distribution, you can build from source.

### Prerequisites (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install -y build-essential cmake git curl wget \
    libwxgtk3.2-dev libcurl4-openssl-dev \
    xclip xdotool libnotify-bin
```

### Build Instructions
```bash
git clone https://github.com/reimen-cpu/Fast_translator.git
cd Fast_translator
./build_native.sh
```

---

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

<div align="center">
Made with ❤️ for the open-source community
</div>
