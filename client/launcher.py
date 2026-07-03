#!/usr/bin/env python3
"""Remote launcher client for nx-remote-ovl.

Sends a launch request to the nx-remote-ovl sysmodule running on a Switch.
Settings are resolved in this order: CLI flags > config file > built-in
defaults, so a Playnite/Bash/whatever caller only has to pass the title ID.
"""
import argparse
import configparser
import os
import sys

import requests

DEFAULT_CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.ini")


def load_config(path):
    defaults = {"switch_ip": "192.168.1.50", "port": "8080", "password": "changeme"}

    parser = configparser.ConfigParser()
    parser["nx-remote-ovl"] = defaults
    if os.path.isfile(path):
        parser.read(path)

    section = parser["nx-remote-ovl"]
    return {
        "switch_ip": section.get("switch_ip"),
        "port": section.getint("port"),
        "password": section.get("password"),
    }


def launch_game(switch_ip, port, password, title_id, timeout=5):
    url = f"http://{switch_ip}:{port}/api/launch"
    headers = {
        "Authorization": password,
        "Content-Type": "text/plain",
    }

    print(f"Attempting to launch {title_id} on {switch_ip}:{port}...")

    try:
        response = requests.post(url, headers=headers, data=title_id, timeout=timeout)
    except requests.exceptions.RequestException as e:
        print(f"Failed to connect to the Switch: {e}")
        return False

    if response.status_code == 200:
        print("Success! Game is booting on the Switch.")
        return True
    elif response.status_code == 429:
        print("Rate limited! Please wait a few seconds before launching another game.")
    elif response.status_code == 401:
        print("Authentication failed. Check your password.")
    else:
        print(f"Error {response.status_code}: {response.text}")

    return False


def main():
    parser = argparse.ArgumentParser(description="Launch a game remotely on a modded Switch.")
    parser.add_argument("title_id", help="Title ID of the game to launch, in hex (e.g. 0100000000010000)")
    parser.add_argument("--ip", dest="switch_ip", help="Switch IP address (overrides config.ini)")
    parser.add_argument("--port", type=int, help="Sysmodule port (overrides config.ini)")
    parser.add_argument("--password", help="Sysmodule password (overrides config.ini)")
    parser.add_argument("--config", default=DEFAULT_CONFIG_PATH, help="Path to config.ini")
    parser.add_argument("--timeout", type=float, default=5, help="Request timeout in seconds")
    args = parser.parse_args()

    config = load_config(args.config)
    switch_ip = args.switch_ip or config["switch_ip"]
    port = args.port or config["port"]
    password = args.password or config["password"]

    ok = launch_game(switch_ip, port, password, args.title_id, timeout=args.timeout)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
