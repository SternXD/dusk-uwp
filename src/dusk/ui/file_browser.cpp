#include "file_browser.hpp"

#include "dusk/app_info.hpp"
#include "dusk/data.hpp"
#include "dusk/io.hpp"
#include "prelaunch.hpp"
#include "settings.hpp"
#include "ui.hpp"

#include <aurora/rmlui.hpp>
#include <fmt/format.h>

#include <SDL3/SDL_filesystem.h>

#include <Z2AudioLib/Z2SeMgr.h>
#include <m_Do/m_Do_audio.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace dusk::ui {
namespace {

bool is_disc_extension(const std::filesystem::path& path) {
    std::string ext = dusk::io::fs_path_to_string(path.extension());
    if (ext.size() > 1 && ext.front() == '.') {
        ext.erase(ext.begin());
    }
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    static constexpr const char* kKnown[] = {
        "iso",
        "gcm",
        "ciso",
        "gcz",
        "nfs",
        "rvz",
        "wbfs",
        "wia",
        "tgc",
    };
    for (const char* k : kKnown) {
        if (ext == k) {
            return true;
        }
    }
    return false;
}

std::filesystem::path normalize_existing_dir(const std::filesystem::path& p) {
    std::error_code ec;
    if (p.empty() || !std::filesystem::exists(p, ec) || !std::filesystem::is_directory(p, ec)) {
        return {};
    }
    std::filesystem::path out =
        std::filesystem::weakly_canonical(std::filesystem::absolute(p, ec), ec);
    return out.empty() ? std::filesystem::path{} : out;
}

std::filesystem::path initial_browser_directory() {
    const auto& configured = prelaunch_state().configuredDiscPath;
    if (!configured.empty()) {
        const std::filesystem::path p = dusk::io::path_from_utf8(configured);
        if (auto dir = normalize_existing_dir(p.parent_path()); !dir.empty()) {
            return dir;
        }
        if (auto dir = normalize_existing_dir(p); !dir.empty()) {
            return dir;
        }
    }

    if (const char* base = SDL_GetBasePath()) {
        const std::filesystem::path b = dusk::io::path_from_utf8(base);
        if (auto dir = normalize_existing_dir(b); !dir.empty()) {
            return dir;
        }
        if (!b.empty()) {
            std::error_code ecAbs;
            std::filesystem::path abs = std::filesystem::absolute(b, ecAbs);
            if (!abs.empty()) {
                return abs;
            }
        }
    }

    if (char* pref = SDL_GetPrefPath(dusk::OrgName, dusk::AppName)) {
        const std::filesystem::path prefPath = dusk::io::path_from_utf8(pref);
        SDL_free(pref);
        if (auto dir = normalize_existing_dir(prefPath); !dir.empty()) {
            return dir;
        }
    }

    if (const char* docs = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS)) {
        if (auto dir = normalize_existing_dir(dusk::io::path_from_utf8(docs)); !dir.empty()) {
            return dir;
        }
    }

    std::error_code ec;
    return normalize_existing_dir(std::filesystem::current_path(ec));
}

std::filesystem::path initial_data_folder_browser_directory() {
    const std::filesystem::path configured = dusk::data::configured_data_path();
    if (auto dir = normalize_existing_dir(configured); !dir.empty()) {
        return dir;
    }
    if (auto dir = normalize_existing_dir(configured.parent_path()); !dir.empty()) {
        return dir;
    }
    return initial_browser_directory();
}

bool path_less_ci(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::string sa = dusk::io::fs_path_to_string(a.filename());
    std::string sb = dusk::io::fs_path_to_string(b.filename());
    for (char& c : sa) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    for (char& c : sb) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return sa < sb;
}

#if defined(_WIN32)
bool is_windows_drive_letter_root(const std::filesystem::path& p) {
    std::error_code ec;
    const std::filesystem::path abs = std::filesystem::absolute(p, ec).lexically_normal();
    if (ec || abs.empty() || !abs.has_root_name()) {
        return false;
    }
    return abs.relative_path().empty();
}

