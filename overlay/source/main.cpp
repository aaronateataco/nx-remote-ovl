#include <ultrahand/ultrahand.hpp>
#include <string>

// This is a placeholder for your actual IP retrieval logic
std::string GetSwitchIP() {
    return "192.168.1.50"; 
}

class UltraLaunchGUI : public ui::View {
public:
    UltraLaunchGUI() {
        std::string ip_address = GetSwitchIP(); 

        auto ip_text = ui::MakeText("Switch IP: " + ip_address);
        auto status_text = ui::MakeText("Daemon Status: RUNNING");
        auto password_text = ui::MakeText("Current Password: my_epic_password");

        this->AddChild(ip_text);
        this->AddChild(status_text);
        this->AddChild(password_text);
    }
};
