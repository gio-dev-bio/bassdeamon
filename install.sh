#!/bin/bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin

if [[ "$(uname -s)" != Darwin ]]; then
    printf 'Questo installer richiede macOS.\n' >&2
    exit 1
fi

source_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source_bundle="$source_dir/BassDeamon.vst3"
destination="/Library/Audio/Plug-Ins/VST3"
target="$destination/BassDeamon.vst3"

if [[ ! -f "$source_bundle/Contents/MacOS/BassDeamon" ]]; then
    printf 'BassDeamon.vst3 mancante o incompleto. Estrai tutto lo ZIP e riprova.\n' >&2
    exit 1
fi

sudo /bin/mkdir -p "$destination"
staging="$(sudo /usr/bin/mktemp -d "$destination/.BassDeamon-install.XXXXXX")"
trap 'sudo /bin/rm -rf -- "$staging"' EXIT
sudo /usr/bin/ditto "$source_bundle" "$staging/BassDeamon.vst3"
sudo /bin/rm -rf -- "$target"
sudo /bin/mv "$staging/BassDeamon.vst3" "$target"

if ! quarantine_error="$(sudo /usr/bin/xattr -r -d com.apple.quarantine "$target" 2>&1)"; then
    attributes="$(sudo /usr/bin/xattr -r "$target")"
    if [[ "$attributes" == *com.apple.quarantine* || "$quarantine_error" != *"No such xattr: com.apple.quarantine"* ]]; then
        printf '%s\n' "$quarantine_error" >&2
        exit 1
    fi
fi
printf 'BassDeamon installato in %s\n' "$target"
