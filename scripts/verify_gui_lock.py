#!/usr/bin/env python3
"""Reject unnoticed UI drift from the visually reviewed source revision."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parent.parent
lock = json.loads((root / 'docs/RAW_GUI_LOCK.json').read_text())
changed = [name for name, expected in lock['sha256'].items()
           if hashlib.sha256((root / name).read_bytes()).hexdigest() != expected]
if changed:
    raise SystemExit('GUI visual lock mismatch: ' + ', '.join(changed)
                     + '. Render Main and Advanced and compare with the approved reference before updating the lock.')
print('PASS reviewed GUI source and asset identity')