std::vector<std::filesystem::path> enumerate_windows_drives() {
    std::vector<std::filesystem::path> out;
    wchar_t scratch[512]{};
    const DWORD got = GetLogicalDriveStringsW(static_cast<DWORD>(std::size(scratch)), scratch);
    if (got == 0 || got >= std::size(scratch)) {
        return out;
    }
    for (wchar_t* p = scratch; *p != L'\0';) {
        const std::wstring_view piece(p);
        const std::filesystem::path rootPath{piece};
        const UINT dt = GetDriveTypeW(rootPath.c_str());
        std::error_code ec;
        if (dt != DRIVE_UNKNOWN && dt != DRIVE_NO_ROOT_DIR && std::filesystem::exists(rootPath, ec))
        {
            std::error_code ec2;
            out.push_back(std::filesystem::absolute(rootPath, ec2).lexically_normal());
        }
        p += piece.size() + 1;
    }
    std::sort(
        out.begin(), out.end(), [](const std::filesystem::path& a, const std::filesystem::path& b) {
            std::string sa = dusk::io::fs_path_to_string(a);
            std::string sb = dusk::io::fs_path_to_string(b);
            for (char& c : sa) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            for (char& c : sb) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return sa < sb;
        });
    return out;
}

std::string drive_row_label(const std::filesystem::path& root) {
    std::string s = dusk::io::fs_path_to_string(root.lexically_normal());
    while (!s.empty() && (s.back() == '\\' || s.back() == '/')) {
        s.pop_back();
    }
    if (s.size() >= 2 && s[1] == ':') {
        return std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])))) +
               ":";
    }
    return s;
}

std::string folder_row_label(const std::filesystem::path& dir) {
    std::string fn = dusk::io::fs_path_to_string(dir.filename());
    if (fn.empty() || fn == "." || fn == "..") {
        fn = dusk::io::fs_path_to_string(dir);
    }
    return "[" + fn + "]";
}
#else
bool is_windows_drive_letter_root(const std::filesystem::path&) {
    return false;
}
std::vector<std::filesystem::path> enumerate_windows_drives() {
    return {};
}
std::string drive_row_label(const std::filesystem::path& root) {
    return dusk::io::fs_path_to_string(root);
}
std::string folder_row_label(const std::filesystem::path& dir) {
    std::string fn = dusk::io::fs_path_to_string(dir.filename());
    if (fn.empty() || fn == "." || fn == "..") {
        fn = dusk::io::fs_path_to_string(dir);
    }
    return "[" + fn + "]";
}
#endif

std::string trim_folder_name(std::string_view s) {
    size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t')) {
        ++a;
    }
    size_t b = s.size();
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) {
        --b;
    }
    return std::string(s.substr(a, b - a));
}

bool folder_name_allowed(std::string_view name) {
    if (name.empty() || name == "." || name == "..") {
        return false;
    }
    for (unsigned char uc : name) {
        const char c = static_cast<char>(uc);
        if (c < 0x20) {
            return false;
        }
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|')
        {
            return false;
        }
    }
    return true;
}

bool element_is_or_under(Rml::Element* ancestor, Rml::Element* node) {
    if (ancestor == nullptr || node == nullptr) {
        return false;
    }
    for (auto* walk = node; walk != nullptr; walk = walk->GetParentNode()) {
        if (walk == ancestor) {
            return true;
        }
    }
    return false;
}

class NewFolderDialog final : public WindowSmall {
public:
    NewFolderDialog(std::filesystem::path parent, std::function<void(bool)> onDone);

    bool focus() override;

protected:
    bool handle_nav_command(Rml::Event& event, NavCommand cmd) override;

private:
    bool consume_vertical_nav(Rml::Event& event, NavCommand cmd);
    void try_create();
    void dismiss_cancel();
    void show_error(std::string_view message);

    std::filesystem::path mParent;
    std::function<void(bool)> mOnDone;
    Rml::Element* mInput = nullptr;
    Rml::Element* mErrorLine = nullptr;
    std::vector<std::unique_ptr<Button> > mButtons;
};

