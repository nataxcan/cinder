#!/usr/bin/env python3
"""Report the published repo's top level and the token's scopes (never the token)."""
from __future__ import annotations

import json
import pathlib
import re
import urllib.request

HOSTS = pathlib.Path.home() / ".config" / "gh" / "hosts.yml"
TOKEN = re.search(r"oauth_token:\s*(\S+)", HOSTS.read_text()).group(1)


def api(url: str, method: str = "GET"):
    req = urllib.request.Request(url, method=method)
    req.add_header("Authorization", f"token {TOKEN}")
    req.add_header("Accept", "application/vnd.github+json")
    with urllib.request.urlopen(req, timeout=30) as fh:
        return fh, json.load(fh)


_, repo = api("https://api.github.com/repos/nataxcan/cinder")
print("repo:", repo["full_name"], "private:", repo["private"], "size:", repo["size"], "branch:", repo["default_branch"])

_, root = api("https://api.github.com/repos/nataxcan/cinder/contents/")
print("remote root:", sorted(x["name"] for x in root))

_, commit = api("https://api.github.com/repos/nataxcan/cinder/commits/main")
print("remote HEAD:", commit["sha"][:7], "-", commit["commit"]["message"].splitlines()[0])
print("files in remote tree:", len(commit["files"]) if "files" in commit else "(not expanded)")

fh, _ = api("https://api.github.com/user")
print("token scopes:", fh.headers.get("x-oauth-scopes"))
