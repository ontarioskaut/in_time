#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os
import json
import argparse
import time
from datetime import datetime
import tempfile

from api import TimeServerAPI
import config


# ---------------- Persistent state ----------------

CORE_MODE = False
AUTHORIZED_MODE = False
USER = None

def _state_file_path() -> str:
    return os.path.expanduser(getattr(config, "CLI_STATE_FILE", "/tmp/.intime_cli_state.json"))

def _load_state() -> None:
    global CORE_MODE, AUTHORIZED_MODE, USER
    path = _state_file_path()
    if not os.path.exists(path):
        return
    try:
        with open(path, "r", encoding="utf-8") as f:
            obj = json.load(f)
        CORE_MODE = bool(obj.get("CORE_MODE", CORE_MODE))
        AUTHORIZED_MODE = bool(obj.get("AUTHORIZED_MODE", AUTHORIZED_MODE))
        USER = obj.get("USER", USER)
        if USER is not None:
            USER = str(USER)
    except Exception:
        pass

def _save_state() -> None:
    path = _state_file_path()
    state = {
        "CORE_MODE": CORE_MODE,
        "AUTHORIZED_MODE": AUTHORIZED_MODE,
        "USER": USER,
    }
    # Ensure parent dir exists
    d = os.path.dirname(path)
    if d and not os.path.exists(d):
        try:
            os.makedirs(d, exist_ok=True)
        except Exception:
            return
    # Atomic-ish write
    try:
        fd, tmp = tempfile.mkstemp(prefix=".intime_state_", dir=d if d else None)
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(state, f, ensure_ascii=False, indent=2)
        os.replace(tmp, path)
    except Exception:
        # Best-effort only
        try:
            if os.path.exists(tmp):
                os.unlink(tmp)
        except Exception:
            pass


# ---------------- Helpers ----------------

def compare_input_to_int(text: str, ref: int):
    t = text.strip().replace("_", "")
    sign = -1 if t[:1] == "-" else 1
    if t[:1] in "+-":
        t = t[1:]
    if not t or not t.isdigit():
        return False, None
    v = sign * int(t)
    return True, (0 if v == ref else (-1 if v < ref else 1))

def input_equals_int(ref: int) -> bool:
    text = input("Odpověď: ")
    ok, cmpv = compare_input_to_int(text, ref)
    return ok and cmpv == 0

def _yes_no(prompt: str) -> bool:
    try:
        ans = input(f"{prompt} [y/N]: ").strip().lower()
    except EOFError:
        return False
    return ans in ("y", "yes")

def _parse_bool(s: str) -> bool:
    v = s.strip().lower()
    if v in ("1", "true", "t", "yes", "y", "on"):
        return True
    if v in ("0", "false", "f", "no", "n", "off"):
        return False
    raise argparse.ArgumentTypeError(f"Invalid boolean: {s}")

def _print_table(headers, rows):
    cols = len(headers)
    widths = [len(str(h)) for h in headers]
    for r in rows:
        for i in range(cols):
            widths[i] = max(widths[i], len(str(r[i])))
    def fmt_row(row):
        return " | ".join(str(row[i]).ljust(widths[i]) for i in range(cols))
    sep = "-+-".join("-" * w for w in widths)
    print(fmt_row(headers))
    print(sep)
    for r in rows:
        print(fmt_row(r))


# ---------------- Commands ----------------

def cmd_get_active(api: TimeServerAPI) -> int:
    print(api.get_system_active())
    return 0

def cmd_list_user_times(api: TimeServerAPI) -> int:
    print("Přijímám data...")
    data = api.list_user_times()
    if data is False:
        return 1

    # Expecting: [{'name': str, 'offset': int, 'start': 'YYYY-MM-DDTHH:MM:SS'}, ...]
    now = datetime.now()
    rows = []
    for item in data:
        try:
            name = item["name"]
            offset = int(item["offset"])
            start = datetime.fromisoformat(item["start"])
            elapsed = int((now - start).total_seconds())
            if elapsed < 0:
                elapsed = 0
            # remaining
            total = offset - elapsed
            if total < 0:
                total = 0
            fmt = TimeServerAPI.format_time(total)
            if total == 0:
                fmt = f"DEAD: {fmt}"
            rows.append([
                name,
                item["start"],
                offset,
                elapsed,
                total,
                fmt,
            ])
        except Exception as e:
            rows.append([str(item), "-", "-", "-", "-", f"ERR: {e}"])

    headers = ["Jméno", "Start", "Offset(s)", "Uplynulo(s)", "Zbývá(s)", "Výsledný čas(fmt)"]
    _print_table(headers, rows)
    return 0

def cmd_list_users(api: TimeServerAPI) -> int:
    print("Přijímám data...")
    data = api.list_users()
    if data is False:
        return 1
    # Structure: [[id, tag, name, acro, offset, start, active], ...]
    headers = ["ID", "Tag", "Jméno", "Acr", "Offset(s)", "Start", "Aktivní"]
    _print_table(headers, data)
    return 0