NewFolderDialog::NewFolderDialog(std::filesystem::path parent, std::function<void(bool)> onDone)
    : WindowSmall("modal", "modal-dialog"), mParent(std::move(parent)), mOnDone(std::move(onDone)) {
    mDoAud_seStartMenu(kSoundWindowOpen);

    auto* header = append(mDialog, "div");
    header->SetClass("modal-header", true);
    auto* title = append(header, "div");
    title->SetClass("modal-title", true);
    title->SetInnerRML("New folder");

    auto* body = append(mDialog, "div");
    body->SetClass("modal-body", true);
    body->SetInnerRML(
        "Enter a name for the new folder:<br/><input type=\"text\" id=\"new-folder-input\" "
        "maxlength=\"120\" />");

    mInput = mDocument->GetElementById("new-folder-input");

    mErrorLine = append(mDialog, "div");
    mErrorLine->SetClass("file-browser-error", true);
    mErrorLine->SetProperty(Rml::PropertyId::Display, Rml::Style::Display::None);

    auto* actions = append(mDialog, "div");
    actions->SetClass("modal-actions", true);

    auto createBtn = std::make_unique<Button>(actions, "Create");
    createBtn->root()->SetClass("modal-btn", true);
    createBtn->on_pressed([this] { try_create(); });
    mButtons.push_back(std::move(createBtn));

    auto cancelBtn = std::make_unique<Button>(actions, "Cancel");
    cancelBtn->root()->SetClass("modal-btn", true);
    cancelBtn->on_pressed([this] { dismiss_cancel(); });
    mButtons.push_back(std::move(cancelBtn));

    listen(
        mDocument, Rml::EventId::Keydown,
        [this](Rml::Event& event) {
            if (!visible()) {
                return;
            }
            const auto cmd = map_nav_event(event);
            if (cmd == NavCommand::None) {
                return;
            }
            if (consume_vertical_nav(event, cmd)) {
                event.StopImmediatePropagation();
            }
        },
        true);
}

bool NewFolderDialog::focus() {
    if (mInput != nullptr && mInput->Focus(true)) {
        return true;
    }
    if (!mButtons.empty()) {
        return mButtons.front()->focus();
    }
    return false;
}

bool NewFolderDialog::consume_vertical_nav(Rml::Event& event, NavCommand cmd) {
    auto* target = event.GetTargetElement();
    if (target == nullptr) {
        return false;
    }
    if (cmd == NavCommand::Confirm && mInput != nullptr && element_is_or_under(mInput, target)) {
        try_create();
        return true;
    }
    if (cmd == NavCommand::Down && mInput != nullptr && element_is_or_under(mInput, target)) {
        if (!mButtons.empty() && mButtons[0]->focus()) {
            mDoAud_seStartMenu(kSoundItemFocus);
            return true;
        }
        return false;
    }
    if (cmd == NavCommand::Up) {
        for (int i = 0; i < static_cast<int>(mButtons.size()); ++i) {
            if (!mButtons[i]->contains(target)) {
                continue;
            }
            if (i > 0) {
                if (mButtons[i - 1]->focus()) {
                    mDoAud_seStartMenu(kSoundItemFocus);
                    return true;
                }
            } else if (mInput != nullptr && mInput->Focus(true)) {
                mDoAud_seStartMenu(kSoundItemFocus);
                return true;
            }
            return false;
        }
    }
    return false;
}

bool NewFolderDialog::handle_nav_command(Rml::Event& event, NavCommand cmd) {
    if (cmd == NavCommand::Cancel || cmd == NavCommand::Menu) {
        mDoAud_seStartMenu(kSoundWindowClose);
        dismiss_cancel();
        event.StopPropagation();
        return true;
    }
    if (consume_vertical_nav(event, cmd)) {
        event.StopPropagation();
        return true;
    }

    int direction = 0;
    if (cmd == NavCommand::Left) {
        direction = -1;
    } else if (cmd == NavCommand::Right) {
        direction = 1;
    } else {
        return false;
    }

    auto* target = event.GetTargetElement();
    for (int i = 0; i < static_cast<int>(mButtons.size()); ++i) {
        if (mButtons[i]->contains(target)) {
            const int next = i + direction;
            if (next >= 0 && next < static_cast<int>(mButtons.size()) && mButtons[next]->focus()) {
                mDoAud_seStartMenu(kSoundItemFocus);
                return true;
            }
            return false;
        }
    }
    return false;
}

void NewFolderDialog::dismiss_cancel() {
    if (mOnDone) {
        mOnDone(false);
    }
    pop();
}

void NewFolderDialog::show_error(std::string_view message) {
    if (mErrorLine == nullptr) {
        return;
    }
    mErrorLine->SetInnerRML(Rml::String(escape(message).c_str()));
    mErrorLine->SetProperty(Rml::PropertyId::Display, Rml::Style::Display::Block);
}

