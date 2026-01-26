#include "gui.hpp"

#include "cli.hpp"
#include "core.hpp"

#include <wx/dirctrl.h>
#include <wx/listctrl.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/tglbtn.h>

#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace ft {

class MainFrame;

static std::string format_size(std::uintmax_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_idx < 4) {
        size /= 1024.0;
        unit_idx++;
    }
    
    char buf[64];
    if (unit_idx == 0) {
        snprintf(buf, sizeof(buf), "%'.0f %s", size, units[unit_idx]);
    } else {
        snprintf(buf, sizeof(buf), "%'.2f %s", size, units[unit_idx]);
    }
    return std::string(buf);
}

static bool relaunch_elevated(const Config&) {
    std::string exe = "filetoggler";
    std::string args = "--chdir \\\"" + fs::current_path().string() + "\\\"";

    const std::vector<std::vector<std::string>> candidates = {
        {"pkexec", exe, "--chdir", fs::current_path().string()},
        {"kdesu", "-c", exe + " --chdir \"" + fs::current_path().string() + "\""},
    };

    for (const auto& cmd : candidates) {
        wxString command;
        for (size_t i = 0; i < cmd.size(); i++) {
            if (i != 0) {
                command += " ";
            }

            wxString part = wxString::FromUTF8(cmd[i]);
            if (part.Find(' ') != wxNOT_FOUND || part.Find('"') != wxNOT_FOUND) {
                part.Replace("\"", "\\\"");
                command += "\"" + part + "\"";
            } else {
                command += part;
            }
        }

        long pid = wxExecute(command, wxEXEC_ASYNC);
        if (pid != 0) {
            return true;
        }
    }

    return false;
}

static std::vector<std::string> findInvalidFilesForGui(const std::vector<std::string>& files, const Config& cfg) {
        std::vector<std::string> invalid;
        std::error_code ec;
        for (const auto& f : files) {
                fs::path p = f;

                if (fs::exists(p, ec)) {
                        continue;
                }

                fs::path dp = disabled_path_for(p, cfg);
                if (fs::exists(dp, ec)) {
                        continue;
                }

                invalid.push_back(f);
        }
        return invalid;
}

