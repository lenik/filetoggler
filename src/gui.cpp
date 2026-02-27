#include "gui.hpp"

#include "cli.hpp"
#include "core.hpp"

#include <wx/artprov.h>
#include <wx/dirctrl.h>
#include <wx/imaglist.h>
#include <wx/listctrl.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/tglbtn.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

enum {
    ID_ViewStop = wxID_HIGHEST + 1,
    ID_ViewReload,
    ID_ViewReset,
    ID_ViewShowHidden,
    ID_ViewShowBackup,
    ID_ArrangeManually,
    ID_ArrangeName,
    ID_ArrangeSize,
    ID_ArrangeSizeOnDisk,
    ID_ArrangeType,
    ID_ArrangeModDate,
    ID_ArrangeEmblems,
    ID_ArrangeExtension,
    ID_ArrangeCompactLayout,
    ID_ArrangeReversedOrder,
    ID_ViewIcons,
    ID_ViewList,
    ID_ViewCompact,
    ID_ProfileFirst,
    ID_ProfileAdd,
    ID_ProfileDelete,
};

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

            wxString part = wxString::FromUTF8(cmd[i].c_str());
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

enum class ViewMode { Icons, List, Compact };

class FileListCtrl : public wxListCtrl {
 public:
    explicit FileListCtrl(wxWindow* parent, const Config& cfg, MainFrame* frame)
        : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_HRULES | wxLC_VRULES),
            m_cfg(cfg), m_frame(frame) {
        m_baseFont = GetFont();
        setupImageList();
        setupColumns();

        Bind(wxEVT_LIST_ITEM_ACTIVATED, &FileListCtrl::OnActivate, this);
        Bind(wxEVT_LIST_ITEM_SELECTED, &FileListCtrl::OnSelectionChanged, this);
        Bind(wxEVT_LIST_ITEM_DESELECTED, &FileListCtrl::OnSelectionChanged, this);
        // Use wxEVT_CHAR so menu accelerators (F5, Ctrl+H/K, etc.) still work.
        Bind(wxEVT_CHAR, &FileListCtrl::OnCharHook, this);
        Bind(wxEVT_LIST_COL_CLICK, &FileListCtrl::OnColumnClick, this);
        Bind(wxEVT_LIST_ITEM_FOCUSED, &FileListCtrl::OnItemFocused, this);
        Bind(wxEVT_LIST_BEGIN_LABEL_EDIT, &FileListCtrl::OnBeginLabelEdit, this);
        Bind(wxEVT_LIST_END_LABEL_EDIT, &FileListCtrl::OnEndLabelEdit, this);

        m_typeTimer.Bind(wxEVT_TIMER, &FileListCtrl::OnTypeTimer, this);
        m_renameTimer.Bind(wxEVT_TIMER, &FileListCtrl::OnRenameTimer, this);
    }

    ~FileListCtrl() override {
        // Stop timers to avoid callbacks after destruction.
        m_typeTimer.Stop();
        m_renameTimer.Stop();
    }

    void setupImageList() {
        const int size = getIconSize();
        m_imageList = new wxImageList(size, size, true, 2);
        m_imageList->Add(wxArtProvider::GetBitmap(wxART_FOLDER, wxART_LIST, wxSize(size, size)));
        m_imageList->Add(wxArtProvider::GetBitmap(wxART_NORMAL_FILE, wxART_LIST, wxSize(size, size)));
        AssignImageList(m_imageList, wxIMAGE_LIST_NORMAL);
    }

    int getIconSize() const {
        int s = 32 + (m_iconZoom * 8);
        if (s < 16) s = 16;
        if (s > 96) s = 96;
        return s;
    }

    bool getShowHidden() const { return m_showHidden; }
    void setShowHidden(bool v) { m_showHidden = v; }
    bool getShowBackup() const { return m_showBackup; }
    void setShowBackup(bool v) { m_showBackup = v; }
    bool getCompactLayout() const { return m_compactLayout; }
    void setCompactLayout(bool v) { m_compactLayout = v; }
    bool getReversedOrder() const { return m_reversedOrder; }
    void setReversedOrder(bool v) { m_reversedOrder = v; }
    int getIconZoom() const { return m_iconZoom; }
    void setIconZoom(int z) { m_iconZoom = z; }
    int getSortColumn() const { return m_sortColumn; }
    bool getSortAscending() const { return m_sortAscending; }
    void setSortColumn(int c) { m_sortColumn = c; }
    void setSortAscending(bool a) { m_sortAscending = a; }
    void setArrangeBy(int col) { m_sortColumn = col; refreshEntries(); }
    void setReversedOrderAndRefresh(bool v) { m_reversedOrder = v; refreshEntries(); }
    void setCompactLayoutAndRefresh(bool v) { m_compactLayout = v; refreshEntries(); }
    void zoomIn() { if (m_iconZoom < 8) { m_iconZoom++; applyIconSize(); } }
    void zoomOut() { if (m_iconZoom > -2) { m_iconZoom--; applyIconSize(); } }
    void zoomReset() { m_iconZoom = 0; applyIconSize(); }
    void applyIconSize() {
        // Clamp zoom between -4x and +4x
        if (m_iconZoom > 4) m_iconZoom = 4;
        if (m_iconZoom < -4) m_iconZoom = -4;

        // Adjust font size for all view modes
        int basePt = m_baseFont.GetPointSize();
        if (basePt <= 0) basePt = 10;
        double factor = 1.0 + 0.25 * static_cast<double>(m_iconZoom);
        if (factor < 0.25) factor = 0.25;
        int newPt = static_cast<int>(basePt * factor);
        if (newPt < 4) newPt = 4;
        wxFont f = m_baseFont;
        f.SetPointSize(newPt);
        SetFont(f);

        // Recreate icons for icon/compact modes
        if (GetWindowStyleFlag() & wxLC_ICON) {
            setupImageList();
        }
        refreshEntries();
    }
    static bool isBackupName(const std::string& name) {
        if (name.empty()) return false;
        if (name.back() == '~') return true;
        size_t dot = name.rfind('.');
        if (dot != std::string::npos) {
            std::string ext = name.substr(dot + 1);
            if (ext == "bak" || ext == "swp" || ext == "orig" || ext == "backup") return true;
        }
        return false;
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
        printf("setDir: %s <- %s\n", m_dir.string().c_str(), dir.string().c_str());
        m_dir = dir;
        refreshEntries();
        updateStatusBar();
    }

    fs::path getDir() const { return m_dir; }

    void refreshEntries() {
        m_entries = list_dir_entries_with_disabled(m_dir, m_cfg);
        if (!m_showHidden) {
            m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                [](const FileEntry& e) { return !e.display_name.empty() && e.display_name[0] == '.'; }), m_entries.end());
        }
        if (!m_showBackup) {
            m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                [this](const FileEntry& e) { return isBackupName(e.display_name); }), m_entries.end());
        }
        sortEntries();
        if (m_reversedOrder) {
            std::reverse(m_entries.begin(), m_entries.end());
        }

        DeleteAllItems();
        
        long style = GetWindowStyleFlag();
        bool isReportMode = (style & wxLC_REPORT) != 0;
        bool isIconMode = (style & wxLC_ICON) != 0;
        bool isListMode = (style & wxLC_LIST) != 0;
        
        if (isReportMode) {
            updateColumnHeaders();
        }

        for (size_t i = 0; i < m_entries.size(); i++) {
            const auto& e = m_entries[i];
            
            wxString label;
            int imageIdx = e.is_dir ? 0 : 1;
            if (isIconMode || isListMode) {
                label = wxString::FromUTF8(e.display_name.c_str());
                long idx = InsertItem(static_cast<long>(i), label, imageIdx);
                (void)idx;
            } else {
                const char* stateIcon = (e.state == FileState::Disabled) ? "\xe2\x9c\x97 " : "\xe2\x9c\x93 ";
                const char* typeIcon = e.is_dir ? "\xf0\x9f\x93\x81 " : "\xf0\x9f\x93\x84 ";
                label = wxString::FromUTF8(stateIcon) + wxString::FromUTF8(typeIcon) + wxString::FromUTF8(e.display_name.c_str());
                long idx = InsertItem(static_cast<long>(i), label);

                // Column 1: Size
                if (e.is_dir) {
                    SetItem(idx, 1, "");
                } else {
                    SetItem(idx, 1, format_size(e.size));
                }
                SetItem(idx, 2, e.is_dir ? "Directory" : "File");
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

            long idx = static_cast<long>(i);
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
        m_renameTimer.Stop();
        long idx = evt.GetIndex();
        if (idx >= 0 && static_cast<size_t>(idx) < m_entries.size()) {
            const auto& e = m_entries[idx];
            if (e.is_dir) {
                printf("on activate: enabled_path: %s, m_dir: %s, display_name: %s\n", 
                    e.enabled_path.string().c_str(), m_dir.string().c_str(), e.display_name.c_str());

                // handleDirActivation(e.enabled_path);
                // Derive subdirectory from current directory and entry name to
                // avoid relying on potentially corrupted stored paths.
                fs::path subdir = m_dir / e.display_name;
                handleDirActivation(subdir);
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
        const bool ctrl = evt.ControlDown();
        const bool alt = evt.AltDown();

        if (code == WXK_RETURN && !ctrl && !alt) {
            EnableSelected(shift);
            return;
        }

        if (code == WXK_DELETE && !ctrl && !alt) {
            DisableSelected(shift);
            return;
        }

        if (code == WXK_SPACE && !ctrl && !alt) {
            ToggleSelected(shift);
            return;
        }

        if (code == WXK_F2 && !ctrl && !alt) {
            m_renameTimer.Stop();
            long focus = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
            if (focus >= 0 && static_cast<size_t>(focus) < m_entries.size()) {
                EditLabel(focus);
            }
            return;
        }

        // Type-to-find only for plain alphanumerics without modifiers
        if (!ctrl && !alt &&
            ((code >= '0' && code <= '9') || (code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z'))) {
            m_typeBuffer += static_cast<char>(code);
            m_typeTimer.StartOnce(700);
            JumpToPrefix(m_typeBuffer);
            return;
        }

        // Let unhandled keys propagate so accelerators work when list has focus
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
        if (col < 0) return;
        if (m_sortColumn == col) {
            m_sortAscending = !m_sortAscending;
        } else {
            m_sortColumn = col;
            m_sortAscending = true;
        }
        refreshEntries();
        updateColumnHeaders();
    }

    void OnItemFocused(wxListEvent& evt) {
        m_renameItemIndex = evt.GetIndex();
        m_renameTimer.StartOnce(1000);
    }

    void OnRenameTimer(wxTimerEvent&) {
        if (m_renameItemIndex >= 0 && static_cast<size_t>(m_renameItemIndex) < m_entries.size()) {
            EditLabel(static_cast<long>(m_renameItemIndex));
        }
        m_renameItemIndex = -1;
    }

    void OnBeginLabelEdit(wxListEvent& evt) {
        m_renameTimer.Stop();
        long idx = evt.GetIndex();
        if (idx >= 0 && static_cast<size_t>(idx) < m_entries.size()) {
            wxTextCtrl* edit = GetEditControl();
            if (edit) {
                edit->SetValue(wxString::FromUTF8(m_entries[idx].display_name.c_str()));
            }
        }
    }

    void OnEndLabelEdit(wxListEvent& evt) {
        if (evt.IsEditCancelled()) return;
        long idx = evt.GetIndex();
        if (idx < 0 || static_cast<size_t>(idx) >= m_entries.size()) return;
        std::string newName = evt.GetLabel().ToUTF8().data();
        if (newName.empty() || newName == m_entries[idx].display_name) return;
        std::string err;
        if (!rename_one(m_entries[idx].enabled_path, newName, m_cfg, &err)) {
            wxMessageBox(wxString::FromUTF8(err.c_str()), "Rename failed", wxOK | wxICON_WARNING, this);
            return;
        }
        refreshEntries();
        selectByName(newName);
    }

    static std::string getExtension(const std::string& name) {
        size_t dot = name.rfind('.');
        if (dot == std::string::npos) return "";
        return name.substr(dot + 1);
    }

    void sortEntries() {
        auto cmpStr = [this](const std::string& a, const std::string& b) {
            if (m_sortAscending) return a < b;
            return a > b;
        };
        auto cmpU64 = [this](std::uintmax_t a, std::uintmax_t b) {
            if (m_sortAscending) return a < b;
            return a > b;
        };
        auto cmpTime = [this](const fs::file_time_type& a, const fs::file_time_type& b) {
            if (m_sortAscending) return a < b;
            return a > b;
        };

        if (m_sortColumn >= 0) {
            std::sort(m_entries.begin(), m_entries.end(), [&](const FileEntry& a, const FileEntry& b) {
                switch (m_sortColumn) {
                    case 0: return cmpStr(a.display_name, b.display_name);
                    case 1: return cmpU64(a.size, b.size);
                    case 2: return cmpStr(a.is_dir ? "dir" : "file", b.is_dir ? "dir" : "file");
                    case 3: return cmpTime(a.mtime, b.mtime);
                    case 4: return cmpU64(a.size, b.size);  // Size on disk (use size)
                    case 5: return cmpStr(getExtension(a.display_name), getExtension(b.display_name));
                    case 6: return cmpStr(a.is_dir ? "dir" : "file", b.is_dir ? "dir" : "file");  // Emblems
                    default: return cmpStr(a.display_name, b.display_name);
                }
            });
        }
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
    bool m_showHidden{false};
    bool m_showBackup{false};
    bool m_compactLayout{false};
    bool m_reversedOrder{false};
    int m_iconZoom{0};
    wxFont m_baseFont;
    wxImageList* m_imageList{nullptr};

    wxTimer m_typeTimer{this};
    std::string m_typeBuffer;
    wxTimer m_renameTimer{this};
    int m_renameItemIndex{-1};
};

class MainFrame : public wxFrame {
 public:
    explicit MainFrame(const Config& cfg)
        : wxFrame(nullptr, wxID_ANY, "filetoggler", wxDefaultPosition, wxSize(1000, 700)),
            m_cfg(cfg) {
        m_splitter = new wxSplitterWindow(this, wxID_ANY);

        m_dirCtrl = new wxGenericDirCtrl(m_splitter, wxID_ANY, fs::current_path().string(),
                                         wxDefaultPosition, wxDefaultSize, wxDIRCTRL_DIR_ONLY);

        m_rightPanel = new wxPanel(m_splitter, wxID_ANY);
        auto* vbox = new wxBoxSizer(wxVERTICAL);

        auto* top = new wxBoxSizer(wxHORIZONTAL);
        top->AddStretchSpacer(1);

        m_btnIcon = new wxToggleButton(m_rightPanel, wxID_ANY, "Icon");
        m_btnList = new wxToggleButton(m_rightPanel, wxID_ANY, "List");
        m_btnCompact = new wxToggleButton(m_rightPanel, wxID_ANY, "Compact");

        m_btnList->SetValue(true);

        top->Add(m_btnIcon, 0, wxALL, 4);
        top->Add(m_btnList, 0, wxALL, 4);
        top->Add(m_btnCompact, 0, wxALL, 4);

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

        createMenuBar();

        // Initialize profiles for the starting directory
        refreshProfilesForCurrentDir();

        Bind(wxEVT_DIRCTRL_SELECTIONCHANGED, &MainFrame::OnDirChanged, this);
        Bind(wxEVT_CHAR_HOOK, &MainFrame::OnCharHook, this);

        m_btnIcon->Bind(wxEVT_TOGGLEBUTTON, &MainFrame::OnViewModeToggle, this);
        m_btnList->Bind(wxEVT_TOGGLEBUTTON, &MainFrame::OnViewModeToggle, this);
        m_btnCompact->Bind(wxEVT_TOGGLEBUTTON, &MainFrame::OnViewModeToggle, this);
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
        
        wxMenu* viewMenu = new wxMenu();
        viewMenu->Append(ID_ViewStop, "&Stop");
        viewMenu->Append(wxID_REFRESH, "&Reload\tF5");
        viewMenu->AppendSeparator();
        viewMenu->Append(ID_ViewReset, "&Reset view to defaults");
        viewMenu->Append(ID_ViewShowHidden, "Show &hidden files\tCtrl+H", "", wxITEM_CHECK);
        viewMenu->Append(ID_ViewShowBackup, "Show &backup files\tCtrl+K", "", wxITEM_CHECK);
        viewMenu->AppendSeparator();
        wxMenu* arrangeMenu = new wxMenu();
        arrangeMenu->AppendRadioItem(ID_ArrangeManually, "&Manually");
        arrangeMenu->AppendRadioItem(ID_ArrangeName, "&Name");
        arrangeMenu->AppendRadioItem(ID_ArrangeSize, "&Size");
        arrangeMenu->AppendRadioItem(ID_ArrangeSizeOnDisk, "Size on &disk");
        arrangeMenu->AppendRadioItem(ID_ArrangeType, "&Type");
        arrangeMenu->AppendRadioItem(ID_ArrangeModDate, "&Modification Date");
        arrangeMenu->AppendRadioItem(ID_ArrangeEmblems, "&Emblems");
        arrangeMenu->AppendRadioItem(ID_ArrangeExtension, "E&xtension");
        arrangeMenu->AppendSeparator();
        arrangeMenu->AppendCheckItem(ID_ArrangeCompactLayout, "&Compact Layout");
        arrangeMenu->AppendCheckItem(ID_ArrangeReversedOrder, "&Reversed Order");
        viewMenu->AppendSubMenu(arrangeMenu, "&Arrange Items");
        viewMenu->AppendSeparator();
        viewMenu->Append(wxID_ZOOM_IN, "Zoom &In\tCtrl++");
        viewMenu->Append(wxID_ZOOM_OUT, "Zoom &Out\tCtrl+-");
        viewMenu->Append(wxID_ZOOM_100, "&Normal size\tCtrl+0");
        viewMenu->AppendSeparator();
        viewMenu->AppendRadioItem(ID_ViewIcons, "&Icons\tCtrl+1");
        viewMenu->AppendRadioItem(ID_ViewList, "&List\tCtrl+2");
        viewMenu->AppendRadioItem(ID_ViewCompact, "&Compact\tCtrl+3");
        
        wxMenu* helpMenu = new wxMenu();
        helpMenu->Append(wxID_ANY, "&Keyboard Shortcuts");
        helpMenu->AppendSeparator();
        helpMenu->Append(wxID_ABOUT, "&About");
        
        menuBar->Append(fileMenu, "&File");
        menuBar->Append(editMenu, "&Edit");
        menuBar->Append(viewMenu, "&View");

        m_profileMenu = new wxMenu();
        menuBar->Append(m_profileMenu, "&Profile");

        menuBar->Append(helpMenu, "&Help");
        
        SetMenuBar(menuBar);
        
        viewMenu->Check(ID_ViewShowHidden, m_list->getShowHidden());
        viewMenu->Check(ID_ViewShowBackup, m_list->getShowBackup());
        viewMenu->Check(ID_ArrangeCompactLayout, m_list->getCompactLayout());
        viewMenu->Check(ID_ArrangeReversedOrder, m_list->getReversedOrder());
        arrangeMenu->Check(getArrangeIdForSortColumn(m_list->getSortColumn()), true);
        viewMenu->Check(ID_ViewList, true);
        
        Bind(wxEVT_MENU, &MainFrame::OnSelectFolder, this, wxID_OPEN);
        Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);
        Bind(wxEVT_MENU, &MainFrame::OnEnable, this, editMenu->FindItemByPosition(0)->GetId());
        Bind(wxEVT_MENU, &MainFrame::OnDisable, this, editMenu->FindItemByPosition(1)->GetId());
        Bind(wxEVT_MENU, &MainFrame::OnToggle, this, editMenu->FindItemByPosition(2)->GetId());
        Bind(wxEVT_MENU, &MainFrame::OnViewStop, this, ID_ViewStop);
        Bind(wxEVT_MENU, &MainFrame::OnViewReload, this, wxID_REFRESH);
        Bind(wxEVT_MENU, &MainFrame::OnViewReset, this, ID_ViewReset);
        Bind(wxEVT_MENU, &MainFrame::OnViewShowHidden, this, ID_ViewShowHidden);
        Bind(wxEVT_MENU, &MainFrame::OnViewShowBackup, this, ID_ViewShowBackup);
        Bind(wxEVT_MENU, &MainFrame::OnArrange, this, ID_ArrangeManually, ID_ArrangeReversedOrder);
        Bind(wxEVT_MENU, &MainFrame::OnZoomIn, this, wxID_ZOOM_IN);
        Bind(wxEVT_MENU, &MainFrame::OnZoomOut, this, wxID_ZOOM_OUT);
        Bind(wxEVT_MENU, &MainFrame::OnZoomNormal, this, wxID_ZOOM_100);
        Bind(wxEVT_MENU, &MainFrame::OnViewMode, this, ID_ViewIcons);
        Bind(wxEVT_MENU, &MainFrame::OnViewMode, this, ID_ViewList);
        Bind(wxEVT_MENU, &MainFrame::OnViewMode, this, ID_ViewCompact);
        Bind(wxEVT_MENU, &MainFrame::OnSelectProfile, this, ID_ProfileFirst, ID_ProfileFirst + 1000);
        Bind(wxEVT_MENU, &MainFrame::OnAddProfile, this, ID_ProfileAdd);
        Bind(wxEVT_MENU, &MainFrame::OnDeleteProfile, this, ID_ProfileDelete);
        Bind(wxEVT_MENU, &MainFrame::OnKeyboardShortcuts, this, helpMenu->FindItemByPosition(0)->GetId());
        Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);

        rebuildProfileMenu();
    }
    
    int getArrangeIdForSortColumn(int col) const {
        const int ids[] = { ID_ArrangeManually, ID_ArrangeName, ID_ArrangeSize, ID_ArrangeType,
            ID_ArrangeModDate, ID_ArrangeSizeOnDisk, ID_ArrangeExtension, ID_ArrangeEmblems };
        return col < 0 ? ids[0] : (col <= 6 ? ids[col + 1] : ID_ArrangeName);
    }
    int getSortColumnForArrangeId(int id) const {
        if (id == ID_ArrangeManually) return -1;
        if (id == ID_ArrangeName) return 0;
        if (id == ID_ArrangeSize) return 1;
        if (id == ID_ArrangeSizeOnDisk) return 4;
        if (id == ID_ArrangeType) return 2;
        if (id == ID_ArrangeModDate) return 3;
        if (id == ID_ArrangeEmblems) return 6;
        if (id == ID_ArrangeExtension) return 5;
        return 0;
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

    void OnViewStop(wxCommandEvent&) {
        // Stop could cancel an in-progress refresh; no async refresh yet
    }
    void OnViewReload(wxCommandEvent&) {
        m_list->refreshEntries();
        updateCurrentProfileFromDisabled();
    }
    void OnViewReset(wxCommandEvent&) {
        m_list->setShowHidden(false);
        m_list->setShowBackup(false);
        m_list->setSortColumn(0);
        m_list->setSortAscending(true);
        m_list->setCompactLayoutAndRefresh(false);
        m_list->setReversedOrderAndRefresh(false);
        m_list->zoomReset();
        setViewMode(ViewMode::List);
        m_list->refreshEntries();
        GetMenuBar()->Check(ID_ViewShowHidden, false);
        GetMenuBar()->Check(ID_ViewShowBackup, false);
        GetMenuBar()->Check(ID_ArrangeCompactLayout, false);
        GetMenuBar()->Check(ID_ArrangeReversedOrder, false);
        GetMenuBar()->Check(getArrangeIdForSortColumn(0), true);
        GetMenuBar()->Check(ID_ViewList, true);
        updateCurrentProfileFromDisabled();
    }
    void OnViewShowHidden(wxCommandEvent&) {
        m_list->setShowHidden(!m_list->getShowHidden());
        GetMenuBar()->Check(ID_ViewShowHidden, m_list->getShowHidden());
        m_list->refreshEntries();
        updateCurrentProfileFromDisabled();
    }
    void OnViewShowBackup(wxCommandEvent&) {
        m_list->setShowBackup(!m_list->getShowBackup());
        GetMenuBar()->Check(ID_ViewShowBackup, m_list->getShowBackup());
        m_list->refreshEntries();
        updateCurrentProfileFromDisabled();
    }
    void OnArrange(wxCommandEvent& evt) {
        int id = evt.GetId();
        if (id == ID_ArrangeCompactLayout) {
            m_list->setCompactLayoutAndRefresh(!m_list->getCompactLayout());
            GetMenuBar()->Check(ID_ArrangeCompactLayout, m_list->getCompactLayout());
            return;
        }
        if (id == ID_ArrangeReversedOrder) {
            m_list->setReversedOrderAndRefresh(!m_list->getReversedOrder());
            GetMenuBar()->Check(ID_ArrangeReversedOrder, m_list->getReversedOrder());
            return;
        }
        int col = getSortColumnForArrangeId(id);
        m_list->setSortColumn(col);
        m_list->setSortAscending(true);
        m_list->refreshEntries();
        m_list->updateColumnHeaders();
    }
    void OnZoomIn(wxCommandEvent&) { m_list->zoomIn(); }
    void OnZoomOut(wxCommandEvent&) { m_list->zoomOut(); }
    void OnZoomNormal(wxCommandEvent&) { m_list->zoomReset(); }
    void OnViewMode(wxCommandEvent& evt) {
        if (m_viewModeUpdating) return;
        switch (evt.GetId()) {
            case ID_ViewIcons:
                setViewMode(ViewMode::Icons);
                break;
            case ID_ViewList:
                setViewMode(ViewMode::List);
                break;
            case ID_ViewCompact:
                setViewMode(ViewMode::Compact);
                break;
            default:
                break;
        }
    }
    void OnSelectProfile(wxCommandEvent& evt) {
        int id = evt.GetId();
        if (id < ID_ProfileFirst) return;
        int index = id - ID_ProfileFirst;
        switchToProfile(index);
    }
    void OnAddProfile(wxCommandEvent&) {
        addProfileFromCurrentDisabled();
    }
    void OnDeleteProfile(wxCommandEvent&) {
        deleteCurrentProfile();
    }
    void setViewMode(ViewMode mode) {
        if (m_viewModeUpdating) return;
        if (m_viewMode == mode) return;
        m_viewModeUpdating = true;
        m_viewMode = mode;
        m_btnList->SetValue(mode == ViewMode::List);
        m_btnIcon->SetValue(mode == ViewMode::Icons);
        m_btnCompact->SetValue(mode == ViewMode::Compact);
        if (mode == ViewMode::List) {
            m_list->SetWindowStyleFlag(wxLC_REPORT | wxLC_HRULES | wxLC_VRULES);
            m_list->setupColumns();
        } else if (mode == ViewMode::Icons) {
            m_list->SetWindowStyleFlag(wxLC_ICON);
            m_list->DeleteAllColumns();
        } else {
            m_list->SetWindowStyleFlag(wxLC_LIST);
            m_list->DeleteAllColumns();
        }
        m_list->refreshEntries();
        GetMenuBar()->Check(ID_ViewIcons, mode == ViewMode::Icons);
        GetMenuBar()->Check(ID_ViewList, mode == ViewMode::List);
        GetMenuBar()->Check(ID_ViewCompact, mode == ViewMode::Compact);
        m_viewModeUpdating = false;
    }

    // --- Profile helpers ---
    fs::path currentProfileDir() const {
        fs::path dir = m_list->getDir();
        return dir / m_cfg.disabled_dir / "profile";
    }

    static std::vector<std::string> readProfileFile(const fs::path& path) {
        std::vector<std::string> lines;
        std::ifstream in(path);
        if (!in.is_open()) {
            return lines;
        }
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
        std::sort(lines.begin(), lines.end());
        return lines;
    }

    std::vector<std::string> currentDisabledFilesSorted() const {
        std::vector<std::string> out;
        auto entries = list_dir_entries_with_disabled(m_list->getDir(), m_cfg);
        for (const auto& e : entries) {
            if (!e.is_dir && e.state == FileState::Disabled) {
                out.push_back(e.display_name);
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    void rebuildProfileMenu() {
        if (!m_profileMenu) return;
        int count = m_profileMenu->GetMenuItemCount();
        for (int i = count - 1; i >= 0; --i) {
            wxMenuItem* item = m_profileMenu->FindItemByPosition(i);
            if (item) {
                m_profileMenu->Delete(item);
            }
        }
        for (size_t i = 0; i < m_profiles.size(); ++i) {
            int id = ID_ProfileFirst + static_cast<int>(i);
            auto* item = m_profileMenu->AppendRadioItem(id, wxString::FromUTF8(m_profiles[i].name.c_str()));
            item->Check(static_cast<int>(i) == m_currentProfileIndex);
        }
        if (!m_profiles.empty()) {
            m_profileMenu->AppendSeparator();
        }
        m_profileMenu->Append(ID_ProfileAdd, "Add Profile");
        m_profileMenu->Append(ID_ProfileDelete, "Delete Profile");
    }

    void updateWindowTitleForProfile() {
        fs::path dir = m_list->getDir();
        wxString title = "filetoggler - " + wxString::FromUTF8(dir.string().c_str());
        if (m_currentProfileIndex >= 0 && m_currentProfileIndex < static_cast<int>(m_profiles.size())) {
            title += " [" + wxString::FromUTF8(m_profiles[m_currentProfileIndex].name.c_str()) + "]";
        }
        SetTitle(title);
    }

    void updateCurrentProfileFromDisabled() {
        std::vector<std::string> disabled = currentDisabledFilesSorted();
        m_currentProfileIndex = -1;
        for (size_t i = 0; i < m_profiles.size(); ++i) {
            if (m_profiles[i].files == disabled) {
                m_currentProfileIndex = static_cast<int>(i);
                break;
            }
        }
        rebuildProfileMenu();
        updateWindowTitleForProfile();
    }

    void refreshProfilesForCurrentDir() {
        m_profiles.clear();
        m_currentProfileIndex = -1;
        fs::path root = currentProfileDir();
        std::error_code ec;
        if (fs::exists(root, ec) && fs::is_directory(root, ec)) {
            for (const auto& de : fs::directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
                if (ec) break;
                fs::path p = de.path();
                if (!fs::is_regular_file(p, ec)) continue;
                ProfileEntry pe;
                pe.name = p.filename().string();
                pe.files = readProfileFile(p);
                m_profiles.push_back(std::move(pe));
            }
            std::sort(m_profiles.begin(), m_profiles.end(), [](const ProfileEntry& a, const ProfileEntry& b) {
                return a.name < b.name;
            });
        }
        rebuildProfileMenu();
        updateCurrentProfileFromDisabled();
    }

    void addProfileFromCurrentDisabled() {
        std::vector<std::string> disabled = currentDisabledFilesSorted();
        if (disabled.empty()) return;
        fs::path root = currentProfileDir();
        std::error_code ec;
        fs::create_directories(root, ec);

        // Generate unique "Profile N" name
        std::set<std::string> existing;
        for (const auto& p : m_profiles) existing.insert(p.name);
        int n = 1;
        std::string name;
        do {
            name = "Profile " + std::to_string(n++);
        } while (existing.count(name) > 0);

        fs::path file = root / name;
        std::ofstream out(file);
        if (!out.is_open()) return;
        for (const auto& s : disabled) {
            out << s << "\n";
        }
        out.close();

        refreshProfilesForCurrentDir();
    }

    void deleteCurrentProfile() {
        if (m_currentProfileIndex < 0 || m_currentProfileIndex >= static_cast<int>(m_profiles.size())) return;
        fs::path root = currentProfileDir();
        fs::path file = root / m_profiles[m_currentProfileIndex].name;
        std::error_code ec;
        fs::remove(file, ec);
        refreshProfilesForCurrentDir();
    }

    void switchToProfile(int index) {
        if (index < 0 || index >= static_cast<int>(m_profiles.size())) return;
        const auto& prof = m_profiles[index];
        std::set<std::string> target(prof.files.begin(), prof.files.end());

        auto entries = list_dir_entries_with_disabled(m_list->getDir(), m_cfg);
        std::string err;

        // Enable files not in target
        for (const auto& e : entries) {
            if (e.is_dir) continue;
            bool shouldBeDisabled = target.count(e.display_name) > 0;
            if (!shouldBeDisabled && e.state == FileState::Disabled) {
                enable_one(e.enabled_path, m_cfg, &err);
            }
        }

        // Disable files that should be disabled
        for (const auto& e : entries) {
            if (e.is_dir) continue;
            bool shouldBeDisabled = target.count(e.display_name) > 0;
            if (shouldBeDisabled && e.state == FileState::Enabled) {
                disable_one(e.enabled_path, m_cfg, &err);
            }
        }

        m_list->refreshEntries();
        updateCurrentProfileFromDisabled();
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
            "  Alt+Up/Down - Switch profile\n\n"
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
        refreshProfilesForCurrentDir();
    }
    
    friend class FileListCtrl;
    
    void OnCharHook(wxKeyEvent& evt) {
        const int code = evt.GetKeyCode();
        const bool alt = evt.AltDown();
        
        if (alt && (code == WXK_UP || code == WXK_DOWN)) {
            if (!m_profiles.empty()) {
                int count = static_cast<int>(m_profiles.size());
                int idx = m_currentProfileIndex;
                if (idx < 0 || idx >= count) {
                    idx = 0;
                }
                int delta = (code == WXK_UP) ? -1 : 1;
                idx = (idx + delta + count) % count;
                switchToProfile(idx);
            }
            return;
        }
        
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
        refreshProfilesForCurrentDir();
    }

    void OnViewModeToggle(wxCommandEvent& evt) {
        if (m_viewModeUpdating) return;

        auto* btn = wxDynamicCast(evt.GetEventObject(), wxToggleButton);
        if (!btn) {
            return;
        }

        // Only react when a button is turned ON; turning the others OFF is a side effect.
        if (!btn->GetValue()) {
            return;
        }

        if (btn == m_btnList) {
            setViewMode(ViewMode::List);
        } else if (btn == m_btnIcon) {
            setViewMode(ViewMode::Icons);
        } else if (btn == m_btnCompact) {
            setViewMode(ViewMode::Compact);
        }
    }

    Config m_cfg;

    wxSplitterWindow* m_splitter{nullptr};
    wxGenericDirCtrl* m_dirCtrl{nullptr};
    wxPanel* m_rightPanel{nullptr};
    wxToggleButton* m_btnIcon{nullptr};
    wxToggleButton* m_btnList{nullptr};
    wxToggleButton* m_btnCompact{nullptr};
    FileListCtrl* m_list{nullptr};
    ViewMode m_viewMode{ViewMode::List};
    bool m_viewModeUpdating{false};
    
    wxMenu* m_profileMenu{nullptr};
    struct ProfileEntry {
        std::string name;
        std::vector<std::string> files;
    };
    std::vector<ProfileEntry> m_profiles;
    int m_currentProfileIndex{-1};
    
    std::vector<fs::path> m_dirHistory;
    int m_dirHistoryIndex{0};
    std::map<fs::path, fs::path> m_lastDirInParent;
};

void FileListCtrl::handleDirActivation(const fs::path& dir) {
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        return;
    }
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
                msg += "    " + wxString::FromUTF8(s.c_str()) + "\n";
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
