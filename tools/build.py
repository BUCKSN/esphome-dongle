#!/usr/bin/env python3
"""Собирает прошивку и кладёт файлы в папку firmware/ в корне проекта.

Запуск той же средой Python, где установлен ESPHome:
  python tools/build.py [конфигурация]        по умолчанию ac-aux
  python tools/build.py --clean               ещё и удаляет старые файлы из firmware/
"""
import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("config", nargs="?", default="ac-aux")
    parser.add_argument("--version", default="dev")
    parser.add_argument("--clean", action="store_true")
    args = parser.parse_args()

    devices = ROOT / "devices"
    command = [sys.executable, "-m", "esphome", "-s", "fw_version", args.version, "compile", f"{args.config}.yaml"]
    if subprocess.run(command, cwd=devices, check=False).returncode != 0:
        return 1

    images = sorted(devices.glob(".esphome/build/*/build/firmware.ota.bin"), key=lambda p: p.stat().st_mtime)
    if not images:
        print("Файл прошивки не найден")
        return 1
    ota = images[-1]
    marker = re.search(rb"DONGLEFW1\|board=[\w.-]+\|device=([\w.-]+)\|version=([\w.-]+)\|", ota.read_bytes())
    if marker is None:
        print("В образе нет метки совместимости")
        return 1

    out = ROOT / "firmware"
    out.mkdir(exist_ok=True)
    if args.clean:
        for old in out.glob("esphome-dongle-*.bin"):
            old.unlink()
    device, version = marker[1].decode(), marker[2].decode()
    for kind, source in (("ota", ota), ("factory", ota.with_name("firmware.factory.bin"))):
        target = out / f"esphome-dongle-{device}-{version}.{kind}.bin"
        shutil.copy2(source, target)
        print(target)
    return 0


if __name__ == "__main__":
    sys.exit(main())