void NewFolderDialog::try_create() {
    if (mErrorLine != nullptr) {
        mErrorLine->SetInnerRML("");
        mErrorLine->SetProperty(Rml::PropertyId::Display, Rml::Style::Display::None);
    }

    auto* ctrl = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(mInput);
    if (ctrl == nullptr) {
        pop();
        return;
    }

    const Rml::String raw = ctrl->GetValue();
    const std::string name = trim_folder_name(std::string_view(raw.c_str(), raw.size()));
    if (!folder_name_allowed(name)) {
        show_error("Enter a valid folder name.");
        return;
    }

    const std::filesystem::path child = mParent / dusk::io::path_from_utf8(name);
    std::error_code ecExist;
    if (std::filesystem::exists(child, ecExist)) {
        show_error("A file or folder with that name already exists.");
        return;
    }

    std::error_code ec;
    if (!std::filesystem::create_directory(child, ec)) {
        const std::string errMsg = ec ? fmt::format("Could not create folder ({})", ec.message()) :
                                        std::string("Could not create that folder.");
        show_error(errMsg);
        return;
    }

    mDoAud_seStartMenu(kSoundItemChange);
    if (mOnDone) {
        mOnDone(true);
    }
    pop();
}

void try_focus_toolbar_button(Button* b, Rml::Event& event) {
    if (b != nullptr && !b->disabled() && b->focus()) {
        mDoAud_seStartMenu(kSoundItemFocus);
        event.StopImmediatePropagation();
    }
}

bool try_focus_file_pane(Pane* pane, Rml::Event& event) {
    if (pane != nullptr && pane->focus()) {
        mDoAud_seStartMenu(kSoundItemFocus);
        event.StopImmediatePropagation();
        return true;
    }
    return false;
}

bool try_focus_file_pane_last(Pane* pane, Rml::Event& event) {
    if (pane != nullptr && pane->focus_last()) {
        mDoAud_seStartMenu(kSoundItemFocus);
        event.StopImmediatePropagation();
        return true;
    }
    return false;
}

void try_focus_bottom_actions(Button* useFolder, Button* cancel, Rml::Event& event) {
    if (useFolder != nullptr && !useFolder->disabled() && useFolder->focus()) {
        mDoAud_seStartMenu(kSoundItemFocus);
        event.StopImmediatePropagation();
        return;
    }
    if (cancel != nullptr && cancel->focus()) {
        mDoAud_seStartMenu(kSoundItemFocus);
        event.StopImmediatePropagation();
    }
}

void try_focus_toolbar_from_bottom(
    Button* up, Button* localState, Button* newFolder, Rml::Event& event) {
    if (newFolder != nullptr && !newFolder->disabled() && newFolder->focus()) {
        mDoAud_seStartMenu(kSoundItemFocus);
        event.StopImmediatePropagation();
        return;
    }
    if (localState != nullptr && !localState->disabled() && localState->focus()) {
        mDoAud_seStartMenu(kSoundItemFocus);
        event.StopImmediatePropagation();
        return;
    }
    try_focus_toolbar_button(up, event);
}

bool try_linear_button_nav(
    const std::vector<Button*>& buttons, Rml::Element* target, int direction) {
    int idx = -1;
    for (int i = 0; i < static_cast<int>(buttons.size()); ++i) {
        if (buttons[i] != nullptr && buttons[i]->contains(target)) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        return false;
    }
    for (int j = idx + direction; j >= 0 && j < static_cast<int>(buttons.size()); j += direction) {
        if (buttons[j] == nullptr || buttons[j]->disabled()) {
            continue;
        }
        if (buttons[j]->focus()) {
            mDoAud_seStartMenu(kSoundItemFocus);
            return true;
        }
    }
    return false;
}

}  // namespace