def cmd_list_categories(api: TimeServerAPI) -> int:
    print("Přijímám data...")
    data = api.list_user_cat()
    if data is False:
        return 1
    headers = ["ID", "Jméno"]
    _print_table(headers, data)
    return 0

def cmd_get_logs(api: TimeServerAPI) -> int:
    print("Přijímám data...")
    data = api.list_logs()
    if data is False:
        return 1
    headers = ["ID", "Časové razítko", "UserID", "Změna", "Poznámka"]
    _print_table(headers, data)
    return 0

def cmd_get_allocated_time(api: TimeServerAPI) -> int:
    print("Přijímám data...")
    print("Celkový dostupný čas je:", api.get_time_allocation(), "vteřin")
    return 0

def cmd_apply_user_offset(api: TimeServerAPI, user_id: int, offset: int) -> int:
    print("Upravuji offsety uživatelů...")
    ok = api.apply_offset_user(user_id, offset)
    print("Operace byla úspěšná" if ok else "Operace selhala")
    return 0 if ok else 1

def cmd_apply_user_cat(api: TimeServerAPI, cat_id: int, offset: int) -> int:
    print("Upravuji offset kategorie...")
    ok = api.apply_offset_cat(cat_id, offset)
    print("Operace byla úspěšná" if ok else "Operace selhala")
    return 0 if ok else 1

def cmd_set_core_mode(api: TimeServerAPI) -> int:
    global CORE_MODE
    print("CORE MODE vám umožní povolit úpravy, které nejsou za běžných podmínek dostupné. Nesprávné zacházení může způsobit újmu na technickém vybavení i životech obyvatel! Používejte s rozvahou.")
    if not _yes_no("Přijímáte zodpovědnost?"):
        print("Zrušeno uživatelem.")
        return 2
    if not _yes_no("Nastavit mód ovládání na CORE_MODE?"):
        print("Zrušeno uživatelem.")
        return 2
    print("Kontrolní otázka: Jaký je současný dostupný celkový čas ve vteřinách?")
    if not input_equals_int(api.get_time_allocation()):
        print("Nesprávná odpověď, zrušeno.")
        return 2
    CORE_MODE = True
    _save_state()
    print("CORE_MODE=True")
    print("Mód CORE_MODE povolen, jednáte v rizikové zóně!")
    return 0

def cmd_authorize() -> int:
    global AUTHORIZED_MODE
    print("Autorizací se dostanete do kritických částí systému, na kterých závisí samotné životy občanů (včetně Vás). Jednejte s maximální opatrností a rozvahou! Změny jsou absolutně nevratné!")
    if not _yes_no("Přijímáte zodpovědnost?"):
        print("Zrušeno uživatelem.")
        return 2
    if (not CORE_MODE) or (USER is None):
        print("Nenacházíte se v odpovídajícím módu operací nebo není nastaven operující uživatel.")
        print("Operace zrušena!")
        return 2
    print("Pro pokračování zadejte přihlašovací údaje")
    print("Uživatel:", USER)
    password = input("Heslo: ")
    unlock = getattr(config, "UNLOCK_CODE", "")
    if password != unlock:
        print("Zadáno nesprávné heslo. Incident byl reportován!")
        return 2
    AUTHORIZED_MODE = True
    _save_state()
    print("AUTHORIZED_MODE=True")
    print("Mód AUTHORIZED_MODE povolen, jednáte v kritické zóně!")
    return 0

def cmd_set_user(api: TimeServerAPI, name: str) -> int:
    global USER
    print("Zadáním jména se přibližujete restriktivní zóně In-Time serveru. Opravdu si přejete pokračovat, i když každá změna může být fatální?")
    if not _yes_no("Přijímáte zodpovědnost?"):
        print("Zrušeno uživatelem.")
        return 2
    users = api.list_user_times()
    num_users = len(users)+1 if isinstance(users, (list, dict)) else 0
    print("Kontrolní otázka: Kolik je momentálně v časovém systému uživatelů?")
    if not input_equals_int(num_users):
        print("Nesprávná odpověď, zrušeno. Pozor: Přidáno do databáze neoprávněných přístupů!")
        return 2
    if name != getattr(config, "UNLOCK_USER", name):
        print("Zadaný uživatel neexistuje. Pokus o přihlášení byl reportován!")
        return 2
    USER = name
    _save_state()
    print(f"USER={USER}")
    print("Uživatel uložen!")
    return 0

