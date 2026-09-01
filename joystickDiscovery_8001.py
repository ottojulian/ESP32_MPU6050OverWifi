import socket
import time
import threading
import webbrowser
import json
import urllib.request
import urllib.error

from pythonosc.osc_message import OscMessage


# ============================================================
# CONFIGURATION
# ============================================================

LISTEN_IP = "0.0.0.0"

# Puerto FIJO de discovery.
# Es independiente del puerto OSC configurado en cada joystick.
DISCOVERY_PORT = 8001

# Puerto HTTP del ESP32
WEB_PORT = 80

# Tiempo sin recibir discovery antes de marcar offline
TIMEOUT = 5.0

# Timeout para consultar /config
HTTP_TIMEOUT = 1.5


# ============================================================
# JOYSTICK DATA
# ============================================================

joysticks = {}
lock = threading.Lock()


# ============================================================
# READ CONFIG FROM ESP32
# ============================================================

def read_joystick_config(ip):
    url = f"http://{ip}:{WEB_PORT}/config"

    try:
        with urllib.request.urlopen(
            url,
            timeout=HTTP_TIMEOUT
        ) as response:

            data = response.read().decode("utf-8")
            config = json.loads(data)

            return config

    except (
        urllib.error.URLError,
        urllib.error.HTTPError,
        TimeoutError,
        json.JSONDecodeError,
        OSError
    ):
        return None


# ============================================================
# UPDATE CONFIG
# ============================================================

def update_joystick_config(mac, ip):
    config = read_joystick_config(ip)

    if config is None:
        return

    with lock:
        if mac not in joysticks:
            return

        joystick = joysticks[mac]

        # Sólo actualizamos si la IP sigue siendo la misma.
        if joystick["ip"] != ip:
            return

        if "group" in config:
            joystick["group"] = str(config["group"])

        if "address" in config:
            joystick["address"] = str(config["address"])

        if "port" in config:
            joystick["port"] = int(config["port"])


# ============================================================
# OSC DISCOVERY LISTENER
# ============================================================

def listen_discovery():

    sock = socket.socket(
        socket.AF_INET,
        socket.SOCK_DGRAM
    )

    sock.setsockopt(
        socket.SOL_SOCKET,
        socket.SO_REUSEADDR,
        1
    )

    sock.setsockopt(
        socket.SOL_SOCKET,
        socket.SO_BROADCAST,
        1
    )

    sock.bind(
        (LISTEN_IP, DISCOVERY_PORT)
    )

    print(
        f"Listening for joystick discovery "
        f"on UDP port {DISCOVERY_PORT}..."
    )

    print("Turn on the joysticks.")
    print()

    while True:

        try:

            data, address = sock.recvfrom(4096)

            sender_ip = address[0]

            try:
                message = OscMessage(data)

            except Exception:
                continue

            # El discovery utiliza SIEMPRE /joystick
            if message.address != "/joystick":
                continue

            # Esperamos:
            #
            # [0] MAC
            # [1] IP
            # [2] GROUP
            # [3] OSC ADDRESS
            # [4] UDP PORT

            if len(message.params) < 5:
                continue

            mac = str(message.params[0])
            reported_ip = str(message.params[1])
            group = str(message.params[2])
            address_name = str(message.params[3])

            try:
                port = int(message.params[4])
            except (ValueError, TypeError):
                port = 0

            now = time.time()

            is_new = False

            with lock:

                if mac not in joysticks:

                    joystick_number = len(joysticks) + 1

                    joysticks[mac] = {
                        "number": joystick_number,
                        "mac": mac,
                        "ip": sender_ip,
                        "group": group,
                        "address": address_name,
                        "port": port,
                        "last_seen": now
                    }

                    is_new = True

                    print(
                        f"Joystick {joystick_number} detected: "
                        f"{mac} -> {sender_ip}"
                    )

                else:

                    joystick = joysticks[mac]

                    joystick["ip"] = sender_ip
                    joystick["last_seen"] = now
                    joystick["group"] = group
                    joystick["address"] = address_name
                    joystick["port"] = port

            # ------------------------------------------------
            # Confirm configuration through HTTP
            # ------------------------------------------------

            if is_new:

                threading.Thread(
                    target=update_joystick_config,
                    args=(mac, sender_ip),
                    daemon=True
                ).start()

        except KeyboardInterrupt:
            break

        except Exception as e:
            print(f"Discovery listener error: {e}")


# ============================================================
# DISPLAY
# ============================================================

def print_table():

    with lock:
        entries = list(joysticks.values())

    print("\033[2J\033[H", end="")

    print("==============================================================")
    print("                    JOYSTICK DISCOVERY")
    print("==============================================================")
    print()

    print("# : IP              : GROUP : ADDRESS  : PORT")
    print("--------------------------------------------------------------")

    if not entries:

        print("Waiting for joysticks...")

    else:

        for joystick in sorted(
            entries,
            key=lambda x: x["number"]
        ):

            age = time.time() - joystick["last_seen"]

            if age <= TIMEOUT:
                status = joystick["ip"]
            else:
                status = f"{joystick['ip']} (OFFLINE)"

            print(
                f"{joystick['number']:1} : "
                f"{status:16} : "
                f"{joystick['group']:5} : "
                f"{joystick['address']:8} : "
                f"{joystick['port']}"
            )

    print()
    print("--------------------------------------------------------------")


# ============================================================
# USER INTERFACE
# ============================================================

def user_interface():

    while True:

        time.sleep(1)

        print_table()

        with lock:

            available_numbers = [
                joystick["number"]
                for joystick in joysticks.values()
                if (
                    time.time()
                    - joystick["last_seen"]
                    <= TIMEOUT
                )
            ]

        if not available_numbers:

            print("No active joysticks detected.")
            print("Waiting...")
            continue

        print(
            "Enter joystick number to open its "
            "configuration (q to quit):"
        )

        choice = input("> ").strip()

        if choice.lower() == "q":

            print("Exiting.")
            break

        try:

            number = int(choice)

        except ValueError:

            print("Invalid input.")
            continue

        selected = None

        with lock:

            for joystick in joysticks.values():

                if joystick["number"] == number:

                    selected = joystick
                    break

        if selected is None:

            print("Joystick number not found.")
            continue

        if (
            time.time()
            - selected["last_seen"]
            > TIMEOUT
        ):

            print("That joystick is currently offline.")
            continue

        ip = selected["ip"]

        url = f"http://{ip}:{WEB_PORT}"

        print(f"Opening: {url}")

        webbrowser.open(url)


# ============================================================
# MAIN
# ============================================================

if __name__ == "__main__":

    listener = threading.Thread(
        target=listen_discovery,
        daemon=True
    )

    listener.start()

    try:

        user_interface()

    except KeyboardInterrupt:

        print("\nExiting.")