FileBrowser::FileBrowser(FileBrowserPurpose purpose)
    : WindowSmall("modal", "modal-dialog"), mPurpose(purpose) {
    mRoot->SetClass("file-browser", true);
    mDoAud_seStartMenu(kSoundWindowOpen);

    auto* header = append(mDialog, "div");
    header->SetClass("modal-header", true);
    auto* title = append(header, "div");
    title->SetClass("modal-title", true);
    title->SetInnerRML(
        mPurpose == FileBrowserPurpose::DataFolder ? "Select data folder" : "Select disc image");

    auto* toolbar = append(mDialog, "div");
    toolbar->SetClass("file-browser-toolbar", true);
    mUpButton = std::make_unique<Button>(toolbar, "Up");
    mUpButton->root()->SetClass("modal-btn", true);
    mUpButton->on_pressed([this] { on_up_pressed(); });

#if defined(_UWP)
    mLocalStateButton = std::make_unique<Button>(toolbar, "LocalState");
    mLocalStateButton->root()->SetClass("modal-btn", true);
    mLocalStateButton->on_pressed([this] { on_local_state_pressed(); });
#endif

    mNewFolderButton = std::make_unique<Button>(toolbar, "New folder");
    mNewFolderButton->root()->SetClass("modal-btn", true);
    mNewFolderButton->on_pressed([this] { on_new_folder_pressed(); });

    mPathDisplay = append(toolbar, "div");
    mPathDisplay->SetClass("file-browser-path", true);

    mErrorDisplay = append(mDialog, "div");
    mErrorDisplay->SetClass("file-browser-error", true);

    auto* listWrap = append(mDialog, "div");
    listWrap->SetClass("file-browser-list", true);
    mFilePane = std::make_unique<Pane>(listWrap, Pane::Type::Uncontrolled);

    auto* actions = append(mDialog, "div");
    actions->SetClass("modal-actions", true);
    if (mPurpose == FileBrowserPurpose::DataFolder) {
        mUseFolderButton = std::make_unique<Button>(actions, "Use this folder");
        mUseFolderButton->root()->SetClass("modal-btn", true);
        mUseFolderButton->on_pressed([this] { on_use_this_folder_pressed(); });
    }
    mCancelButton = std::make_unique<Button>(actions, "Cancel");
    mCancelButton->root()->SetClass("modal-btn", true);
    mCancelButton->on_pressed([this] {
        mDoAud_seStartMenu(kSoundWindowClose);
        pop();
    });

#if defined(_WIN32)
    mBrowseMode = BrowseMode::SelectDrive;
#else
    mBrowseMode = BrowseMode::InDirectory;
    mCurrent = mPurpose == FileBrowserPurpose::DataFolder ?
                   initial_data_folder_browser_directory() :
                   initial_browser_directory();
#endif
    refresh_list();

    listen(
        mDocument, Rml::EventId::Keydown,
        [this](Rml::Event& event) {
            if (!visible()) {
                return;
            }
            const auto cmd = map_nav_event(event);
            if (cmd == NavCommand::None) {
                return;
            }
            if (consume_vertical_nav_keys(event, cmd)) {
                event.StopImmediatePropagation();
            }
        },
        true);
}

bool FileBrowser::consume_vertical_nav_keys(Rml::Event& event, NavCommand cmd) {
    auto* target = event.GetTargetElement();

    if (cmd == NavCommand::Up && mFilePane != nullptr) {
        const bool onUse = mUseFolderButton != nullptr && mUseFolderButton->contains(target);
        const bool onCancel = mCancelButton != nullptr && mCancelButton->contains(target);
        if (onUse || onCancel) {
            if (mFilePane->row_count() > 0) {
                if (try_focus_file_pane_last(mFilePane.get(), event)) {
                    return true;
                }
            }
            try_focus_toolbar_from_bottom(mUpButton.get(),
#if defined(_UWP)
                mLocalStateButton.get(),
#else
                nullptr,
#endif
                mNewFolderButton.get(), event);
            return true;
        }

        const int row = mFilePane->child_index_containing(target);
        const int nrows = mFilePane->row_count();
        const bool atListTop =
            row == 0 || (nrows == 0 && !(mUpButton != nullptr && mUpButton->contains(target)));
        if (atListTop) {
            if (mBrowseMode == BrowseMode::InDirectory) {
                try_focus_toolbar_button(mUpButton.get(), event);
                return true;
            }
            if (mBrowseMode == BrowseMode::SelectDrive) {
#if defined(_UWP)
                if (!mLocalStateButton->disabled()) {
                    try_focus_toolbar_button(mLocalStateButton.get(), event);
                    return true;
                }
#endif
                if (mNewFolderButton != nullptr && !mNewFolderButton->disabled()) {
                    try_focus_toolbar_button(mNewFolderButton.get(), event);
                    return true;
                }
                return true;
            }
        }
    }

    if (cmd == NavCommand::Down && mFilePane != nullptr) {
        if (mUpButton != nullptr && mUpButton->contains(target)) {
            if (!try_focus_file_pane(mFilePane.get(), event)) {
                try_focus_bottom_actions(mUseFolderButton.get(), mCancelButton.get(), event);
            }
            return true;
        }
#if defined(_UWP)
        if (mLocalStateButton->contains(target)) {
            if (!try_focus_file_pane(mFilePane.get(), event)) {
                try_focus_bottom_actions(mUseFolderButton.get(), mCancelButton.get(), event);
            }
            return true;
        }
#endif
        if (mNewFolderButton != nullptr && mNewFolderButton->contains(target)) {
            if (!try_focus_file_pane(mFilePane.get(), event)) {
                try_focus_bottom_actions(mUseFolderButton.get(), mCancelButton.get(), event);
            }
            return true;
        }
    }

    return false;
}

