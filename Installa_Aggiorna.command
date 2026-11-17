#!/bin/bash
set -u
cd "$(dirname "$0")" || exit 1
printf '\nBassDeamon — installazione / aggiornamento\n\n'
if /usr/bin/pgrep -if '/Cubase[^/]*/Contents/MacOS/' >/dev/null; then
    printf 'Chiudi Cubase e riapri questo file.\n'
    read -r -p 'Premi Invio per chiudere… ' _
    exit 1
fi
if [[ -d /Library/Audio/Plug-Ins/VST3/BassDeamon.vst3 ]]; then
    /bin/bash ./install.sh --system
    result=$?
else
    /bin/bash ./install.sh
    result=$?
fi
if [[ $result -eq 0 ]]; then
    printf '\nInstallazione completata. Puoi riaprire Cubase.\n'
else
    printf '\nInstallazione non completata (codice %s).\n' "$result"
fi
read -r -p 'Premi Invio per chiudere… ' _
exit "$result"
