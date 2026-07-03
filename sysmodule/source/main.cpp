#include <switch.h>
#include <httplib.h>
#include <time.h>
#include <string>

// Configuration
const int RATE_LIMIT_SECONDS = 3;
std::string AUTH_TOKEN = "my_epic_password"; 
time_t last_launch_time = 0;

// Function to close the current game and launch a new one
bool switch_game(u64 title_id) {
    Result rc = 0;
    
    // 1. Terminate currently running application (if any)
    u64 pid = 0;
    pmdmntGetApplicationProcessId(&pid);
    if (pid != 0) {
        svcExitProcess(); // or use pmdmntTerminateProcess(pid) via libnx
    }

    // 2. Launch the new requested Title ID
    rc = appletRequestToLaunchApplication(title_id, NULL);
    return R_SUCCEEDED(rc);
}

void start_server() {
    httplib::Server svr;

    svr.Post("/api/launch", [](const httplib::Request& req, httplib::Response& res) {
        // Authentication Check
        if (req.get_header_value("Authorization") != AUTH_TOKEN) {
            res.status = 401;
            res.set_content("Unauthorized: Invalid Password", "text/plain");
            return;
        }

        // Rate Limit Check
        time_t current_time = time(NULL);
        if (current_time - last_launch_time < RATE_LIMIT_SECONDS) {
            res.status = 429;
            res.set_content("Rate Limited: Wait 3 seconds.", "text/plain");
            return;
        }

        // Parse Title ID
        std::string title_id_str = req.body; 
        u64 title_id = std::stoull(title_id_str, nullptr, 16);

        if (switch_game(title_id)) {
            last_launch_time = current_time;
            res.status = 200;
            res.set_content("Game launched successfully!", "text/plain");
        } else {
            res.status = 500;
            res.set_content("Failed to launch game via applet.", "text/plain");
        }
    });

    svr.listen("0.0.0.0", 8080);
}

int main(int argc, char **argv) {
    appletInitialize();
    pmdmntInitialize();
    socketInitializeDefault();

    start_server();

    socketExit();
    pmdmntExit();
    appletExit();
    return 0;
}