FileBrowser::~FileBrowser() = default;

void FileBrowser::update() {
    if (mFilePane != nullptr) {
        mFilePane->update();
    }
}

bool FileBrowser::focus() {
    if (mFilePane != nullptr && mFilePane->focus()) {
        return true;
    }
    if (mUpButton != nullptr && mUpButton->focus()) {
        return true;
    }
#if defined(_UWP)
    if (mLocalStateButton->focus()) {
        return true;
    }
#endif
    if (mNewFolderButton != nullptr && mNewFolderButton->focus()) {
        return true;
    }
    if (mUseFolderButton != nullptr && mUseFolderButton->focus()) {
        return true;
    }
    if (mCancelButton != nullptr) {
        return mCancelButton->focus();
    }
    return false;
}

bool FileBrowser::handle_nav_command(Rml::Event& event, NavCommand cmd) {
    if (cmd == NavCommand::Cancel || cmd == NavCommand::Menu) {
        mDoAud_seStartMenu(kSoundWindowClose);
        pop();
        event.StopPropagation();
        return true;
    }
    if (consume_vertical_nav_keys(event, cmd)) {
        event.StopPropagation();
        return true;
    }

    int direction = 0;
    if (cmd == NavCommand::Left) {
        direction = -1;
    } else if (cmd == NavCommand::Right) {
        direction = 1;
    } else {
        return WindowSmall::handle_nav_command(event, cmd);
    }

    auto* target = event.GetTargetElement();

    std::vector<Button*> toolbar;
    toolbar.reserve(4);
    toolbar.push_back(mUpButton.get());
#if defined(_UWP)
    toolbar.push_back(mLocalStateButton.get());
#endif
    toolbar.push_back(mNewFolderButton.get());
    if (try_linear_button_nav(toolbar, target, direction)) {
        return true;
    }

    std::vector<Button*> actions;
    actions.reserve(2);
    if (mUseFolderButton != nullptr) {
        actions.push_back(mUseFolderButton.get());
    }
    if (mCancelButton != nullptr) {
        actions.push_back(mCancelButton.get());
    }
    if (try_linear_button_nav(actions, target, direction)) {
        return true;
    }

    return WindowSmall::handle_nav_command(event, cmd);
}

bool FileBrowser::can_go_up() const {
    if (mBrowseMode == BrowseMode::SelectDrive) {
        return false;
    }
    std::error_code ec;
    const std::filesystem::path parent =
        std::filesystem::absolute(mCurrent.parent_path(), ec).lexically_normal();
    if (ec || parent.empty()) {
        return false;
    }
    const auto cur = mCurrent.lexically_normal();
    if (parent == cur) {
        return false;
    }
    return std::filesystem::is_directory(parent, ec);
}

void FileBrowser::on_up_pressed() {
    if (mBrowseMode == BrowseMode::SelectDrive) {
        return;
    }
#if defined(_WIN32)
    if (is_windows_drive_letter_root(mCurrent)) {
        mBrowseMode = BrowseMode::SelectDrive;
        refresh_list();
        return;
    }
#endif
    go_up();
}

void FileBrowser::select_drive_root(const std::filesystem::path& root) {
    std::error_code ec;
    const std::filesystem::path r = std::filesystem::absolute(root, ec).lexically_normal();
    if (ec || r.empty() || !std::filesystem::is_directory(r, ec)) {
        return;
    }
    mCurrent = r;
    mBrowseMode = BrowseMode::InDirectory;
    refresh_list();
}

