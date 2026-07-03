# 🎮 nx-remote-ovl

> **The ultimate, always-on remote launcher for modded Nintendo Switches.**

**nx-remote-ovl** completely revolutionizes how you launch games remotely on your Switch. In the past, you had to manually open a specific homebrew app to receive network commands. Not anymore. 

By utilizing a lightweight background sysmodule combined with a sleek **Ultrahand overlay**, you can send a command to your Switch from *anywhere* (Linux, Windows, Playnite, etc.). If you are already in a game, the sysmodule will seamlessly close it and launch your requested title instantly. 

---

## ✨ Epic Features

* **⚡ Always-On Background Execution:** Powered by a custom sysmodule, your Switch is always listening for network requests. No need to boot into the Homebrew Menu.
* **🔒 Secure Authentication:** Don't let random people on your network hijack your Switch. Set up a secure password via the overlay. The sysmodule will reject any requests that don't pass the credential check.
* **⏱️ Rate Limiting:** Built-in protection limits requests to **1 game every 3 seconds** to prevent spamming, accidental double-clicks, or system crashes.
* **🖥️ Ultrahand Overlay GUI:** Easily pull up the overlay in-game to check your Switch's IP address, view daemon status, and manage your password.
* **🌍 Universal & Frontend Agnostic:** Built entirely around standard REST API requests. Whether you are running a Bash script on Linux or using a slick UI on Windows, integration is flawless.

---

## 🛠️ How It Works

The project is split into two main components:
1.  **The Sysmodule (Backend):** Runs silently in the background of Horizon OS. It spins up a tiny HTTP server, authenticates incoming requests, kills the active PID (closing your current game), and passes the new Title ID to the applet launcher.
2.  **The Ultrahand Overlay (Frontend):** Your on-console control panel for the sysmodule. 

---

## 🚀 Playnite Integration

Want to click "Play" on your PC and have your Switch instantly boot the game? I've built a dedicated Playnite integration for exactly this purpose!

Check out my sister repository to get it set up:
👉 **[Yeetov/NXRLPlayniteSupport](https://github.com/Yeetov/NXRLPlayniteSupport)**

---

## 🙏 Credits & Acknowledgments

This project wouldn't be possible without the incredible foundation laid by the Switch homebrew community. 

* **[kirankunigiri/nx-remote-launcher](https://github.com/kirankunigiri/nx-remote-launcher)**: A massive shoutout and credit to Kiran for the original `nx-remote-launcher` project. Their `nx.js` concept was the direct inspiration for this background sysmodule/overlay evolution!
* **The Ultrahand Team**: For creating the incredible overlay framework that makes the in-game GUI possible.
