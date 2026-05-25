import concurrent.futures
import socket
from typing import Optional


DEFAULT_BASE = "192.168.15"
START_HOST = 1
END_HOST = 200
DEFAULT_PORT = 80
DEFAULT_TIMEOUT = 1.0
MAX_WORKERS = 64
APP_SIGNATURES = (
    "Controle IR ESP32",
    "/capturar",
    "/ligar-tv",
    "/reset-wifi",
)


def fetch_http(ip: str, port: int, timeout: float) -> Optional[str]:
    try:
        with socket.create_connection((ip, port), timeout=timeout) as conn:
            conn.settimeout(timeout)
            request = (
                f"GET / HTTP/1.1\r\n"
                f"Host: {ip}\r\n"
                f"Connection: close\r\n"
                f"User-Agent: detect.py\r\n\r\n"
            )
            conn.sendall(request.encode("ascii"))
            chunks = []
            while True:
                chunk = conn.recv(4096)
                if not chunk:
                    break
                chunks.append(chunk)
    except (OSError, socket.timeout):
        return None

    if not chunks:
        return None

    return b"".join(chunks).decode("iso-8859-1", errors="replace")


def scan_host(ip: str) -> Optional[tuple[str, str]]:
    response = fetch_http(ip, DEFAULT_PORT, DEFAULT_TIMEOUT)
    if response is None:
        return None

    lines = response.splitlines()
    if not lines:
        return None

    status_line = lines[0].strip()
    if " 200 " not in status_line:
        return None

    if not all(signature in response for signature in APP_SIGNATURES):
        return None

    return ip, status_line


def main() -> None:
    print(
        f"Varrendo {DEFAULT_BASE}.{START_HOST} ate {DEFAULT_BASE}.{END_HOST} "
        f"na porta {DEFAULT_PORT}..."
    )

    found = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
        futures = {
            executor.submit(scan_host, f"{DEFAULT_BASE}.{host}"): f"{DEFAULT_BASE}.{host}"
            for host in range(START_HOST, END_HOST + 1)
        }
        for future in concurrent.futures.as_completed(futures):
            result = future.result()
            if result is None:
                continue
            found.append(result)
            ip, status = result
            print(f"{ip} -> {status}")

    if not found:
        print("Nenhum host retornou HTTP 200 na porta 80.")
        return

    print(f"\nTotal encontrado: {len(found)} host(s) com HTTP 200 na porta 80.")


if __name__ == "__main__":
    main()