void FileBrowser::go_up() {
    if (mBrowseMode != BrowseMode::InDirectory || !can_go_up()) {
        return;
    }
    std::error_code ec;
    std::filesystem::path parent =
        std::filesystem::absolute(mCurrent.parent_path(), ec).lexically_normal();
    if (ec || parent.empty() || !std::filesystem::is_directory(parent, ec)) {
        return;
    }

    const std::filesystem::path saved = mCurrent;
    mCurrent = parent;
    if (!refresh_list()) {
        mCurrent = saved;
        refresh_list();
    }
}

void FileBrowser::enter_dir(const std::filesystem::path& p) {
    if (mBrowseMode != BrowseMode::InDirectory) {
        return;
    }
    std::error_code ec;
    std::filesystem::path next = std::filesystem::absolute(p, ec).lexically_normal();
    if (ec || next.empty() || !std::filesystem::is_directory(next, ec)) {
        return;
    }

    const std::filesystem::path saved = mCurrent;
    mCurrent = next;
    if (!refresh_list()) {
        mCurrent = saved;
        refresh_list();
        if (mErrorDisplay != nullptr) {
            mErrorDisplay->SetProperty(Rml::PropertyId::Display, Rml::Style::Display::Block);
            mErrorDisplay->SetInnerRML(escape("Could not open that folder."));
        }
        if (mFilePane != nullptr) {
            mFilePane->focus();
        }
    }
}

void FileBrowser::pick_file(const std::filesystem::path& p) {
    if (mPurpose != FileBrowserPurpose::DiscImage || mBrowseMode != BrowseMode::InDirectory) {
        return;
    }
    queue_disc_image_from_browser(dusk::io::fs_path_to_string(p));
    pop();
}

void FileBrowser::on_use_this_folder_pressed() {
    if (mPurpose != FileBrowserPurpose::DataFolder || mBrowseMode != BrowseMode::InDirectory) {
        return;
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(mCurrent, ec)) {
        return;
    }
    if (apply_picked_data_folder(dusk::io::fs_path_to_string(mCurrent))) {
        pop();
    }
}

void FileBrowser::on_local_state_pressed() {
#if defined(_UWP)
    const auto p = dusk::io::uwp_local_folder_path();
    if (!p.has_value()) {
        return;
    }
    std::error_code ec;
    const std::filesystem::path next = std::filesystem::absolute(*p, ec).lexically_normal();
    if (ec || next.empty() || !std::filesystem::is_directory(next, ec)) {
        return;
    }
    mBrowseMode = BrowseMode::InDirectory;
    mCurrent = next;
    refresh_list();
#endif
}

void FileBrowser::on_new_folder_pressed() {
    if (mBrowseMode != BrowseMode::InDirectory) {
        return;
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(mCurrent, ec)) {
        return;
    }
    push(std::make_unique<NewFolderDialog>(mCurrent, [this](bool created) {
        if (created) {
            refresh_list();
        }
    }));
}

