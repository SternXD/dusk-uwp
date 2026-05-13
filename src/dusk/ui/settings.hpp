#pragma once
#include "window.hpp"

namespace dusk::ui {

void show_data_folder_error_modal(std::string_view message);
bool apply_picked_data_folder(std::string utf8_path);

class SettingsWindow : public Window {
public:
    SettingsWindow(bool prelaunch = false);

    void update() override;

protected:
    bool mPrelaunch;
};

}  // namespace dusk::ui