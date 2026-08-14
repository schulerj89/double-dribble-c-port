#!/usr/bin/env python3
"""Small client for a locally running Ghidra MCP HTTP bridge.

The live server is intentionally external to this source-only repository. This
client provides connection diagnostics and bounded decompile/symbol requests
when the bridge is listening on localhost.
"""

import argparse
import json
import sys
import urllib.error
import urllib.request


def request(host: str, port: int, endpoint: str, payload: dict | None = None) -> dict:
    url = f"http://{host}:{port}/{endpoint.lstrip('/')}"
    data = None if payload is None else json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=5) as response:
            return json.loads(response.read().decode("utf-8"))
    except (urllib.error.URLError, json.JSONDecodeError) as error:
        return {"status": "error", "url": url, "message": str(error)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", default=13370, type=int)
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("status")
    symbols = subparsers.add_parser("symbols")
    symbols.add_argument("--filter", default="")
    decompile = subparsers.add_parser("decompile")
    decompile.add_argument("target")
    args = parser.parse_args()

    if args.command == "status":
        result = request(args.host, args.port, "status")
    elif args.command == "symbols":
        result = request(args.host, args.port, "symbols", {"filter": args.filter})
    else:
        result = request(args.host, args.port, "decompile", {"target": args.target})

    print(json.dumps(result, indent=2))
    return 0 if result.get("status") != "error" else 2


if __name__ == "__main__":
    sys.exit(main())

