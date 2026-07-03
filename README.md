# 🎮 nx-remote-ovl

> **The ultimate, always-on remote launcher for modded Nintendo Switches.**

**nx-remote-ovl** completely revolutionizes how you launch games remotely on your Switch. In the past, you had to manually open a specific homebrew app to receive network commands. Not anymore.

By utilizing a lightweight background sysmodule combined with a sleek **Tesla/Ultrahand overlay**, you can send a command to your Switch from *anywhere* (Linux, Windows, Playnite, etc.). If you are already in a game, the sysmodule will seamlessly close it and launch your requested title instantly.

---

## ✨ Epic Features

* **⚡ Always-On Background Execution:** A custom Atmosphère sysmodule runs from boot, always listening for network requests. No need to boot into the Homebrew Menu.
* **🔒 Secure Authentication:** Don't let random people on your network hijack your Switch. Set up a secure password via the overlay. The sysmodule rejects any request with a missing or wrong `Authorization` header.
* **⏱️ Rate Limiting:** Built-in protection limits requests to **1 game every 3 seconds** (configurable) to prevent spamming, accidental double-clicks, or system crashes.
* **🖥️ Tesla/Ultrahand Overlay GUI:** Pull up the overlay in-game to check your Switch's IP address, daemon status, and set your password with the on-screen keyboard.
* **🌍 Universal & Frontend Agnostic:** A plain REST API (`POST /api/launch`). Whether you are running a Bash script on Linux or using a slick UI on Windows, integration is flawless.

---

## 🛠️ How It Works

The project has three parts, each buildable independently:

1. **`sysmodule/` (Backend):** A libnx system module (`AppletType_None`) that runs silently in the background of Horizon OS. It embeds [cpp-httplib](https://github.com/yhirose/cpp-httplib) to serve a tiny HTTP API, authenticates incoming requests, terminates the currently running application via `pm:shell`/`pm:dmnt`, and launches the requested Title ID via `pmshellLaunchProgram`.
2. **`overlay/` (Frontend):** A [libtesla](https://github.com/WerWolv/libtesla)-based overlay (`.ovl`) — fully compatible with both Tesla-Menu and Ultrahand — that is your on-console control panel for the sysmodule.
3. **`client/launcher.py` (Client):** A small Python CLI that POSTs a launch request. Point any launcher (Playnite, a Bash alias, whatever) at it.

Both native components read/write a shared config file at `sd:/config/nx-remote-ovl/config.ini` (password, port, rate limit), so changing the password from the overlay takes effect on the sysmodule's very next request — no reboot required.

---

## 📦 Building

Every push to `main` that touches `sysmodule/`, `overlay/`, or `client/` is built automatically by [`.github/workflows/release.yml`](.github/workflows/release.yml), which bumps a patch tag and publishes a GitHub Release with a ready-to-copy `nx-remote-ovl.zip` (SD card layout: `atmosphere/`, `switch/.overlays/`, and the client script). Grab that if you'd rather not set up devkitPro locally.

### Prerequisites

- [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the `switch-dev` package group (provides devkitA64, libnx, `elf2nro`, `npdmtool`, etc). Set the `DEVKITPRO` environment variable as the installer instructs.
- Git (for the libtesla submodule).

### Sysmodule

```sh
cd sysmodule
make
```

This produces `nx-remote-ovl-sys.nsp`. Install it and set it to run at boot:

```sh
# On the SD card used by your modded Switch:
mkdir -p "sd:/atmosphere/contents/4200000000000F00/flags"
cp nx-remote-ovl-sys.nsp "sd:/atmosphere/contents/4200000000000F00/exefs.nsp"
touch "sd:/atmosphere/contents/4200000000000F00/flags/boot2.flag"
```

> The title ID (`4200000000000F00`) is set in `sysmodule/title.json`. If you already run another homebrew sysmodule using that ID, change it there (and in `overlay/source/main.cpp`'s `SysmoduleTitleId`, which must match) before building.

### Overlay

The overlay depends on libtesla as a git submodule:

```sh
git submodule update --init --recursive
cd overlay
make
```

This produces `nx-remote-ovl.ovl`. Drop it in `sd:/switch/.overlays/` alongside Tesla-Menu or Ultrahand, and launch it in-game with the overlay hotkey.

### Client

```sh
cd client
pip install requests
cp config.ini.example config.ini
# edit config.ini with your Switch's IP and the password you set in the overlay
python3 launcher.py 0100000000010000
```

`config.ini` is git-ignored since it holds your password — CLI flags (`--ip`, `--port`, `--password`) override it if you'd rather not keep one around.

---

## 🚀 Playnite Integration

Want to click "Play" on your PC and have your Switch instantly boot the game? There's a dedicated Playnite integration for exactly this purpose!

Check out this epic repository to get it set up:
👉 **[NXRLPlayniteSupport](https://github.com/Yeetov/NXRLPlayniteSupport)**

---

## 🙏 Credits & Acknowledgments

This project wouldn't be possible without the incredible foundation laid by the Switch homebrew community.

* **[kirankunigiri/nx-remote-launcher](https://github.com/kirankunigiri/nx-remote-launcher)**: A massive shoutout and credit to Kiran for the original `nx-remote-launcher` project — the direct inspiration for this background sysmodule/overlay evolution!
* **[WerWolv/libtesla](https://github.com/WerWolv/libtesla) & The Ultrahand Team**: For the overlay framework that makes the in-game GUI possible.
* **[yhirose/cpp-httplib](https://github.com/yhirose/cpp-httplib)**: The HTTP server embedded in the sysmodule.
