#!/usr/bin/env python3
"""Первичная прошивка донгла: проверка, резервная копия, запись, проверка записи.

Запуск без параметров задаёт вопросы по шагам. Готовая программа содержит прошивку внутри,
при запуске из исходников укажите файл: python flash.py --firmware firmware.factory.bin
Возврат заводской прошивки: python flash.py --restore папка_с_копией/dump-1.bin
"""
import argparse
import contextlib
import hashlib
import io
import locale
import os
import re
import sys
import time

FLASH_SIZE = 0x400000
BAUD = "115200"

TEXT = {
    "ru": {
        "intro": "Первичная прошивка донгла. Донгл должен быть снят с прибора и подключён через адаптер UART.",
        "ports": "Найденные порты:",
        "noport": "Порт не найден. Подключите адаптер и установите его драйвер, затем запустите программу снова.",
        "choose": "Номер порта: ",
        "boot": "Отключите питание донгла, зажмите кнопку на торце, подайте питание, отпустите кнопку. Затем нажмите Enter.",
        "check": "Проверяю чип...",
        "nochip": "Донгл не ответил. Проверьте режим загрузки, GND и поменяйте местами TX и RX.",
        "wrong": "Неподходящее устройство, нужен ESP32-C3 с flash 4 МБ. Обнаружено:",
        "ok": "Чип подходит.",
        "dump": "Снимаю резервную копию %d из 2, около 6 минут...",
        "mismatch": "Копии не совпали, запись отменена. Проверьте провода и запустите снова.",
        "saved": "Резервная копия сохранена: %s",
        "token": "ВАЖНО: в копии есть ваши личные данные (токен). Не отправляйте этот файл никому и храните его: только он вернёт заводскую прошивку.",
        "confirm": "Записать новую прошивку? Заводская будет стёрта. Введите yes: ",
        "cancel": "Отменено, донгл не изменён.",
        "erase": "Стираю память...",
        "write": "Записываю прошивку...",
        "verify": "Проверяю запись...",
        "done": "Готово. Отключите питание и вставьте донгл в прибор. Подключитесь к сети esphome-dongle-XXXXXX и задайте Wi-Fi на открывшейся странице. Там же будет ключ для Home Assistant.",
        "restore": "Вернуть заводскую прошивку из %s? Текущая будет стёрта. Введите yes: ",
        "restored": "Заводская прошивка возвращена. Отключите питание и вставьте донгл обратно.",
        "badsum": "Контрольная сумма копии не совпала, восстановление отменено.",
        "nofw": "Файл прошивки не найден. Укажите его параметром --firmware.",
        "failed": "Ошибка: %s",
    },
    "en": {
        "intro": "First flash of the dongle. It must be removed from the appliance and connected through a UART adapter.",
        "ports": "Serial ports found:",
        "noport": "No port found. Connect the adapter and install its driver, then run again.",
        "choose": "Port number: ",
        "boot": "Power off the dongle, hold the button on its end, power on, release the button. Then press Enter.",
        "check": "Checking the chip...",
        "nochip": "The dongle did not reply. Check boot mode, GND and swap TX and RX.",
        "wrong": "Wrong device, ESP32-C3 with 4 MB flash is required. Detected:",
        "ok": "Chip is suitable.",
        "dump": "Reading backup %d of 2, about 6 minutes...",
        "mismatch": "Backups differ, writing cancelled. Check the wires and run again.",
        "saved": "Backup saved: %s",
        "token": "IMPORTANT: the backup contains your personal data (a token). Do not send this file to anyone and keep it: only it can restore the stock firmware.",
        "confirm": "Write the new firmware? The stock one will be erased. Type yes: ",
        "cancel": "Cancelled, the dongle is unchanged.",
        "erase": "Erasing flash...",
        "write": "Writing firmware...",
        "verify": "Verifying...",
        "done": "Done. Power off and plug the dongle into the appliance. Join the esphome-dongle-XXXXXX network and set your Wi-Fi on the page that opens. The Home Assistant key is there too.",
        "restore": "Restore stock firmware from %s? The current one will be erased. Type yes: ",
        "restored": "Stock firmware restored. Power off and plug the dongle back in.",
        "badsum": "Backup checksum does not match, restore cancelled.",
        "nofw": "Firmware file not found. Pass it with --firmware.",
        "failed": "Error: %s",
    },
}


