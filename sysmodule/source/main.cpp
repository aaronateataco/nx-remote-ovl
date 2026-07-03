#include <switch.h>
#include <httplib.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>

namespace {

constexpr const char *ConfigDir = "sdmc:/config/nx-remote-ovl";
constexpr const char *ConfigPath = "sdmc:/config/nx-remote-ovl/config.ini";

struct Config {
    std::string password = "changeme";
    int port = 8080;
    int rate_limit_seconds = 3;
};

std::string Trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

void WriteDefaultConfig() {
    mkdir(ConfigDir, 0777);
    std::ofstream out(ConfigPath);
    if (!out.is_open())
        return;
    out << "# nx-remote-ovl sysmodule config\n";
    out << "# Change the password from the Ultrahand overlay in-game, or edit this\n";
    out << "# file directly. Changes are picked up on the next request, no reboot needed.\n";
    out << "password = changeme\n";
    out << "port = 8080\n";
    out << "rate_limit_seconds = 3\n";
}

Config LoadConfig() {
    Config cfg;
    std::ifstream in(ConfigPath);
    if (!in.is_open()) {
        WriteDefaultConfig();
        return cfg;
    }

    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == '[')
            continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = Trim(line.substr(0, eq));
        std::string value = Trim(line.substr(eq + 1));

        if (key == "password" && !value.empty())
            cfg.password = value;
        else if (key == "port")
            cfg.port = std::atoi(value.c_str());
        else if (key == "rate_limit_seconds")
            cfg.rate_limit_seconds = std::atoi(value.c_str());
    }

    return cfg;
}

// Constant-time-ish compare so the password check doesn't leak length/prefix
// information via early-exit timing on a shared home network.
bool SecureEquals(const std::string &a, const std::string &b) {
    if (a.size() != b.size())
        return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); i++)
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    return diff == 0;
}

// Terminates whatever application is currently running (if any) and waits
// briefly for pm to actually tear it down before we hand it a new title,
// otherwise pmshellLaunchProgram can race the previous process's cleanup.
void TerminateCurrentApplication() {
    u64 pid = 0;
    if (R_FAILED(pmdmntGetApplicationProcessId(&pid)) || pid == 0)
        return;

    pmshellTerminateProcess(pid);

    for (int i = 0; i < 40; i++) {
        u64 stillRunning = 0;
        Result rc = pmdmntGetApplicationProcessId(&stillRunning);
        if (R_FAILED(rc) || stillRunning == 0)
            break;
        svcSleepThread(75'000'000ULL); // 75ms
    }
}

bool LaunchTitle(u64 titleId) {
    const NcmProgramLocation location{
        .program_id = titleId,
        .storageID = NcmStorageId_None,
    };

    u64 pid = 0;
    return R_SUCCEEDED(pmshellLaunchProgram(0, &location, &pid));
}

} // namespace

extern "C" {

u32 __nx_applet_type = AppletType_None;
u32 __nx_fs_num_sessions = 1;

// httplib pulls in <regex>/<thread>/<mutex>, so give the module more room
// than a typical bare sysmodule needs.
#define INNER_HEAP_SIZE 0x400000

void __libnx_initheap(void) {
    static u8 inner_heap[INNER_HEAP_SIZE];
    extern void *fake_heap_start;
    extern void *fake_heap_end;

    fake_heap_start = inner_heap;
    fake_heap_end = inner_heap + sizeof(inner_heap);
}

void __appInit(void) {
    Result rc = smInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));

    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion fw;
        rc = setsysGetFirmwareVersion(&fw);
        if (R_SUCCEEDED(rc))
            hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        setsysExit();
    }

    rc = fsInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_FS));
    fsdevMountSdmc();

    rc = pmdmntInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(rc);

    rc = pmshellInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(rc);

    rc = nifmInitialize(NifmServiceType_System);
    if (R_FAILED(rc))
        diagAbortWithResult(rc);

    static const SocketInitConfig socketCfg = {
        .tcp_tx_buf_size = 0x2000,
        .tcp_rx_buf_size = 0x2000,
        .tcp_tx_buf_max_size = 0x4000,
        .tcp_rx_buf_max_size = 0x4000,
        .udp_tx_buf_size = 0,
        .udp_rx_buf_size = 0,
        .sb_efficiency = 4,
        .num_bsd_sessions = 2,
        .bsd_service_type = BsdServiceType_User,
    };
    rc = socketInitialize(&socketCfg);
    if (R_FAILED(rc))
        diagAbortWithResult(rc);

    smExit();
}

void __appExit(void) {
    socketExit();
    nifmExit();
    pmshellExit();
    pmdmntExit();
    fsdevUnmountAll();
    fsExit();
}

} // extern "C"

int main(int, char **) {
    time_t last_launch_time = 0;

    httplib::Server svr;

    // Unauthenticated health check so the client/Playnite plugin can verify
    // the sysmodule is up before prompting for a title.
    svr.Get("/api/ping", [](const httplib::Request &, httplib::Response &res) {
        res.set_content("nx-remote-ovl-sys ok", "text/plain");
    });

    svr.Post("/api/launch", [&](const httplib::Request &req, httplib::Response &res) {
        Config cfg = LoadConfig();

        std::string auth = req.get_header_value("Authorization");
        if (!SecureEquals(auth, cfg.password)) {
            res.status = 401;
            res.set_content("Unauthorized: Invalid Password", "text/plain");
            return;
        }

        time_t now = time(nullptr);
        if (now - last_launch_time < cfg.rate_limit_seconds) {
            res.status = 429;
            res.set_content("Rate Limited: Too many requests.", "text/plain");
            return;
        }

        u64 titleId = 0;
        try {
            titleId = std::stoull(Trim(req.body), nullptr, 16);
        } catch (const std::exception &) {
            res.status = 400;
            res.set_content("Bad Request: Body must be a hex Title ID.", "text/plain");
            return;
        }

        if (titleId == 0) {
            res.status = 400;
            res.set_content("Bad Request: Body must be a hex Title ID.", "text/plain");
            return;
        }

        TerminateCurrentApplication();

        if (LaunchTitle(titleId)) {
            last_launch_time = now;
            res.status = 200;
            res.set_content("Game launched successfully!", "text/plain");
        } else {
            res.status = 500;
            res.set_content("Failed to launch game via pm:shell.", "text/plain");
        }
    });

    Config cfg = LoadConfig();
    svr.listen("0.0.0.0", cfg.port);

    return 0;
}