class FileListCtrl : public wxListCtrl {
 public:
    explicit FileListCtrl(wxWindow* parent, const Config& cfg, MainFrame* frame)
        : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_HRULES | wxLC_VRULES),
            m_cfg(cfg), m_frame(frame) {
        setupColumns();

        Bind(wxEVT_LIST_ITEM_ACTIVATED, &FileListCtrl::OnActivate, this);
        Bind(wxEVT_LIST_ITEM_SELECTED, &FileListCtrl::OnSelectionChanged, this);
        Bind(wxEVT_LIST_ITEM_DESELECTED, &FileListCtrl::OnSelectionChanged, this);
        Bind(wxEVT_CHAR_HOOK, &FileListCtrl::OnCharHook, this);
        Bind(wxEVT_LIST_COL_CLICK, &FileListCtrl::OnColumnClick, this);

        m_typeTimer.Bind(wxEVT_TIMER, &FileListCtrl::OnTypeTimer, this);
    }

    void setupColumns() {
        DeleteAllColumns();
        
        wxFont headerFont = GetFont();
        headerFont.MakeBold();
        
        InsertColumn(0, "Name", wxLIST_FORMAT_LEFT, 350);
        InsertColumn(1, "Size", wxLIST_FORMAT_RIGHT, 100);
        InsertColumn(2, "Type", wxLIST_FORMAT_LEFT, 80);
        InsertColumn(3, "Last Modified", wxLIST_FORMAT_LEFT, 180);
        
        updateColumnHeaders();
    }
    
    void updateColumnHeaders() {
        const char* arrows[] = {"▲", "▼"};
        const char* labels[] = {"Name", "Size", "Type", "Last Modified"};
        
        for (int i = 0; i < 4; i++) {
            wxString header = labels[i];
            if (i == m_sortColumn) {
                header += " ";
                header += arrows[m_sortAscending ? 0 : 1];
            }
            
            wxListItem item;
            item.SetMask(wxLIST_MASK_TEXT);
            item.SetText(header);
            SetColumn(i, item);
        }
    }

    void setDir(const fs::path& dir) {
        m_dir = dir;
        refreshEntries();
        updateStatusBar();
    }

    fs::path getDir() const { return m_dir; }

    void refreshEntries() {
        m_entries = list_dir_entries_with_disabled(m_dir, m_cfg);
        sortEntries();

        DeleteAllItems();
        
        long style = GetWindowStyleFlag();
        bool isReportMode = (style & wxLC_REPORT) != 0;
        
        if (isReportMode) {
            updateColumnHeaders();
        }

        for (size_t i = 0; i < m_entries.size(); i++) {
            const auto& e = m_entries[i];
            
            // Use proper UTF-8 encoding for unicode characters
            const char* stateIcon = (e.state == FileState::Disabled) ? "\xe2\x9c\x97 " : "\xe2\x9c\x93 ";
            const char* typeIcon = e.is_dir ? "\xf0\x9f\x93\x81 " : "\xf0\x9f\x93\x84 ";
            wxString displayName = wxString::FromUTF8(stateIcon) + wxString::FromUTF8(typeIcon) + e.display_name;
            
            long idx = InsertItem(i, displayName);

            if (isReportMode) {
                // Column 1: Size
                if (e.is_dir) {
                    SetItem(idx, 1, "");
                } else {
                    SetItem(idx, 1, format_size(e.size));
                }
                
                // Column 2: Type
                SetItem(idx, 2, e.is_dir ? "Directory" : "File");
                
                // Column 3: Last Modified
                wxString mtime_str;
                {
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        e.mtime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                    std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
                    mtime_str = wxString::FromUTF8(std::string(std::ctime(&tt)).c_str());
                    mtime_str.Trim(true);
                    mtime_str.Trim(false);
                }
                SetItem(idx, 3, mtime_str);
            }

            if (e.state == FileState::Disabled) {
                SetItemTextColour(idx, wxColour(160, 160, 160));
            }
        }
        
        updateStatusBar();
    }

    void EnableSelected(bool backward) {
        DoActionOnSelected(Action::Enable, backward);
    }

    void DisableSelected(bool backward) {
        DoActionOnSelected(Action::Disable, backward);
    }

    void ToggleSelected(bool backward) {
        DoActionOnSelected(Action::Toggle, backward);
    }
    
    void updateStatusBar();
    
    std::vector<FileEntry> getSelectedEntries() {
        std::vector<FileEntry> result;
        auto indices = GetSelectedIndices();
        for (long idx : indices) {
            if (idx >= 0 && static_cast<size_t>(idx) < m_entries.size()) {
                result.push_back(m_entries[idx]);
            }
        }
        return result;
    }
    
    void selectByName(const std::string& name) {
        for (long i = 0; i < GetItemCount(); i++) {
            if (i >= 0 && static_cast<size_t>(i) < m_entries.size()) {
                const auto& e = m_entries[i];
                if (e.display_name == name) {
                    selectSingle(i);
                    EnsureVisible(i);
                    break;
                }
            }
        }
    }

 private:
    void OnActivate(wxListEvent& evt) {
        long idx = evt.GetIndex();
        if (idx >= 0 && static_cast<size_t>(idx) < m_entries.size()) {
            const auto& e = m_entries[idx];
            if (e.is_dir) {
                handleDirActivation(e.enabled_path);
                return;
            }
        }
        ToggleSelected(false);
        evt.Skip();
    }
    
    void handleDirActivation(const fs::path& dir);

    void OnCharHook(wxKeyEvent& evt) {
        const int code = evt.GetKeyCode();
        const bool shift = evt.ShiftDown();

        if (code == WXK_RETURN) {
            EnableSelected(shift);
            return;
        }

        if (code == WXK_DELETE) {
            DisableSelected(shift);
            return;
        }

        if (code == WXK_SPACE) {
            ToggleSelected(shift);
            return;
        }

        if ((code >= '0' && code <= '9') || (code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z')) {
            m_typeBuffer += static_cast<char>(code);
            m_typeTimer.StartOnce(700);
            JumpToPrefix(m_typeBuffer);
            return;
        }

        evt.Skip();
    }
    
    void OnSelectionChanged(wxListEvent&) {
        updateStatusBar();
    }

    void OnTypeTimer(wxTimerEvent&) {
        m_typeBuffer.clear();
    }

    void OnColumnClick(wxListEvent& evt) {
        const int col = evt.GetColumn();
        if (col < 0) {
            return;
        }

        if (m_sortColumn == col) {
            m_sortAscending = !m_sortAscending;
        } else {
            m_sortColumn = col;
            m_sortAscending = true;
        }

        refreshEntries();
        updateColumnHeaders();
    }

    void sortEntries() {
        auto cmpStr = [this](const std::string& a, const std::string& b) {
            if (m_sortAscending) {
                return a < b;
            }
            return a > b;
        };

        auto cmpU64 = [this](std::uintmax_t a, std::uintmax_t b) {
            if (m_sortAscending) {
                return a < b;
            }
            return a > b;
        };

        auto cmpTime = [this](const fs::file_time_type& a, const fs::file_time_type& b) {
            if (m_sortAscending) {
                return a < b;
            }
            return a > b;
        };

        std::sort(m_entries.begin(), m_entries.end(), [&](const FileEntry& a, const FileEntry& b) {
            switch (m_sortColumn) {
                case 0:
                    return cmpStr(a.display_name, b.display_name);
                case 1:
                    return cmpU64(a.size, b.size);
                case 2:
                    return cmpStr(a.is_dir ? "dir" : "file", b.is_dir ? "dir" : "file");
                case 3:
                    return cmpTime(a.mtime, b.mtime);
                default:
                    return cmpStr(a.display_name, b.display_name);
            }
        });
    }

    void JumpToPrefix(const std::string& prefix) {
        if (m_entries.empty()) {
            return;
        }

        for (long i = 0; i < GetItemCount(); i++) {
            if (i >= 0 && static_cast<size_t>(i) < m_entries.size()) {
                const auto& e = m_entries[i];
                if (e.display_name.rfind(prefix, 0) == 0) {
                    selectSingle(i);
                    EnsureVisible(i);
                    break;
                }
            }
        }
    }

    std::vector<long> GetSelectedIndices() {
        std::vector<long> sel;
        long item = -1;
        for (;;) {
            item = GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
            if (item == -1) {
                break;
            }
            sel.push_back(item);
        }
        return sel;
    }

    void selectSingle(long idx) {
        for (long i = 0; i < GetItemCount(); i++) {
            SetItemState(i, 0, wxLIST_STATE_SELECTED);
            SetItemState(i, 0, wxLIST_STATE_FOCUSED);
        }
        if (idx >= 0 && idx < GetItemCount()) {
            SetItemState(idx, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
            SetItemState(idx, wxLIST_STATE_FOCUSED, wxLIST_STATE_FOCUSED);
            SetFocus();
            EnsureVisible(idx);
        }
    }

    void DoActionOnSelected(Action act, bool backward) {
        auto sel = GetSelectedIndices();
        if (sel.empty()) {
            return;
        }

        long first = sel.front();
        long last = sel.back();

        std::string err;
        bool perm_error = false;

        for (long idx : sel) {
            if (idx < 0 || static_cast<size_t>(idx) >= m_entries.size()) {
                continue;
            }
            const auto& e = m_entries[idx];
            bool ok = false;

            try {
                switch (act) {
                    case Action::Enable:
                        ok = enable_one(e.enabled_path, m_cfg, &err);
                        break;
                    case Action::Disable:
                        ok = disable_one(e.enabled_path, m_cfg, &err);
                        break;
                    case Action::Toggle:
                        ok = toggle_one(e.enabled_path, m_cfg, &err);
                        break;
                    case Action::None:
                        ok = true;
                        break;
                }
            } catch (const fs::filesystem_error& fe) {
                if (fe.code() == std::errc::permission_denied) {
                    perm_error = true;
                }
                err = fe.what();
                ok = false;
            }

            if (!ok) {
                break;
            }
        }

        if (perm_error) {
            if (relaunch_elevated(m_cfg)) {
                wxTheApp->ExitMainLoop();
                return;
            }
        }

        refreshEntries();

        long next;
        if (backward) {
            next = first - 1;
        } else {
            next = last + 1;
        }

        if (next < 0) {
            next = 0;
        }
        if (next >= GetItemCount()) {
            next = GetItemCount() - 1;
        }

        selectSingle(next);
    }

    Config m_cfg;
    MainFrame* m_frame;
    fs::path m_dir;
    std::vector<FileEntry> m_entries;

    int m_sortColumn{0};
    bool m_sortAscending{true};

    wxTimer m_typeTimer{this};
    std::string m_typeBuffer;
};

class MainFrame : public wxFrame {
 public:
    explicit MainFrame(const Config& cfg)
        : wxFrame(nullptr, wxID_ANY, "filetoggler", wxDefaultPosition, wxSize(1000, 700)),
            m_cfg(cfg) {
        
        createMenuBar();
        
        m_splitter = new wxSplitterWindow(this, wxID_ANY);

        m_dirCtrl = new wxGenericDirCtrl(m_splitter, wxID_ANY, fs::current_path().string(),
                                         wxDefaultPosition, wxDefaultSize, wxDIRCTRL_DIR_ONLY);

        m_rightPanel = new wxPanel(m_splitter, wxID_ANY);
        auto* vbox = new wxBoxSizer(wxVERTICAL);

        auto* top = new wxBoxSizer(wxHORIZONTAL);
        top->AddStretchSpacer(1);

        m_btnList = new wxToggleButton(m_rightPanel, wxID_ANY, "List");
        m_btnIcon = new wxToggleButton(m_rightPanel, wxID_ANY, "Icon");

        m_btnList->SetValue(true);

        top->Add(m_btnList, 0, wxALL, 4);
        top->Add(m_btnIcon, 0, wxALL, 4);

        m_list = new FileListCtrl(m_rightPanel, m_cfg, this);
        m_list->setDir(fs::current_path());
        
        m_dirHistory.push_back(fs::current_path());
        m_dirHistoryIndex = 0;

        vbox->Add(top, 0, wxEXPAND);
        vbox->Add(m_list, 1, wxEXPAND);
        m_rightPanel->SetSizer(vbox);

        m_splitter->SplitVertically(m_dirCtrl, m_rightPanel, 280);

        CreateStatusBar();
        SetStatusText("Ready");

        Bind(wxEVT_DIRCTRL_SELECTIONCHANGED, &MainFrame::OnDirChanged, this);
        Bind(wxEVT_CHAR_HOOK, &MainFrame::OnCharHook, this);

        m_btnList->Bind(wxEVT_TOGGLEBUTTON, &MainFrame::OnViewToggle, this);
        m_btnIcon->Bind(wxEVT_TOGGLEBUTTON, &MainFrame::OnViewToggle, this);
    }
    
    void updateStatusBar(const wxString& text) {
        if (GetStatusBar()) {
            SetStatusText(text);
        }
    }

 private:
    void createMenuBar() {
        wxMenuBar* menuBar = new wxMenuBar();
        
        wxMenu* fileMenu = new wxMenu();
        fileMenu->Append(wxID_OPEN, "Select &Folder...\tCtrl+O");
        fileMenu->AppendSeparator();
        fileMenu->Append(wxID_EXIT, "E&xit\tCtrl+Q");
        
        wxMenu* editMenu = new wxMenu();
        editMenu->Append(wxID_ANY, "&Enable\tEnter");
        editMenu->Append(wxID_ANY, "&Disable\tDelete");
        editMenu->Append(wxID_ANY, "&Toggle\tSpace");
        
        wxMenu* helpMenu = new wxMenu();
        helpMenu->Append(wxID_ANY, "&Keyboard Shortcuts");
        helpMenu->AppendSeparator();
        helpMenu->Append(wxID_ABOUT, "&About");
        
        menuBar->Append(fileMenu, "&File");
        menuBar->Append(editMenu, "&Edit");
        menuBar->Append(helpMenu, "&Help");
        
        SetMenuBar(menuBar);
        
        Bind(wxEVT_MENU, &MainFrame::OnSelectFolder, this, wxID_OPEN);
        Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);
        Bind(wxEVT_MENU, &MainFrame::OnEnable, this, editMenu->FindItemByPosition(0)->GetId());
        Bind(wxEVT_MENU, &MainFrame::OnDisable, this, editMenu->FindItemByPosition(1)->GetId());
        Bind(wxEVT_MENU, &MainFrame::OnToggle, this, editMenu->FindItemByPosition(2)->GetId());
        Bind(wxEVT_MENU, &MainFrame::OnKeyboardShortcuts, this, helpMenu->FindItemByPosition(0)->GetId());
        Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);
    }
    
    void OnSelectFolder(wxCommandEvent&) {
        wxDirDialog dlg(this, "Select Folder", m_list->getDir().string());
        if (dlg.ShowModal() == wxID_OK) {
            navigateToDir(fs::path(dlg.GetPath().ToStdString()), true);
        }
    }
    
    void OnExit(wxCommandEvent&) {
        Close(true);
    }
    
    void OnEnable(wxCommandEvent&) {
        m_list->EnableSelected(false);
    }
    
    void OnDisable(wxCommandEvent&) {
        m_list->DisableSelected(false);
    }
    
    void OnToggle(wxCommandEvent&) {
        m_list->ToggleSelected(false);
    }
    
    void OnKeyboardShortcuts(wxCommandEvent&) {
        wxString msg = 
            "File Operations:\n"
            "  Enter - Enable selected files\n"
            "  Delete - Disable selected files\n"
            "  Space - Toggle selected files\n"
            "  Shift+Enter/Delete/Space - Same but select previous\n\n"
            "Navigation:\n"
            "  Alt+Left - Go back\n"
            "  Alt+Right - Go forward\n"
            "  Alt+Up - Go to parent directory\n"
            "  Alt+Down - Open selected directory\n\n"
            "Search:\n"
            "  Type alphanumeric - Find by prefix\n\n"
            "Selection:\n"
            "  Ctrl+Click - Multi-select\n"
            "  Shift+Click - Range select";
        wxMessageBox(msg, "Keyboard Shortcuts", wxOK | wxICON_INFORMATION, this);
    }
    
    void OnAbout(wxCommandEvent&) {
        wxMessageBox("filetoggler v1.0\n\nA dual-mode file toggler for quickly enabling/disabling files.",
                     "About filetoggler", wxOK | wxICON_INFORMATION, this);
    }
    
    void navigateToDir(const fs::path& dir, bool addToHistory) {
        if (addToHistory) {
            if (m_dirHistoryIndex < static_cast<int>(m_dirHistory.size()) - 1) {
                m_dirHistory.erase(m_dirHistory.begin() + m_dirHistoryIndex + 1, m_dirHistory.end());
            }
            m_dirHistory.push_back(dir);
            m_dirHistoryIndex = static_cast<int>(m_dirHistory.size()) - 1;
        }
        
        m_dirCtrl->SetPath(dir.string());
        m_list->setDir(dir);
    }
    
    friend class FileListCtrl;
    
    void OnCharHook(wxKeyEvent& evt) {
        const int code = evt.GetKeyCode();
        const bool alt = evt.AltDown();
        
        if (alt && code == WXK_LEFT) {
            if (m_dirHistoryIndex > 0) {
                m_dirHistoryIndex--;
                navigateToDir(m_dirHistory[m_dirHistoryIndex], false);
            }
            return;
        }
        
        if (alt && code == WXK_RIGHT) {
            if (m_dirHistoryIndex < static_cast<int>(m_dirHistory.size()) - 1) {
                m_dirHistoryIndex++;
                navigateToDir(m_dirHistory[m_dirHistoryIndex], false);
            }
            return;
        }
        
        if (alt && code == WXK_UP) {
            fs::path current = m_list->getDir();
            if (current.has_parent_path() && current != current.parent_path()) {
                fs::path parent = current.parent_path();
                std::string currentDirName = current.filename().string();
                navigateToDir(parent, true);
                m_list->selectByName(currentDirName);
            }
            return;
        }
        
        if (alt && code == WXK_DOWN) {
            auto selected = m_list->getSelectedEntries();
            if (selected.size() == 1 && selected[0].is_dir) {
                fs::path targetDir = selected[0].enabled_path;
                navigateToDir(targetDir, true);
            }
            return;
        }
        
        evt.Skip();
    }
    
    void OnDirChanged(wxTreeEvent&) {
        wxString path = m_dirCtrl->GetPath();
        fs::path newDir = fs::path(path.ToStdString());
        
        if (m_lastDirInParent.count(newDir)) {
            m_list->setDir(newDir);
        } else {
            navigateToDir(newDir, true);
        }
    }

    void OnViewToggle(wxCommandEvent& evt) {
        if (evt.GetEventObject() == m_btnList) {
            m_btnList->SetValue(true);
            m_btnIcon->SetValue(false);
            m_list->SetWindowStyleFlag(wxLC_REPORT | wxLC_HRULES | wxLC_VRULES);
            m_list->setupColumns();
            m_list->refreshEntries();
            return;
        }

        if (evt.GetEventObject() == m_btnIcon) {
            m_btnList->SetValue(false);
            m_btnIcon->SetValue(true);
            m_list->SetWindowStyleFlag(wxLC_ICON);
            m_list->DeleteAllColumns();
            m_list->refreshEntries();
            return;
        }
    }

    Config m_cfg;

    wxSplitterWindow* m_splitter{nullptr};
    wxGenericDirCtrl* m_dirCtrl{nullptr};
    wxPanel* m_rightPanel{nullptr};
    wxToggleButton* m_btnList{nullptr};
    wxToggleButton* m_btnIcon{nullptr};
    FileListCtrl* m_list{nullptr};
    
    std::vector<fs::path> m_dirHistory;
    int m_dirHistoryIndex{0};
    std::map<fs::path, fs::path> m_lastDirInParent;
};

