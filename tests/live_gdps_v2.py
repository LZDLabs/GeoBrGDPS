#!/usr/bin/env python3
"""Live checks of the deployed GDPS v2 that leave no test data behind.

Read-only requests plus the owner account (login, me, admin stats, mod access,
chest/quest state). Usage: python3 tests/live_gdps_v2.py <owner name> <owner password>
"""
import base64, hashlib, json, sys, urllib.error, urllib.parse, urllib.request

import os
FN = os.environ.get("GDPS_URL", "https://nolfgjsnnleqpmzvmebo.supabase.co/functions/v1/gdps")
checks = 0


def ok(cond, msg):
    global checks
    if not cond:
        raise SystemExit(f"FAILED: {msg}")
    checks += 1


def post(url, data, ctype):
    req = urllib.request.Request(url, data=data, headers={"Content-Type": ctype, "User-Agent": ""})
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            return r.read().decode()
    except urllib.error.HTTPError as e:
        return e.read().decode()


def gd(endpoint, params):
    return post(f"{FN}/database/{endpoint}.php", urllib.parse.urlencode(params).encode(), "application/x-www-form-urlencoded")


def api(path, body):
    return json.loads(post(f"{FN}/geobr/{path}", json.dumps(body).encode(), "application/json"))


def xor(text, key):
    return "".join(chr(ord(c) ^ ord(key[i % len(key)])) for i, c in enumerate(text))


name, password = sys.argv[1], sys.argv[2]
gjp2 = hashlib.sha1((password + "mI29fmAnxgTs").encode()).hexdigest()

info = api("info", {})
ok(info["ok"] and info["version"] == "1.11.0", f"server v2 ({info.get('version')})")
login = gd("accounts/loginGJAccount", {"userName": name, "gjp2": gjp2, "udid": "live-v2", "secret": "Wmfv3899gc9"})
aid = int(login.split(",")[0])
ok(aid >= 1, f"owner login -> {login}")
auth = {"accountID": aid, "gjp2": gjp2, "secret": "Wmfd2893gb7"}
me = api("me", {"accountID": aid, "gjp2": gjp2})
ok(me["ok"] and me["owner"] and "packs.manage" in me["permissions"], "owner with every permission")
ok(gd("requestUserAccess", auth) == "2", "GD 'Req' gives Elder Moderator (2)")
profile = gd("getGJUserInfo20", {"targetAccountID": aid})
kv = dict(zip(profile.split(":")[0::2], profile.split(":")[1::2]))
ok(kv.get("1") == name and kv.get("49") == "2", "profile shows the Elder Moderator badge")
stats = api("admin", {"accountID": aid, "gjp2": gjp2, "action": "stats"})
ok(stats["ok"], "admin panel stats")
packs = gd("getGJMapPacks21", {"page": 0, "secret": "Wmfd2893gb7"})
ok("#" in packs and packs.split("#")[1].endswith(":0:10"), f"map packs answer ({packs[:40]})")
lists = gd("getGJLevelLists", {"type": 4, "page": 0})
ok(lists == "-1" or "#" in lists, "level lists answer")
chk = "abcde" + base64.b64encode(xor("4242", "59182").encode()).decode()
chest = gd("getGJRewards", {**auth, "udid": "live-v2", "chk": chk, "rewardType": 0})
enc, h = chest[5:].split("|")
ok(h == hashlib.sha1((enc + "pC26fpYaQCtg").encode()).hexdigest(), "chest hash")
plain = xor(base64.urlsafe_b64decode(enc + "=" * (-len(enc) % 4)).decode("latin1"), "59182").split(":")
ok(plain[2] == "4242" and plain[4] == str(aid), "chest answer decoded")
qchk = "fghij" + base64.b64encode(xor("99", "19847").encode()).decode()
quests = gd("getGJChallenges", {**auth, "udid": "live-v2", "chk": qchk})
qenc, qh = quests[5:].split("|")
ok(qh == hashlib.sha1((qenc + "oC36fpYaPtdg").encode()).hexdigest(), "quest hash")
qplain = xor(base64.b64decode(qenc).decode("latin1"), "19847").split(":")
ok(qplain[2] == "99" and len(qplain) == 9, "3 daily quests")
ok(gd("getGJSongInfo", {"songID": 467339}) == "-1", "Newgrounds IDs are not answered by GeoBR")
ok(api("demonlist", {})["ok"], "Demon List")
print(f"PASS live_gdps_v2: {checks} checks (no test data written)")
