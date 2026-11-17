#!/bin/bash
set -euo pipefail

if [[ "$(uname -s)" != Darwin ]]; then
    echo "This installer requires macOS." >&2
    exit 1
fi

system_dir=/Library/Audio/Plug-Ins/VST3
user_dir="$HOME/Library/Audio/Plug-Ins/VST3"
plugin=BassDeamon.vst3

case "${1:-}" in
    "")
        if [[ -e "$system_dir/$plugin" && ! -e "$user_dir/$plugin" ]]; then
            destination="$system_dir"; opposite="$user_dir"; system_install=true
        else
            destination="$user_dir"; opposite="$system_dir"; system_install=false
        fi ;;
    --system) destination="$system_dir"; opposite="$user_dir"; system_install=true ;;
    *) echo "Usage: bash install.sh [--system]" >&2; exit 2 ;;
esac

run() {
    if "$system_install"; then
        sudo "$@"
    else
        "$@"
    fi
}

if [[ -e "$opposite/$plugin" || -L "$opposite/$plugin" ]]; then
    printf 'Trovate copie in entrambe le cartelle VST3. Rimuovi la duplicata prima di aggiornare: %s\n' "$opposite/$plugin" >&2
    exit 1
fi

source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source_bundle="$source_dir/$plugin"
if [[ ! -d "$source_bundle" ]]; then
    printf 'Missing %s. Extract the complete ZIP first.\n' "$source_bundle" >&2
    exit 1
fi
/usr/bin/codesign --verify --strict "$source_bundle"

run /bin/mkdir -p "$destination"
stage="$(run /usr/bin/mktemp -d "$destination/.BassDeamon-install.XXXXXX")"
target="$destination/$plugin"

cleanup() {
    if run /bin/test -e "$stage/previous" || run /bin/test -L "$stage/previous"; then
        if [[ ! -e "$target" && ! -L "$target" ]]; then
            run /bin/mv "$stage/previous" "$target" || return 1
        fi
    fi
    run /bin/rm -rf "$stage"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

run /usr/bin/ditto "$source_bundle" "$stage/$plugin"
run /bin/chmod -R a+rX,go-w "$stage/$plugin"
run /bin/chmod a+rx "$stage/$plugin/Contents/MacOS/BassDeamon"
run /usr/bin/xattr -r -d com.apple.quarantine "$stage/$plugin" 2>/dev/null || true
run /usr/bin/codesign --verify --strict "$stage/$plugin"
if [[ -e "$target" || -L "$target" ]]; then
    run /bin/mv "$target" "$stage/previous"
fi
run /bin/mv "$stage/$plugin" "$target"
if ! run /usr/bin/codesign --verify --strict "$target"; then
    run /bin/rm -rf "$target"
    printf 'Verifica fallita: ripristino della versione precedente.\n' >&2
    exit 1
fi
printf 'Installed %s\n' "$target"