void FileListCtrl::handleDirActivation(const fs::path& dir) {
    m_frame->navigateToDir(dir, true);
}

void FileListCtrl::updateStatusBar() {
    if (!m_frame) return;
    
    auto selected = getSelectedEntries();
    if (selected.empty()) {
        m_frame->updateStatusBar(wxString::Format("%zu items", m_entries.size()));
    } else if (selected.size() == 1) {
        const auto& e = selected[0];
        wxString state = (e.state == FileState::Disabled) ? " (disabled)" : "";
        if (e.is_dir) {
            m_frame->updateStatusBar(e.display_name + state + " - Directory");
        } else {
            m_frame->updateStatusBar(wxString::Format("%s%s - %s", 
                e.display_name, state, format_size(e.size)));
        }
    } else {
        std::uintmax_t totalSize = 0;
        int fileCount = 0;
        int dirCount = 0;
        for (const auto& e : selected) {
            if (e.is_dir) {
                dirCount++;
            } else {
                fileCount++;
                totalSize += e.size;
            }
        }
        wxString msg = wxString::Format("%d items selected", static_cast<int>(selected.size()));
        if (fileCount > 0) {
            msg += wxString::Format(" (%d files, %s)", fileCount, format_size(totalSize));
        }
        if (dirCount > 0) {
            msg += wxString::Format(" (%d dirs)", dirCount);
        }
        m_frame->updateStatusBar(msg);
    }
}

class App : public wxApp {
 public:
    explicit App(const Config& cfg, const std::vector<std::string>& files) : m_cfg(cfg), m_files(files) {}

    bool OnInit() override {
        auto* f = new MainFrame(m_cfg);
        f->Show(true);

        const auto invalid = findInvalidFilesForGui(m_files, m_cfg);
        if (!invalid.empty()) {
            wxString msg = "Invalid filenames:\n";
            for (const auto& s : invalid) {
                msg += "    " + wxString::FromUTF8(s) + "\n";
            }
            wxMessageBox(msg, "filetoggler", wxOK | wxICON_WARNING, f);
        }

        return true;
    }

 private:
    Config m_cfg;
    std::vector<std::string> m_files;
};

int run_gui(const Config& cfg, const std::vector<std::string>& files) {
    wxApp::SetInstance(new App(cfg, files));
    int argc = 0;
    char** argv = nullptr;
    wxEntryStart(argc, argv);
    wxTheApp->CallOnInit();
    wxTheApp->OnRun();
    wxTheApp->OnExit();
    wxEntryCleanup();
    return 0;
}

}
