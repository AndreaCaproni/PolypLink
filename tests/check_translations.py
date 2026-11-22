#!/usr/bin/env python3
"""PolypLink translation checks that do not depend on lrelease."""
import glob
import os
import re
import sys
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
I18N = os.path.join(ROOT, "i18n")
QRC = os.path.join(ROOT, "qml.qrc")

errors = []
ts_files = sorted(glob.glob(os.path.join(I18N, "polyplink_*.ts")))
if len(ts_files) < 20:
    errors.append(f"attesi almeno 20 file .ts, trovati {len(ts_files)}")

for path in ts_files:
    code = os.path.basename(path)[10:-3]
    try:
        root = ET.parse(path).getroot()
    except Exception as exc:
        errors.append(f"{code}: XML non valido: {exc}")
        continue
    for message in root.iter("message"):
        source = message.findtext("source")
        translation = message.find("translation")
        if not source:
            errors.append(f"{code}: messaggio senza source")
            continue
        if translation is None or not (translation.text or "").strip():
            errors.append(f"{code}: traduzione vuota per {source!r}")
        elif translation.get("type") == "unfinished":
            errors.append(f"{code}: traduzione unfinished per {source!r}")

qrc = open(QRC, encoding="utf-8").read()
for path in ts_files:
    qm = path[:-3] + ".qm"
    if not os.path.exists(qm):
        errors.append(f"manca {os.path.basename(qm)}")
    if os.path.basename(qm) not in qrc:
        errors.append(f"{os.path.basename(qm)} non e' in qml.qrc")

for name in ("jellylink_", "JellyLink", "jellyfin-link"):
    if name in qrc:
        errors.append(f"qml.qrc contiene ancora {name!r}")

if errors:
    print("ERRORI:")
    for error in errors:
        print(" -", error)
    sys.exit(1)

print(f"traduzioni ok: {len(ts_files)} file, nessuna unfinished")