def language():
    name = (locale.getlocale()[0] or os.environ.get("LANG") or "").lower()
    return "ru" if name.startswith("ru") or "russian" in name else "en"


T = TEXT[language()]


class FlashError(Exception):
    pass


def run_esptool(port, *command):
    """Запускает esptool в этом же процессе, поэтому работает и в готовой программе."""
    import esptool

    argv = ["--port", port, "--baud", BAUD, "--before", "no-reset", "--after", "no-reset", *command]
    out = io.StringIO()
    try:
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            esptool.main(argv)
    except SystemExit as error:
        if error.code not in (0, None):
            raise FlashError(out.getvalue()[-400:]) from error
    except Exception as error:
        raise FlashError(f"{error}\n{out.getvalue()[-400:]}") from error
    return out.getvalue()


def choose_port():
    from serial.tools import list_ports

    ports = sorted(list_ports.comports(), key=lambda port: port.device)
    if not ports:
        raise FlashError(T["noport"])
    if len(ports) == 1:
        print(f"{T['ports']} {ports[0].device} ({ports[0].description})")
        return ports[0].device
    print(T["ports"])
    for number, port in enumerate(ports, 1):
        print(f"  {number}. {port.device} ({port.description})")
    while True:
        answer = input(T["choose"]).strip()
        if answer.isdigit() and 1 <= int(answer) <= len(ports):
            return ports[int(answer) - 1].device


def check_chip(port):
    print(T["check"])
    try:
        report = run_esptool(port, "flash-id")
    except FlashError as error:
        raise FlashError(T["nochip"]) from error
    chip = re.search(r"ESP32-C3", report) is not None
    size = re.search(r"flash size:\s*4\s*MB", report, re.IGNORECASE) is not None
    if not (chip and size):
        raise FlashError(f"{T['wrong']}\n{report[-400:]}")
    print(T["ok"])


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def make_backup(port, folder):
    os.makedirs(folder, exist_ok=False)
    first, second = os.path.join(folder, "dump-1.bin"), os.path.join(folder, "dump-2.bin")
    for number, path in ((1, first), (2, second)):
        print(T["dump"] % number)
        run_esptool(port, "read-flash", "0", hex(FLASH_SIZE), path)
    sum1, sum2 = sha256(first), sha256(second)
    if sum1 != sum2 or os.path.getsize(first) != FLASH_SIZE:
        raise FlashError(T["mismatch"])
    with open(first + ".sha256", "w", encoding="ascii") as out:
        out.write(sum1 + "\n")
    print(T["saved"] % os.path.abspath(folder))
    print(T["token"])


def write_image(port, image):
    print(T["erase"])
    run_esptool(port, "erase-flash")
    print(T["write"])
    run_esptool(port, "write-flash", "0", image)
    print(T["verify"])
    run_esptool(port, "verify-flash", "0", image)


def bundled_firmware():
    base = getattr(sys, "_MEIPASS", os.path.dirname(os.path.abspath(__file__)))
    path = os.path.join(base, "firmware.factory.bin")
    return path if os.path.exists(path) else None


def restore(port, image):
    sum_file = image + ".sha256"
    if os.path.exists(sum_file):
        with open(sum_file, encoding="ascii") as source:
            if source.read().strip() != sha256(image):
                raise FlashError(T["badsum"])
    if input(T["restore"] % image).strip().lower() != "yes":
        print(T["cancel"])
        return
    write_image(port, image)
    print(T["restored"])


def flash(port, firmware):
    make_backup(port, f"esphome-dongle-backup-{time.strftime('%Y%m%d-%H%M%S')}")
    if input(T["confirm"]).strip().lower() != "yes":
        print(T["cancel"])
        return
    write_image(port, firmware)
    print(T["done"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware")
    parser.add_argument("--restore")
    args = parser.parse_args()
    print(T["intro"])
    try:
        firmware = args.firmware or bundled_firmware()
        if not args.restore and not (firmware and os.path.exists(firmware)):
            raise FlashError(T["nofw"])
        port = choose_port()
        input(T["boot"])
        check_chip(port)
        if args.restore:
            restore(port, args.restore)
        else:
            flash(port, firmware)
    except FlashError as error:
        print(T["failed"] % error)
        return 1
    except KeyboardInterrupt:
        print(T["cancel"])
        return 1
    return 0


if __name__ == "__main__":
    code = main()
    if getattr(sys, "frozen", False):
        input()
    sys.exit(code)
