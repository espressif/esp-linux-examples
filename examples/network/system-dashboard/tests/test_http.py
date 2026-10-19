#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0

import concurrent.futures
import http.client
import json
import signal
import socket
import subprocess
import sys
import time


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def request(port, method, path):
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
    connection.request(method, path)
    response = connection.getresponse()
    body = response.read()
    headers = dict(response.getheaders())
    connection.close()
    return response.status, headers, body


def wait_ready(port):
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        try:
            if request(port, "GET", "/healthz")[0] == 204:
                return
        except OSError:
            time.sleep(0.05)
    raise RuntimeError("server did not become ready")


def validate_status(body):
    snapshot = json.loads(body)
    assert snapshot["schema_version"] == "1.0"
    assert snapshot["cpu"]["count"] >= 1
    assert snapshot["memory"]["total_bytes"] > 0
    assert snapshot["runtime"]["uptime_seconds"] > 0
    assert isinstance(snapshot["network"]["interfaces"], list)
    assert snapshot["system"]["kernel_release"]
    assert snapshot["system"]["os"]["family"] in {
        "Buildroot",
        "Linux",
        "OpenWrt",
        "Yocto",
    }


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f"usage: {sys.argv[0]} BINARY WEB_ROOT")
    binary, web_root = sys.argv[1:]
    port = free_port()
    process = subprocess.Popen(
        [
            binary,
            "--bind",
            "127.0.0.1",
            "--port",
            str(port),
            "--web-root",
            web_root,
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    try:
        wait_ready(port)

        status, headers, body = request(port, "GET", "/")
        assert status == 200
        assert headers["Content-Type"] == "text/html; charset=utf-8"
        assert b"ESP Linux System Dashboard" in body

        status, headers, body = request(port, "GET", "/api/v1/status")
        assert status == 200
        assert headers["Content-Type"] == "application/json"
        assert headers["Cache-Control"] == "no-store"
        assert headers["Connection"] == "close"
        validate_status(body)

        status, headers, body = request(port, "HEAD", "/api/v1/status")
        assert status == 200
        assert headers["Content-Type"] == "application/json"
        assert body == b""

        status, headers, body = request(port, "POST", "/api/v1/status")
        assert status == 405
        assert headers["Allow"] == "GET, HEAD"
        assert json.loads(body)["error"] == "method not allowed"

        for path in ("/missing", "/../etc/passwd", "/index.html"):
            status, headers, body = request(port, "GET", path)
            assert status == 404
            assert headers["Content-Type"] == "application/json"
            assert json.loads(body)["error"] == "not found"

        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            results = list(
                pool.map(
                    lambda _: request(port, "GET", "/api/v1/status"),
                    range(20),
                )
            )
        assert all(result[0] == 200 for result in results)
        for _, _, result_body in results:
            validate_status(result_body)
    finally:
        process.send_signal(signal.SIGTERM)
        try:
            stdout, stderr = process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            stdout, stderr = process.communicate()
            raise AssertionError("server did not stop after SIGTERM")
        if process.returncode != 0:
            raise AssertionError(
                f"server exited with {process.returncode}\n"
                f"stdout:\n{stdout}\nstderr:\n{stderr}"
            )
    print("HTTP integration tests passed")


if __name__ == "__main__":
    main()
