#include "json.hpp" // Local JSON library
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <wx/clipbrd.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/notebook.h>
#include <wx/progdlg.h>
#include <wx/protocol/http.h>
#include <wx/sckstrm.h>
#include <wx/url.h>
#include <wx/wfstream.h>
#include <wx/wx.h>

using json = nlohmann::json;

#ifdef _WIN32
#include <windows.h>
#define PATH_MAX MAX_PATH
#elif __APPLE__
#include <limits.h>
#include <mach-o/dyld.h>
#else
#include <limits.h>
#include <linux/limits.h>
#include <unistd.h>
#endif

// Re-using/duplicating get_executable_dir for now to be self-contained in GUI,
// or should link against utils? Let's duplicate for simplicity to avoid linking
// implementation details of CLI.
std::string get_exec_dir() {
  char buf[PATH_MAX];
  std::string path;
#ifdef _WIN32
  GetModuleFileNameA(NULL, buf, PATH_MAX);
  path = std::string(buf);
  path = path.substr(0, path.find_last_of("\\"));
#elif __APPLE__
  uint32_t size = sizeof(buf);
  if (_NSGetExecutablePath(buf, &size) == 0) {
    path = std::string(buf);
    path = path.substr(0, path.find_last_of("/"));
  }
#else
  ssize_t count = readlink("/proc/self/exe", buf, PATH_MAX);
  if (count != -1) {
    path = std::string(buf, count);
    path = path.substr(0, path.find_last_of("/"));
  }
#endif
  return path;
}

class ArgosManagerApp : public wxApp {
public:
  virtual bool OnInit();
};

// Detect desktop environment
std::string DetectDE() {
  const char *xdg = std::getenv("XDG_CURRENT_DESKTOP");
  if (xdg) {
    std::string de(xdg);
    if (de.find("KDE") != std::string::npos)
      return "kde";
    if (de.find("GNOME") != std::string::npos)
      return "gnome";
    if (de.find("XFCE") != std::string::npos)
      return "xfce";
  }
  const char *session = std::getenv("DESKTOP_SESSION");
  if (session) {
    std::string s(session);
    if (s.find("plasma") != std::string::npos ||
        s.find("kde") != std::string::npos)
      return "kde";
    if (s.find("gnome") != std::string::npos)
      return "gnome";
  }
  return "unknown";
}

class MainFrame : public wxFrame {
public:
  MainFrame(const wxString &title);

private:
  void OnInstall(wxCommandEvent &event);
  void OnRemove(wxCommandEvent &event);
  void OnRefresh(wxCommandEvent &event);

  void RefreshPackageList();

  wxNotebook *notebook;
  wxListBox *installedList;
  wxListBox *availableList;
  wxTextCtrl *cmdPreview;

  std::string packagesDir;
  std::string currentShortcutId; // ID for current selection
  wxButton *btnSetShortcut;      // Reference to button

  struct PackageInfo {
    std::string name;
    std::string from_code;
    std::string to_code;
    std::string url;
  };
  std::vector<PackageInfo> availablePackages;

  void PopulateLanguageDropdowns();
  void OnLanguageSelected(wxCommandEvent &event);
  void OnSetShortcut(wxCommandEvent &event);
  void OnPackageSelected(wxCommandEvent &event);
  void OnCopyCommand(wxCommandEvent &event);
  void FetchRemotePackages(); // Helper to dl and parse index

  std::string GetLangName(const std::string &code);
  std::string DownloadFile(
      const std::string &url,
      const std::string &targetPath); // Returns error msg or empty on success
};

wxIMPLEMENT_APP(ArgosManagerApp);

bool ArgosManagerApp::OnInit() {
  MainFrame *frame = new MainFrame("Argos Translator Manager");
  frame->Show(true);
  return true;
}

