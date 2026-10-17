#!/usr/bin/env bash
# Quick launcher for Atlas Local AI Assistant
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 "${SCRIPT_DIR}/main.py" "$@"
