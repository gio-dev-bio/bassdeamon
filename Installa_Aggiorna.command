#!/bin/bash
set -euo pipefail
script_dir="$(cd -- "$(/usr/bin/dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec /bin/bash "$script_dir/install.sh"
