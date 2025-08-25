import json
import ssl
from urllib.parse import urljoin, urlencode
from urllib.request import Request, urlopen
from urllib.error import URLError, HTTPError

import config


class TimeServerAPI:
    def __init__(self, base_url: str | None = None, verify_ssl: bool = False, timeout: int = 6):
        base = (base_url or getattr(config, "TIMESERVER_URL", "")).rstrip("/") + "/"
        self.base_url = base
        self.timeout = timeout

        if base.startswith("https://"):
            self.ssl_ctx = ssl.create_default_context() if verify_ssl else ssl._create_unverified_context()
        else:
            self.ssl_ctx = None

    def _full_url(self, endpoint: str) -> str:
        return urljoin(self.base_url, endpoint.lstrip("/"))

    @staticmethod
    def _to_form_scalar(v) -> str:
        if isinstance(v, bool):
            return "True" if v else "False"
        if v is None:
            return ""
        return str(v)

    def _encode_query(self, data: dict | None) -> str:
        if not data:
            return ""
        items = []
        for k, v in data.items():
            if isinstance(v, (list, tuple)):
                for x in v:
                    items.append((k, self._to_form_scalar(x)))
            else:
                items.append((k, self._to_form_scalar(v)))
        return urlencode(items)

    def _encode_form(self, data: dict | None) -> bytes | None:
        if data is None:
            return None
        return self._encode_query(data).encode("utf-8")

    def send_request(self, endpoint: str, method: str = "GET",
                     data: dict | None = None, mode: str = "form"):
        url = self._full_url(endpoint)
        headers = {"Accept": "application/json"}
        body = None
        m = method.upper()

        if m == "GET":
            if data:
                url += ("&" if "?" in url else "?") + self._encode_query(data)
        elif m in ("POST", "PUT", "PATCH", "DELETE"):
            if mode == "form":
                body = self._encode_form(data)
                headers["Content-Type"] = "application/x-www-form-urlencoded; charset=utf-8"
            elif mode == "json":
                obj = {} if data is None else data
                body = json.dumps(obj, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
                headers["Content-Type"] = "application/json; charset=utf-8"
            else:
                if isinstance(data, (bytes, bytearray)):
                    body = bytes(data)
                elif isinstance(data, str):
                    body = data.encode("utf-8")
                elif isinstance(data, dict):
                    body = self._encode_form(data)
                    headers["Content-Type"] = "application/x-www-form-urlencoded; charset=utf-8"
                else:
                    body = None

        req = Request(url, data=body, headers=headers, method=m)

        try:
            with urlopen(req, timeout=self.timeout, context=self.ssl_ctx) as resp:
                raw = resp.read()
                charset = resp.headers.get_content_charset() or "utf-8"
                text = raw.decode(charset, errors="replace")
                ctype = resp.headers.get("Content-Type", "")
                if "application/json" in ctype:
                    return json.loads(text)
                try:
                    return json.loads(text)
                except Exception:
                    return text
        except (HTTPError, URLError, ssl.SSLError) as e:
            print(e)
            return False

    @staticmethod
    def format_time(seconds: int | float) -> str:
        # Days:HH:MM:SS
        s = int(seconds) if seconds >= 0 else 0
        days = s // 86400
        s -= days * 86400
        hours = s // 3600
        s -= hours * 3600
        mins = s // 60
        secs = s - mins * 60
        return f"{days}:{hours:02d}:{mins:02d}:{secs:02d}"

    @staticmethod
    def _to_bool(value) -> bool:
        if isinstance(value, bool):
            return value
        if isinstance(value, (int, float)):
            return value != 0
        if isinstance(value, str):
            v = value.strip().lower()
            return v in ("true", "1", "yes", "on", "active")
        return False

    @staticmethod
    def _to_int(value) -> int:
        if isinstance(value, int):
            return value
        if isinstance(value, float):
            return int(value)
        if isinstance(value, str):
            v = value.strip()
            if v.startswith("-"):
                vv = v[1:]
                if vv.isdigit():
                    return -int(vv)
            if v.isdigit():
                return int(v)
        return 0

    def get_system_active(self) -> bool:
        # /api/misc/get_active [GET]
        resp = self.send_request("misc/get_active", "GET")
        if resp is False:
            return False
        if isinstance(resp, dict):
            for k in ("active", "is_active", "system_active"):
                if k in resp:
                    return self._to_bool(resp[k])
        return self._to_bool(resp)

    def list_user_times(self):
        # /api/display/show_times [GET]
        return self.send_request("display/show_times", "GET")

    def list_users(self):
        # /api/admin/list_users [GET]
        return self.send_request("admin/list_users", "GET")

    def list_user_cat(self):
        # /api/admin/list_categories [GET]
        return self.send_request("admin/list_categories", "GET")

    def list_logs(self):
        # /api/misc/get_logs [GET]
        return self.send_request("misc/get_logs", "GET")

    def apply_offset_user(self, user_id: int, offset: int) -> bool:
        # /api/admin/bulk_add_user_time [POST]
        payload = {"user_ids": [user_id], "time_offset": str(offset)}
        resp = self.send_request("admin/bulk_add_user_time", "POST", payload, "json")
        if resp is False:
            return False
        return True

    def apply_offset_cat(self, cat_id: int, offset: int) -> bool:
        # /api/admin/bulk_add_user_time_category [POST]
        payload = {"ids": [cat_id], "time_offset": str(offset)}
        resp = self.send_request("admin/bulk_add_user_time_category", "POST", payload, "json")
        if resp is False:
            return False
        return True

    def set_active(self, new_state: bool) -> bool:
        # /api/misc/set_active [POST], state as "True"/"False" string
        state_str = "True" if new_state else "False"
        resp = self.send_request("misc/set_active", "POST", {"state": state_str})
        if resp is False:
            return False
        if isinstance(resp, dict):
            for k in ("ok", "success", "status", "active"):
                if k in resp:
                    return self._to_bool(resp[k])
        return True

    def deactivate_system(self) -> bool:
        success = False
        if self.get_system_active():
            success = self.set_active(False)
        else:
            print("System already deactivated")
        return success

    def get_time_allocation(self) -> int:
        # /api/misc/get_allocated_time [GET] -> string/int
        resp = self.send_request("misc/get_allocated_time", "GET")
        if isinstance(resp, dict):
            for k in ("allocated_time", "allocated", "value"):
                if k in resp:
                    return self._to_int(resp[k])
        return self._to_int(resp)

    def split_allocated_evenly(self):
        users = self.list_user_times()
        if users is False or users is None:
            return False
        num_users = len(users) if isinstance(users, (list, dict)) else 0
        if num_users == 0:
            print("No users to allocate to.")
            return False
        total_time = self.get_time_allocation()
        offset = int(total_time // num_users)
        print("Applying offset to each user:", offset)
        # Category id 0 = "all users"
        return self.apply_offset_cat(0, offset)

    def show_web_admin(self):
        # DO NOT IMPLEMENT YET, preserve this function
        return 0

