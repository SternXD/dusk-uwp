#pragma once

#include "button.hpp"
#include "pane.hpp"
#include "window.hpp"

namespace dusk::ui {

enum class FileBrowserPurpose : std::uint8_t { DiscImage, DataFolder };

class FileBrowser final : public WindowSmall {
public:
    explicit FileBrowser(FileBrowserPurpose purpose = FileBrowserPurpose::DiscImage);
    ~FileBrowser() override;

    void update() override;
    bool focus() override;

protected:
    bool handle_nav_command(Rml::Event& event, NavCommand cmd) override;

private:
    enum class BrowseMode : std::uint8_t { SelectDrive, InDirectory };

    bool refresh_list();
    void on_up_pressed();
    void go_up();
    void select_drive_root(const std::filesystem::path& root);
    void enter_dir(const std::filesystem::path& p);
    void pick_file(const std::filesystem::path& p);
    void on_use_this_folder_pressed();
    void on_local_state_pressed();
    void on_new_folder_pressed();
    bool can_go_up() const;
    bool consume_vertical_nav_keys(Rml::Event& event, NavCommand cmd);

    FileBrowserPurpose mPurpose;
    BrowseMode mBrowseMode = BrowseMode::InDirectory;
    std::filesystem::path mCurrent;
    Rml::Element* mPathDisplay = nullptr;
    Rml::Element* mErrorDisplay = nullptr;
    std::unique_ptr<Pane> mFilePane;
    std::unique_ptr<Button> mUpButton;
#if defined(_UWP)
    std::unique_ptr<Button> mLocalStateButton;
#endif
    std::unique_ptr<Button> mNewFolderButton;
    std::unique_ptr<Button> mUseFolderButton;
    std::unique_ptr<Button> mCancelButton;
};

}  // namespace dusk::ui
