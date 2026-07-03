import requests
import sys
import time

SWITCH_IP = "192.168.1.50"
PORT = 8080
PASSWORD = "my_epic_password"

def launch_game(title_id):
    url = f"http://{SWITCH_IP}:{PORT}/api/launch"
    headers = {
        "Authorization": PASSWORD,
        "Content-Type": "text/plain"
    }
    
    print(f"Attempting to launch {title_id} on Switch...")
    
    try:
        response = requests.post(url, headers=headers, data=title_id)
        
        if response.status_code == 200:
            print("Success! Game is booting on the Switch.")
        elif response.status_code == 429:
            print("Rate limited! Please wait 3 seconds before launching another game.")
        elif response.status_code == 401:
            print("Authentication failed. Check your password.")
        else:
            print(f"Error {response.status_code}: {response.text}")
            
    except requests.exceptions.RequestException as e:
        print(f"Failed to connect to the Switch: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python launcher.py <TITLE_ID>")
        sys.exit(1)
        
    target_title_id = sys.argv[1]
    launch_game(target_title_id)
