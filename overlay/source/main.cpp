#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <sys/stat.h>

// Raw 48x48 RGBA8888 avatars embedded from overlay/data/*.bin - see
// overlay/Makefile's generic bin2o rule. Headers are generated into build/.
#include "aaronateataco_bin.h"
#include "kirankunigiri_bin.h"
#include "werwolv_bin.h"
#include "yhirose_bin.h"

namespace {

// Must match sysmodule/title.json's title_id.
constexpr u64 SysmoduleTitleId = 0x4200000000000F00;
constexpr s32 AvatarSize = 48;

void DrawCredit(tsl::gfx::Renderer *renderer, s32 x, s32 y, const u8 *avatar, const char *name, const char *role) {
    renderer->drawBitmap(x + 10, y + 6, AvatarSize, AvatarSize, avatar);
    renderer->drawString(name, false, x + AvatarSize + 25, y + 25, 20, renderer->a(tsl::style::color::ColorText));
    renderer->drawString(role, false, x + AvatarSize + 25, y + 47, 15, renderer->a(tsl::style::color::ColorDescription));
}

constexpr const char *ConfigDir = "sdmc:/config/nx-remote-ovl";
constexpr const char *ConfigPath = "sdmc:/config/nx-remote-ovl/config.ini";

std::string Trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// The sysmodule owns the config schema; the overlay only ever rewrites the
// password line so it never clobbers port/rate_limit_seconds set elsewhere.
void SetPassword(const std::string &password) {
    int port = 61337;
    int rateLimit = 3;

    std::ifstream in(ConfigPath);
    if (in.is_open()) {
        std::string line;
        while (std::getline(in, line)) {
            line = Trim(line);
            size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;
            std::string key = Trim(line.substr(0, eq));
            std::string value = Trim(line.substr(eq + 1));
            if (key == "port")
                port = std::atoi(value.c_str());
            else if (key == "rate_limit_seconds")
                rateLimit = std::atoi(value.c_str());
        }
    }

    mkdir(ConfigDir, 0777);
    std::ofstream out(ConfigPath);
    if (!out.is_open())
        return;
    out << "# nx-remote-ovl sysmodule config\n";
    out << "password = " << password << "\n";
    out << "port = " << port << "\n";
    out << "rate_limit_seconds = " << rateLimit << "\n";
}

bool IsWifiConnected() {
    u32 ip = 0;
    return R_SUCCEEDED(nifmGetCurrentIpAddress(&ip));
}

std::string GetSwitchIp() {
    u32 ip = 0;
    if (R_FAILED(nifmGetCurrentIpAddress(&ip)))
        return "Not connected";

    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                  (ip >> 0) & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
    return buf;
}

bool IsDaemonRunning() {
    u64 pid = 0;
    return R_SUCCEEDED(pmdmntGetProcessId(&pid, SysmoduleTitleId)) && pid > 0;
}

// The on-screen keyboard (swkbd) doesn't reliably launch from inside an
// active Tesla/Ultrahand overlay context - even Ultrahand's own UI avoids it
// entirely for text entry. So instead of typing a password, generate one with
// the console's hardware RNG and show it right on the list item: the user
// reads it off screen once and copies it into their client's config.
std::string RandomizePassword() {
    static const char charset[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789";
    constexpr size_t length = 10;

    u8 raw[length];
    csrngGetRandomBytes(raw, sizeof(raw));

    std::string password;
    password.reserve(length);
    for (size_t i = 0; i < length; i++)
        password += charset[raw[i] % (sizeof(charset) - 1)];

    SetPassword(password);
    return password;
}

} // namespace

// Draws the normal frame, then - if there's no Wi-Fi connection - dims the
// whole screen with a translucent grey layer and a status message on top of
// everything else, since nothing here works remotely without a network.
class NxOverlayFrame : public tsl::elm::OverlayFrame {
public:
    NxOverlayFrame(const std::string &title, const std::string &subtitle)
        : tsl::elm::OverlayFrame(title, subtitle) {}

    virtual void draw(tsl::gfx::Renderer *renderer) override {
        tsl::elm::OverlayFrame::draw(renderer);

        if (IsWifiConnected())
            return;

        renderer->drawRect(0, 0, tsl::cfg::FramebufferWidth, tsl::cfg::FramebufferHeight,
                            renderer->a(tsl::Color(0x8, 0x8, 0x8, 0xA)));
        renderer->drawString("Not connected to Wi-Fi", false, 55, 400, 25,
                              renderer->a(tsl::style::color::ColorText));
        renderer->drawString("Remote launching won't work until this Switch is online.", false, 55, 425, 15,
                              renderer->a(tsl::style::color::ColorDescription));
    }
};

class MainGui : public tsl::Gui {
public:
    MainGui() = default;

    virtual tsl::elm::Element *createUI() override {
        auto *frame = new NxOverlayFrame("nx-remote-ovl", VERSION);

        auto *list = new tsl::elm::List();

        list->addItem(new tsl::elm::CategoryHeader("Status"));

        auto *ipItem = new tsl::elm::ListItem("Switch IP Address");
        ipItem->setValue(GetSwitchIp());
        list->addItem(ipItem);

        this->m_statusItem = new tsl::elm::ListItem("Daemon Status");
        this->m_statusItem->setValue(IsDaemonRunning() ? "Running" : "Not Running");
        list->addItem(this->m_statusItem);

        list->addItem(new tsl::elm::CategoryHeader("Security", true));

        auto *passwordItem = new tsl::elm::ListItem("Randomize Password", "Press A");
        passwordItem->setClickListener([passwordItem](u64 keys) -> bool {
            if (keys & HidNpadButton_A) {
                passwordItem->setValue(RandomizePassword());
                return true;
            }
            return false;
        });
        list->addItem(passwordItem);

        list->addItem(new tsl::elm::CategoryHeader("Credits", true));
        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            DrawCredit(renderer, x, y, aaronateataco_bin, "Aaronateataco", "nx-remote-ovl");
        }), AvatarSize + 12);
        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            DrawCredit(renderer, x, y, kirankunigiri_bin, "kirankunigiri/nx-remote-launcher", "Original inspiration");
        }), AvatarSize + 12);
        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            DrawCredit(renderer, x, y, werwolv_bin, "WerWolv/libtesla & Ultrahand Team", "Overlay framework");
        }), AvatarSize + 12);
        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            DrawCredit(renderer, x, y, yhirose_bin, "yhirose/cpp-httplib", "HTTP server");
        }), AvatarSize + 12);

        frame->setContent(list);
        return frame;
    }

    virtual void update() override {
        // Refresh the daemon status roughly twice a second instead of every
        // single frame, pmdmntGetProcessId is an IPC round trip.
        if (this->m_updateCounter++ % 30 != 0)
            return;

        if (this->m_statusItem != nullptr)
            this->m_statusItem->setValue(IsDaemonRunning() ? "Running" : "Not Running");
    }

private:
    tsl::elm::ListItem *m_statusItem = nullptr;
    u32 m_updateCounter = 0;
};

class NxRemoteOverlay : public tsl::Overlay {
public:
    // libtesla already initializes fs, hid, pl, pmdmnt, hid:sys and set:sys
    // for every overlay, so we only need to bring up nifm (IP display) and
    // csrng (random password generation) ourselves.
    virtual void initServices() override {
        nifmInitialize(NifmServiceType_User);
        csrngInitialize();
    }

    virtual void exitServices() override {
        csrngExit();
        nifmExit();
    }

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<MainGui>();
    }
};

int main(int argc, char **argv) {
    return tsl::loop<NxRemoteOverlay>(argc, argv);
}