bool FileBrowser::refresh_list() {
    if (mErrorDisplay != nullptr) {
        mErrorDisplay->SetInnerRML("");
        mErrorDisplay->SetProperty(Rml::PropertyId::Display, Rml::Style::Display::None);
    }

#if defined(_UWP)
    {
        const auto local = dusk::io::uwp_local_folder_path();
        mLocalStateButton->set_disabled(!local.has_value());
    }
#endif

    if (mBrowseMode == BrowseMode::SelectDrive) {
        if (mPathDisplay != nullptr) {
            mPathDisplay->SetInnerRML("Select a drive");
        }
        if (mUpButton != nullptr) {
            mUpButton->set_text("Up");
            mUpButton->set_disabled(true);
        }
        if (mUseFolderButton != nullptr) {
            mUseFolderButton->set_disabled(true);
        }
        if (mNewFolderButton != nullptr) {
            mNewFolderButton->set_disabled(true);
        }
        mFilePane->clear();

        const std::vector<std::filesystem::path> drives = enumerate_windows_drives();
        if (drives.empty()) {
            mBrowseMode = BrowseMode::InDirectory;
            mCurrent = mPurpose == FileBrowserPurpose::DataFolder ?
                           initial_data_folder_browser_directory() :
                           initial_browser_directory();
            return refresh_list();
        }

        for (const auto& d : drives) {
            const std::string label = "[" + drive_row_label(d) + "]";
            auto& btn = mFilePane->add_button(escape(label));
            btn.root()->SetClass("file-browser-row", true);
            const std::filesystem::path target = d;
            btn.on_pressed([this, target] { select_drive_root(target); });
        }

        if (mFilePane != nullptr) {
            mFilePane->focus();
        }
        return true;
    }

    if (mPathDisplay != nullptr) {
        mPathDisplay->SetInnerRML(escape(dusk::io::fs_path_to_string(mCurrent)));
    }
    if (mUpButton != nullptr) {
#if defined(_WIN32)
        if (is_windows_drive_letter_root(mCurrent)) {
            mUpButton->set_text("Drives");
            mUpButton->set_disabled(false);
        } else {
            mUpButton->set_text("Up");
            mUpButton->set_disabled(!can_go_up());
        }
#else
        mUpButton->set_text("Up");
        mUpButton->set_disabled(!can_go_up());
#endif
    }

    std::error_code ec;
    if (!std::filesystem::exists(mCurrent, ec) || !std::filesystem::is_directory(mCurrent, ec)) {
        if (mErrorDisplay != nullptr) {
            mErrorDisplay->SetProperty(Rml::PropertyId::Display, Rml::Style::Display::Block);
            mErrorDisplay->SetInnerRML("This folder is not available.");
        }
        if (mUseFolderButton != nullptr) {
            mUseFolderButton->set_disabled(true);
        }
        if (mNewFolderButton != nullptr) {
            mNewFolderButton->set_disabled(true);
        }
        mFilePane->clear();
        return false;
    }

    mFilePane->clear();

    std::vector<std::filesystem::path> dirs;
    std::vector<std::filesystem::path> files;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(mCurrent)) {
            std::error_code stec;
            const auto status = entry.symlink_status(stec);
            if (stec) {
                continue;
            }
            if (!std::filesystem::is_directory(status) && !std::filesystem::is_regular_file(status))
            {
                continue;
            }
            const auto& path = entry.path();
            std::string fname = dusk::io::fs_path_to_string(path.filename());
            if (fname.empty() || fname == "." || fname == "..") {
                fname = dusk::io::fs_path_to_string(path);
            }
            if (!fname.empty() && fname[0] == '.') {
                continue;
            }
            if (std::filesystem::is_directory(status)) {
                dirs.push_back(path);
            } else if (mPurpose == FileBrowserPurpose::DiscImage &&
                       std::filesystem::is_regular_file(status) && is_disc_extension(path))
            {
                files.push_back(path);
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        if (mErrorDisplay != nullptr) {
            mErrorDisplay->SetProperty(Rml::PropertyId::Display, Rml::Style::Display::Block);
            mErrorDisplay->SetInnerRML(escape(e.what()));
        }
        if (mUseFolderButton != nullptr) {
            mUseFolderButton->set_disabled(true);
        }
        if (mNewFolderButton != nullptr) {
            mNewFolderButton->set_disabled(true);
        }
        return false;
    }

    std::sort(dirs.begin(), dirs.end(), path_less_ci);
    std::sort(files.begin(), files.end(), path_less_ci);

    for (const auto& dir : dirs) {
        auto& btn = mFilePane->add_button(escape(folder_row_label(dir)));
        btn.root()->SetClass("file-browser-row", true);
        const std::filesystem::path target = dir;
        btn.on_pressed([this, target] { enter_dir(target); });
    }
    for (const auto& file : files) {
        auto& btn = mFilePane->add_button(escape(dusk::io::fs_path_to_string(file.filename())));
        btn.root()->SetClass("file-browser-row", true);
        const std::filesystem::path target = file;
        btn.on_pressed([this, target] { pick_file(target); });
    }

    if (dirs.empty() && files.empty() && mErrorDisplay != nullptr) {
        mErrorDisplay->SetProperty(Rml::PropertyId::Display, Rml::Style::Display::Block);
        mErrorDisplay->SetInnerRML(mPurpose == FileBrowserPurpose::DataFolder ?
                                       "This folder is empty." :
                                       "No disc images in this folder.");
    }

    if (mUseFolderButton != nullptr) {
        std::error_code dec;
        const bool ok = std::filesystem::is_directory(mCurrent, dec);
        mUseFolderButton->set_disabled(!ok);
    }

    if (mNewFolderButton != nullptr) {
        mNewFolderButton->set_disabled(false);
    }

    if (mFilePane != nullptr) {
        mFilePane->focus();
    }

    return true;
}

}  // namespace dusk::ui
