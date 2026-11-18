#!/bin/bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin

if [[ "$(uname -s)" != Darwin ]]; then
    printf 'Questo installer richiede macOS.\n' >&2
    exit 1
fi

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
bundle="BassDeamon.vst3"
destination="/Library/Audio/Plug-Ins/VST3"
target="$destination/$bundle"

if [[ ! -f "$bundle/Contents/MacOS/BassDeamon" ]]; then
    printf 'BassDeamon.vst3 mancante o incompleto. Estrai tutto lo ZIP.\n' >&2
    exit 1
fi

sudo mkdir -p "$destination"
staging="$(sudo mktemp -d "$destination/.BassDeamon-install.XXXXXX")"
trap 'sudo rm -rf -- "$staging"' EXIT
sudo ditto "$bundle" "$staging/$bundle"
sudo rm -rf -- "$target"
sudo mv "$staging/$bundle" "$target"

if ! sudo xattr -r -d com.apple.quarantine "$target" 2>/dev/null; then
    attributes="$(sudo xattr -r "$target")"
    if [[ "$attributes" == *com.apple.quarantine* ]]; then
        printf 'Non riesco a rimuovere la quarantena da %s\n' "$target" >&2
        exit 1
    fi
fi
printf 'BassDeamon installato in %s\n' "$target"