MainFrame::MainFrame(const wxString &title)
    : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(600, 450)) {
  // Initialize network support
  wxSocketBase::Initialize();

  wxPanel *mainPanel = new wxPanel(this);
  wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

  notebook = new wxNotebook(mainPanel, wxID_ANY);

  // --- Tab 1: Installed ---
  wxPanel *pnlInstalled = new wxPanel(notebook);
  wxBoxSizer *sizerInstalled = new wxBoxSizer(wxVERTICAL);

  sizerInstalled->Add(
      new wxStaticText(pnlInstalled, wxID_ANY, "Installed Languages:"), 0,
      wxALL, 5);

  installedList = new wxListBox(pnlInstalled, wxID_ANY);
  sizerInstalled->Add(installedList, 1, wxEXPAND | wxALL, 5);

  // Command Preview Section
  wxBoxSizer *sizerCmd = new wxBoxSizer(wxHORIZONTAL);
  cmdPreview = new wxTextCtrl(pnlInstalled, wxID_ANY, "", wxDefaultPosition,
                              wxDefaultSize, wxTE_READONLY);
  btnSetShortcut = new wxButton(pnlInstalled, wxID_ANY, "Set Shortcut");

  sizerCmd->Add(new wxStaticText(pnlInstalled, wxID_ANY, "Command:"), 0,
                wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  sizerCmd->Add(cmdPreview, 1, wxEXPAND | wxRIGHT, 5);
  sizerCmd->Add(btnSetShortcut, 0, wxALIGN_CENTER_VERTICAL);

  sizerInstalled->Add(sizerCmd, 0, wxEXPAND | wxALL, 5);

  // Buttons
  wxBoxSizer *hboxBtns = new wxBoxSizer(wxHORIZONTAL);
  wxButton *btnRemove = new wxButton(pnlInstalled, wxID_ANY, "Remove Selected");
  wxButton *btnRefresh = new wxButton(pnlInstalled, wxID_ANY, "Refresh");
  hboxBtns->Add(btnRemove, 0, wxRIGHT, 5);
  hboxBtns->Add(btnRefresh, 0);

  sizerInstalled->Add(hboxBtns, 0, wxALIGN_CENTER | wxALL, 10);
  pnlInstalled->SetSizer(sizerInstalled);

  // --- Tab 2: Available (Remote) ---
  wxPanel *pnlAvailable = new wxPanel(notebook);
  wxBoxSizer *sizerAvailable = new wxBoxSizer(wxVERTICAL);

  sizerAvailable->Add(
      new wxStaticText(pnlAvailable, wxID_ANY, "Available Packages (Online):"),
      0, wxALL, 5);
  availableList = new wxListBox(pnlAvailable, wxID_ANY);
  sizerAvailable->Add(availableList, 1, wxEXPAND | wxALL, 5);

  wxButton *btnInstall =
      new wxButton(pnlAvailable, wxID_ANY, "Download & Install");
  sizerAvailable->Add(btnInstall, 0, wxALIGN_CENTER | wxALL, 10);

  pnlAvailable->SetSizer(sizerAvailable);

  // --- Add Tabs ---
  notebook->AddPage(pnlInstalled, "Installed");
  notebook->AddPage(pnlAvailable, "Available");

  mainSizer->Add(notebook, 1, wxEXPAND | wxALL, 5);
  mainPanel->SetSizer(mainSizer);

  // Bindings
  installedList->Bind(wxEVT_LISTBOX, &MainFrame::OnPackageSelected, this);
  btnSetShortcut->Bind(wxEVT_BUTTON, &MainFrame::OnSetShortcut, this);
  btnRemove->Bind(wxEVT_BUTTON, &MainFrame::OnRemove, this);
  btnRefresh->Bind(wxEVT_BUTTON, &MainFrame::OnRefresh, this);
  btnInstall->Bind(wxEVT_BUTTON, &MainFrame::OnInstall, this);

  // Determine packages dir
  std::string exeDir = get_exec_dir();
  packagesDir = exeDir + "/packages";

  // Fallback logic similar to CLI
  // Fallback logic similar to CLI
  if (!std::filesystem::exists(packagesDir)) {
    // Try up one level (build/ -> root/)
    std::string dev_packages = exeDir + "/../packages";
    if (std::filesystem::exists(dev_packages)) {
      packagesDir = dev_packages;
    } else {
      // Try current working directory
      if (std::filesystem::exists("packages")) {
        packagesDir = "packages";
      } else if (std::filesystem::exists("../packages")) {
        packagesDir = "../packages";
      }
    }
  }

  // Try to resolve to absolute path for clarity
  try {
    if (std::filesystem::exists(packagesDir)) {
      packagesDir = std::filesystem::canonical(packagesDir).string();
    }
  } catch (...) {
  } // Ignore errors

  RefreshPackageList();
  FetchRemotePackages(); // Trigger fetch on startup
}

void MainFrame::FetchRemotePackages() {
  availableList->Clear();
  availablePackages.clear();

  // 1. Download index.json using curl (simple system call)
  std::string index_path = "index.json";
  std::string cmd = "curl -s -L -o " + index_path +
                    " https://raw.githubusercontent.com/argosopentech/"
                    "argospm-index/main/index.json";

  // Use wxExecute or system. system is generic.
  if (system(cmd.c_str()) != 0) {
    availableList->AppendString("Error fetching package list.");
    return;
  }

  // 2. Parse JSON
  try {
    std::ifstream f(index_path);
    json index = json::parse(f);

    for (const auto &pkg : index) {
      std::string from = pkg.value("from_code", "??");
      std::string to = pkg.value("to_code", "??");
      auto links = pkg["links"];
      if (!links.empty()) {
        std::string url = links[0];
        std::string name = "translate-" + from + "_" + to;

        PackageInfo info = {name, from, to, url};
        availablePackages.push_back(info);

        std::string label = GetLangName(from) + " -> " + GetLangName(to);
        availableList->AppendString(label);
      }
    }
  } catch (const std::exception &e) {
    availableList->AppendString("Error parsing index.json");
    std::cerr << e.what() << std::endl;
  }
}

void MainFrame::RefreshPackageList() {
  installedList->Clear();
  int count = 0;

  if (std::filesystem::exists(packagesDir)) {
    for (const auto &entry : std::filesystem::directory_iterator(packagesDir)) {
      if (entry.is_directory()) {
        count++;
        std::string pkg_name = entry.path().filename().string();

        // Parse package name to get language codes
        std::string from_code, to_code;
        size_t translate_pos = pkg_name.find("translate-");
        if (translate_pos != std::string::npos) {
          std::string lang_part = pkg_name.substr(translate_pos + 10);
          size_t underscore = lang_part.find('_');
          if (underscore != std::string::npos) {
            from_code = lang_part.substr(0, underscore);
            size_t dash = lang_part.find('-', underscore);
            if (dash != std::string::npos) {
              to_code = lang_part.substr(underscore + 1, dash - underscore - 1);
            } else {
              to_code = lang_part.substr(underscore + 1);
            }
          }
        } else {
          size_t underscore = pkg_name.find('_');
          if (underscore != std::string::npos) {
            from_code = pkg_name.substr(0, underscore);
            to_code = pkg_name.substr(underscore + 1);
          }
        }

        // Display in readable format
        if (!from_code.empty() && !to_code.empty()) {
          std::string display =
              GetLangName(from_code) + " -> " + GetLangName(to_code);
          installedList->AppendString(display);
        } else {
          installedList->AppendString(pkg_name);
        }
      }
    }

    // Debug info removed

  } else {
    installedList->AppendString("No packages folder found.");
    wxMessageBox("Could not find 'packages' folder.\nLooked in: " + packagesDir,
                 "Error", wxOK | wxICON_ERROR);
  }
}

void MainFrame::OnPackageSelected(wxCommandEvent &event) {
  int sel = installedList->GetSelection();
  if (sel == wxNOT_FOUND)
    return;

  std::string display = installedList->GetString(sel).ToStdString();

  // Parse display format "English → Spanish" to get codes
  std::string from_code, to_code;

  // Find arrow separator
  size_t arrow_pos = display.find(" -> ");
  if (arrow_pos != std::string::npos) {
    std::string from_name = display.substr(0, arrow_pos);
    std::string to_name =
        display.substr(arrow_pos + 4); // Skip " -> " (4 characters)

    // Reverse lookup: find code from name
    auto findCode = [this](const std::string &name) -> std::string {
      static std::map<std::string, std::string> langs = {
          {"English", "en"},    {"Spanish", "es"},    {"French", "fr"},
          {"German", "de"},     {"Italian", "it"},    {"Portuguese", "pt"},
          {"Russian", "ru"},    {"Chinese", "zh"},    {"Japanese", "ja"},
          {"Korean", "ko"},     {"Hindi", "hi"},      {"Arabic", "ar"},
          {"Dutch", "nl"},      {"Polish", "pl"},     {"Turkish", "tr"},
          {"Ukrainian", "uk"},  {"Vietnamese", "vi"}, {"Indonesian", "id"},
          {"Catalan", "ca"},    {"Czech", "cs"},      {"Danish", "da"},
          {"Greek", "el"},      {"Esperanto", "eo"},  {"Estonian", "et"},
          {"Persian", "fa"},    {"Finnish", "fi"},    {"Irish", "ga"},
          {"Galician", "gl"},   {"Hebrew", "he"},     {"Hungarian", "hu"},
          {"Icelandic", "is"},  {"Georgian", "ka"},   {"Lithuanian", "lt"},
          {"Latvian", "lv"},    {"Macedonian", "mk"}, {"Malay", "ms"},
          {"Maltese", "mt"},    {"Norwegian", "nb"},  {"Romanian", "ro"},
          {"Slovak", "sk"},     {"Slovenian", "sl"},  {"Albanian", "sq"},
          {"Serbian", "sr"},    {"Swedish", "sv"},    {"Swahili", "sw"},
          {"Thai", "th"},       {"Tagalog", "tl"},    {"Bengali", "bn"},
          {"Burmese", "my"},    {"Gujarati", "gu"},   {"Kannada", "kn"},
          {"Malayalam", "ml"},  {"Marathi", "mr"},    {"Nepali", "ne"},
          {"Punjabi", "pa"},    {"Sinhala", "si"},    {"Tamil", "ta"},
          {"Telugu", "te"},     {"Urdu", "ur"},       {"Azerbaijani", "az"},
          {"Belarusian", "be"}, {"Bulgarian", "bg"},  {"Bosnian", "bs"},
          {"Welsh", "cy"},      {"Armenian", "hy"},   {"Kazakh", "kk"},
          {"Kyrgyz", "ky"},     {"Lao", "lo"},        {"Mongolian", "mn"},
          {"Pashto", "ps"},     {"Sindhi", "sd"},     {"Somali", "so"},
          {"Tajik", "tg"},      {"Turkmen", "tk"},    {"Uzbek", "uz"},
          {"Yiddish", "yi"},    {"Zulu", "zu"}};
      if (langs.count(name))
        return langs[name];
      return name; // Fallback
    };

    from_code = findCode(from_name);
    to_code = findCode(to_name);
  }

  // Generate command
  std::string exe = get_exec_dir() + "/Fast_translator";
  std::string cmd = exe + " " + from_code + ":" + to_code;
  cmdPreview->SetValue(cmd);

  // Store ID for shortcut
  currentShortcutId = from_code + "_" + to_code;
}

void MainFrame::OnSetShortcut(wxCommandEvent &event) {
  if (currentShortcutId.empty()) {
    wxMessageBox("Select a language pair first.", "Info", wxOK);
    return;
  }

  // Get current command
  std::string cmd = cmdPreview->GetValue().ToStdString();

  // Remove existing shortcut indicator if present
  size_t bracket = cmd.find(" [");
  if (bracket != std::string::npos) {
    cmd = cmd.substr(0, bracket);
  }

  // Copy command to clipboard
  if (wxTheClipboard->Open()) {
    wxTheClipboard->SetData(new wxTextDataObject(cmd));
    wxTheClipboard->Close();
  }

  // Detect desktop environment and open appropriate settings
  std::string de = DetectDE();
  std::string settingsCmd;
  std::string instructions;

  if (de == "kde") {
    settingsCmd = "systemsettings kcm_keys &";
    instructions = "KDE Keyboard Shortcuts will open.\n\n"
                   "1. Click 'Add New' -> 'Command or Script'\n"
                   "2. Paste the command (already copied)\n"
                   "3. Click 'Add custom shortcut' and press your keys\n"
                   "4. Click Apply";
  } else if (de == "gnome") {
    settingsCmd = "gnome-control-center keyboard &";
    instructions = "GNOME Keyboard Settings will open.\n\n"
                   "1. Scroll down to 'Custom Shortcuts'\n"
                   "2. Click '+' to add new shortcut\n"
                   "3. Name: 'Fast Translator'\n"
                   "4. Command: Paste (already copied)\n"
                   "5. Click 'Set Shortcut' and press your keys";
  } else if (de == "xfce") {
    settingsCmd = "xfce4-keyboard-settings &";
    instructions = "XFCE Keyboard Settings will open.\n\n"
                   "1. Go to 'Application Shortcuts' tab\n"
                   "2. Click 'Add'\n"
                   "3. Paste the command (already copied)\n"
                   "4. Press your desired shortcut keys";
  } else {
    wxMessageBox("Desktop environment not detected.\n\n"
                 "Command copied to clipboard:\n" +
                     cmd +
                     "\n\n"
                     "Please add this as a custom keyboard shortcut in your "
                     "system settings.",
                 "Manual Setup Required", wxOK | wxICON_INFORMATION);
    return;
  }

  wxMessageBox("Command copied to clipboard!\n\n" + instructions,
               "Set Keyboard Shortcut", wxOK | wxICON_INFORMATION);

  system(settingsCmd.c_str());
}

void MainFrame::OnInstall(wxCommandEvent &event) {
  int sel = availableList->GetSelection();
  if (sel == wxNOT_FOUND || sel >= availablePackages.size())
    return;

  const auto &pkg = availablePackages[sel];

  wxString msg =
      "Download and install " + pkg.name + "?\nThis may take a moment.";
  if (wxMessageBox(msg, "Confirm Install", wxYES_NO) != wxYES)
    return;

  // 1. Prepare
  if (!std::filesystem::exists(packagesDir)) {
    std::filesystem::create_directories(packagesDir);
  }

  std::string zipPath =
      packagesDir +
      "/temp_pkg.zip"; // Check permissions? better in /tmp or exe dir

  // 2. Download
  std::string error = DownloadFile(pkg.url, zipPath);
  if (!error.empty()) {
    wxMessageBox("Download failed:\n" + error, "Error", wxOK | wxICON_ERROR);
    return;
  }

  // 3. Unzip

  // 3. Unzip
  // argos models are zips that contain the model files. usually they unzip into
  // a folder or current dir? Usually they unzip into a folder named after the
  // package. logic varies. Let's unzip to packages/
  std::string cmdUnzip = "unzip -o " + zipPath + " -d " + packagesDir;
  if (system(cmdUnzip.c_str()) != 0) {
    wxEndBusyCursor();
    wxMessageBox("Extraction failed (install 'unzip' on system).", "Error",
                 wxOK | wxICON_ERROR);
    return;
  }

  // Cleanup
  std::filesystem::remove(zipPath);
  wxEndBusyCursor();

  wxMessageBox("Installed " + pkg.name, "Success", wxOK);
  RefreshPackageList();
}

void MainFrame::OnRemove(wxCommandEvent &event) {
  int sel = installedList->GetSelection();
  if (sel != wxNOT_FOUND) {
    wxString package = installedList->GetString(sel);
    int ans = wxMessageBox("Are you sure you want to remove " + package + "?",
                           "Confirm", wxYES_NO | wxICON_WARNING);
    if (ans == wxYES) {
      // TODO: Real logic
      wxMessageBox("Removal simulation: " + package + " would be deleted.",
                   "Info", wxOK);
    }
  }
}

void MainFrame::OnRefresh(wxCommandEvent &event) { RefreshPackageList(); }

std::string MainFrame::GetLangName(const std::string &code) {
  static std::map<std::string, std::string> langs = {
      {"en", "English"},    {"es", "Spanish"},    {"fr", "French"},
      {"de", "German"},     {"it", "Italian"},    {"pt", "Portuguese"},
      {"ru", "Russian"},    {"zh", "Chinese"},    {"ja", "Japanese"},
      {"ko", "Korean"},     {"hi", "Hindi"},      {"ar", "Arabic"},
      {"nl", "Dutch"},      {"pl", "Polish"},     {"tr", "Turkish"},
      {"uk", "Ukrainian"},  {"vi", "Vietnamese"}, {"id", "Indonesian"},
      {"ca", "Catalan"},    {"cs", "Czech"},      {"da", "Danish"},
      {"el", "Greek"},      {"eo", "Esperanto"},  {"et", "Estonian"},
      {"fa", "Persian"},    {"fi", "Finnish"},    {"ga", "Irish"},
      {"gl", "Galician"},   {"he", "Hebrew"},     {"hu", "Hungarian"},
      {"is", "Icelandic"},  {"ka", "Georgian"},   {"lt", "Lithuanian"},
      {"lv", "Latvian"},    {"mk", "Macedonian"}, {"ms", "Malay"},
      {"mt", "Maltese"},    {"nb", "Norwegian"},  {"ro", "Romanian"},
      {"sk", "Slovak"},     {"sl", "Slovenian"},  {"sq", "Albanian"},
      {"sr", "Serbian"},    {"sv", "Swedish"},    {"sw", "Swahili"},
      {"th", "Thai"},       {"tl", "Tagalog"},    {"bn", "Bengali"},
      {"my", "Burmese"},    {"gu", "Gujarati"},   {"kn", "Kannada"},
      {"ml", "Malayalam"},  {"mr", "Marathi"},    {"ne", "Nepali"},
      {"pa", "Punjabi"},    {"si", "Sinhala"},    {"ta", "Tamil"},
      {"te", "Telugu"},     {"ur", "Urdu"},       {"az", "Azerbaijani"},
      {"be", "Belarusian"}, {"bg", "Bulgarian"},  {"bs", "Bosnian"},
      {"cy", "Welsh"},      {"hy", "Armenian"},   {"kk", "Kazakh"},
      {"ky", "Kyrgyz"},     {"lo", "Lao"},        {"mn", "Mongolian"},
      {"no", "Norwegian"},  {"ps", "Pashto"},     {"sd", "Sindhi"},
      {"so", "Somali"},     {"tg", "Tajik"},      {"tk", "Turkmen"},
      {"uz", "Uzbek"},      {"yi", "Yiddish"},    {"zu", "Zulu"}};
  if (langs.count(code))
    return langs[code];
  return code; // Fallback
}

std::string MainFrame::DownloadFile(const std::string &url_str,
                                    const std::string &targetPath) {
  // Use curl for better HTTPS support and progress tracking
  std::string progressFile = "/tmp/curl_progress.txt";
  std::remove(progressFile.c_str());

  // Build curl command with progress output
  std::string cmd = "curl -L --progress-bar -o \"" + targetPath + "\" \"" +
                    url_str + "\" 2> " + progressFile + " &";

  // Start download in background
  int result = system(cmd.c_str());
  if (result != 0) {
    return "Failed to start download process.\nEnsure 'curl' is installed.";
  }

  // Show progress dialog
  wxProgressDialog dialog("Downloading", "Please wait...", 100, this,
                          wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_CAN_ABORT |
                              wxPD_ELAPSED_TIME);

  // Monitor download progress
  bool completed = false;
  bool cancelled = false;

  for (int i = 0; i < 300 && !completed; i++) { // Max 5 minutes
    wxMilliSleep(1000);                         // Check every second

    // Check if file exists and is growing
    if (std::filesystem::exists(targetPath)) {
      static size_t lastSize = 0;
      size_t currentSize = std::filesystem::file_size(targetPath);

      // Estimate progress (rough)
      int progress = std::min(95, (int)(i * 2)); // Fake progress, max 95%

      if (!dialog.Update(progress)) {
        cancelled = true;
        system("pkill -f curl"); // Kill curl process
        break;
      }

      // If file stopped growing, assume complete
      if (currentSize > 0 && currentSize == lastSize && i > 5) {
        completed = true;
      }
      lastSize = currentSize;
    }
  }

  // Final check
  if (cancelled) {
    std::filesystem::remove(targetPath);
    return "Download cancelled by user.";
  }

  if (!std::filesystem::exists(targetPath) ||
      std::filesystem::file_size(targetPath) == 0) {
    return "Download failed or file is empty.\nCheck your internet connection.";
  }

  dialog.Update(100);
  return ""; // Success
}