def cmd_set_active(api: TimeServerAPI, state: bool) -> int:
    if not AUTHORIZED_MODE:
        print("Pro vykonání této operace potřebujete vyšší oprávnění!")
        return 2
    if not _yes_no(f"Opravdu si přejete nastavit stav systému na {state}?"):
        print("Zrušeno uživatelem.")
        return 2
    if not _yes_no("Jste si opravdu vědomi potenciálních rizik? Tento úkon může způsobit kolaps celého systému i společnosti."):
        print("Zrušeno uživatelem.")
        return 2
    if not _yes_no("Změny budou nevratné a fatální! Pokračovat?"):
        print("Zrušeno uživatelem.")
        return 2
    print("Nastavování...")
    time.sleep(2)
    print("Synchronizace...")
    time.sleep(5)
    print("Poslední úpravy...")
    time.sleep(2)
    ok = api.set_active(state)
    print("Operace proběhla úspěšně" if ok else "Operace selhala")
    print("Stav systému nastaven na:", state)
    if not state:
        print("Pozor, časový systém úplně zastaven. Očekávaná nestabilita!")
        print(r"""
          ____                  _       __                           
         / ___|   _   _   ___  | |_    /_/   _ __ ___                
         \___ \  | | | | / __| | __|  / _ \ | '_ ` _ \               
          ___) | | |_| | \__ \ | |_  |  __/ | | | | | |              
         |____/   \__, | |___/  \__|  \___| |_| |_| |_|              
                   |___/                                              
                      _                                    _ 
          ____   __ _   ___  | |_    __ _  __   __   ___   _ __   | |
         |_  /  / _` | / __| | __|  / _` | \ \ / /  / _ \ | '_ \  | |
          / /  | (_| | \__ \ | |_  | (_| |  \ V /  |  __/ | | | | |_|
         /___|  \__,_| |___/  \__|  \__,_|   \_/    \___| |_| |_| (_)
        """)

    return 0 if ok else 1

def cmd_split_allocated_time(api: TimeServerAPI) -> int:
    if not AUTHORIZED_MODE:
        print("Pro vykonání této operace potřebujete vyšší oprávnění!")
        return 2
    if not _yes_no("Rozdělit dostupný čas rovnoměrně mezi uživatele?"):
        print("Zrušeno uživatelem.")
        return 2
    ok = api.split_allocated_evenly()
    print("Operace byla úspěšná" if ok else "Operace selhala")
    return 0 if ok else 1


# ---------------- Argparse ----------------

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="intime",
        description="In-Time server configuration utility (with persistent local state).",
    )

    mx = p.add_mutually_exclusive_group(required=True)
    mx.add_argument("--get_active", action="store_true", help="Print True/False")
    mx.add_argument("--list_user_times", action="store_true", help="Print computed user times table")
    mx.add_argument("--list_users", action="store_true", help="Print users table")
    mx.add_argument("--list_categories", action="store_true", help="Print categories table")
    mx.add_argument("--get_logs", action="store_true", help="Print logs table")
    mx.add_argument("--get_allocated_time", action="store_true", help="Print allocated time")

    mx.add_argument("--apply_user_offset", nargs=2, metavar=("USER_ID", "OFFSET"), help="Apply offset to user")
    mx.add_argument("--apply_user_cat", nargs=2, metavar=("CAT_ID", "OFFSET"), help="Apply offset to category")

    mx.add_argument("--set_core_mode", action="store_true", help="Set CORE_MODE=True (persisted)")
    mx.add_argument("--authorize", action="store_true", help="Set AUTHORIZED_MODE=True (persisted)")
    mx.add_argument("--set_user", metavar="USER_NAME", help="Set USER variable (persisted)")

    mx.add_argument("--set_active", metavar="BOOL", help="Confirm, then call set_active(True/False)")
    mx.add_argument("--split_allocated_time", action="store_true", help="Confirm, then split allocated time evenly")

    p.add_argument("--verify-ssl", action="store_true", help="Verify TLS certs (if base URL is https)")
    p.add_argument("--timeout", type=int, default=6, help="HTTP timeout (seconds)")
    p.add_argument("--base-url", default=None, help="Override config.TIMESERVER_URL")

    return p


def main(argv=None) -> int:
    _load_state()  # restore persisted CORE_MODE/AUTHORIZED_MODE/USER

    args = build_parser().parse_args(argv)
    api = TimeServerAPI(base_url=args.base_url, verify_ssl=args.verify_ssl, timeout=args.timeout)

    if args.get_active:
        return cmd_get_active(api)

    if args.list_user_times:
        return cmd_list_user_times(api)

    if args.list_users:
        return cmd_list_users(api)

    if args.list_categories:
        return cmd_list_categories(api)

    if args.get_logs:
        return cmd_get_logs(api)

    if args.get_allocated_time:
        return cmd_get_allocated_time(api)

    if args.apply_user_offset:
        uid, off = args.apply_user_offset
        return cmd_apply_user_offset(api, int(uid), int(off))

    if args.apply_user_cat:
        cid, off = args.apply_user_cat
        return cmd_apply_user_cat(api, int(cid), int(off))

    if args.set_core_mode:
        return cmd_set_core_mode(api)

    if args.authorize:
        return cmd_authorize()

    if args.set_user is not None:
        return cmd_set_user(api, args.set_user)

    if args.set_active is not None:
        state = _parse_bool(args.set_active)
        return cmd_set_active(api, state)

    if args.split_allocated_time:
        return cmd_split_allocated_time(api)

    return 2


if __name__ == "__main__":
    sys.exit(main())

