#!/usr/bin/env python3
"""Live smoke test of the deployed `gdps` Edge Function (GeoBR GDPS).

Talks to the real server the way GD does (form POSTs, plain-text answers) and
the way the mod does (JSON). Creates accounts named GeoBRTeste<random>; the
cleanup SQL printed at the end removes them. The admin part needs the first
test account to hold the Admin role (granted by SQL between the two phases):

  python3 tests/live_gdps.py phase1          -> prints the account IDs
  (grant Admin to account A by SQL)
  python3 tests/live_gdps.py phase2 <state>  -> admin, Daily, songs moderation
"""
import base64, hashlib, json, os, random, string, sys, urllib.error, urllib.parse, urllib.request

BASE = "https://nolfgjsnnleqpmzvmebo.supabase.co"
FN = BASE + "/functions/v1/gdps"
PUBLISHABLE = "sb_publishable_iXNlORpQZQZqTyxqoMXuew_9TE0ZTT1"
SALT = "xI25fpAapCQg"
checks = 0


def ok(cond, msg):
    global checks
    if not cond:
        raise SystemExit(f"FAILED: {msg}")
    checks += 1


def http(url, data=None, headers=None, method=None):
    req = urllib.request.Request(url, data=data, headers=headers or {}, method=method)
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


def gd(endpoint, params, prefix="/database/"):
    body = urllib.parse.urlencode(params).encode()
    status, out = http(FN + prefix + endpoint + ".php", body, {"Content-Type": "application/x-www-form-urlencoded", "User-Agent": ""})
    return out.decode()


def api(path, body):
    status, out = http(FN + "/geobr/" + path, json.dumps(body).encode(), {"Content-Type": "application/json"})
    return json.loads(out)


def sha1(s):
    return hashlib.sha1(s.encode()).hexdigest()


def b64(s):
    return base64.urlsafe_b64encode(s.encode()).decode()


def gen_solo(level):
    chars = ["a"] * 40
    n = len(level)
    if n:
        step = n // 40
        if step == 0:
            chars = [level[0]] * 40
        else:
            p, k = 0, 0
            while p < 40 and k < n:
                chars[p] = level[k]; p += 1; k += step
    return sha1("".join(chars) + SALT)


def anon_session():
    status, out = http(BASE + "/auth/v1/signup", b"{}", {"apikey": PUBLISHABLE, "Content-Type": "application/json"})
    j = json.loads(out)
    ok(status == 200 and j.get("access_token"), f"anonymous GeoBR session ({status})")
    return j


def phase1():
    tag = "".join(random.choice(string.ascii_lowercase + string.digits) for _ in range(6))
    users = {}
    status, root = http(FN)
    ok(root.decode() == "GeoBR GDPS", "function answers")
    info = api("info", {})
    ok(info["ok"] and info["songs"] == [8000001, 9999999], "info")
    for k in ("A", "B"):
        name = f"GeoBRTeste{k}{tag}"[:20]
        pw = f"Teste-{tag}-{k}1"
        ok(gd("accounts/registerGJAccount", {"userName": name, "password": pw, "email": "sem-email@geobr.invalid", "secret": "Wmfv3899gc9"}) == "1", f"register {name}")
        g = sha1(pw + "mI29fmAnxgTs")
        r = gd("accounts/loginGJAccount", {"userName": name, "gjp2": g, "udid": "live", "secret": "Wmfv3899gc9"})
        aid = int(r.split(",")[0])
        ok(aid >= 900000001, f"login {name} -> {r}")
        users[k] = {"name": name, "id": aid, "gjp2": g}
    A, B = users["A"], users["B"]
    auth = lambda u: {"accountID": u["id"], "gjp2": u["gjp2"], "secret": "Wmfd2893gb7"}

    # Cloud save through GD's account URL (Storage, private bucket).
    url = gd("getAccountURL", {"accountID": A["id"], "type": 1, "secret": "Wmfd2893gb7"})
    ok(url == FN, f"account URL {url}")
    save = "H4sILIVEMANAGER;H4sILIVELEVELS"
    ok(gd("accounts/backupGJAccountNew", {**auth(A), "saveData": save, "gameVersion": 22, "binaryVersion": 45}) == "1", "backup to Storage")
    ok(gd("accounts/syncGJAccountNew", auth(A)) == f"{save};22;45;a;a", "sync from Storage")

    # Level upload, search, download with GD's hashes.
    level = "H4sIAAAAAAAACq2QwQ3DIAxFF7IlbGNIlFNmyAAMkBWyfCH+qdNLe+gBP/wfNjL7pmpQD2yyGqlojTu0Ccx/FlJgTQmA1yJtAbUAS7l1YmlTQTnaEtmxHE6QTlA/OIGvA30pPUJPVzodSyQOHS8RqSqiXh1xjjPeQz8eT1ZTCh0RXx2Y/dHOwPsBsF5k3psBAAA="
    lid = int(gd("uploadGJLevel21", {**auth(A), "userName": A["name"], "levelID": 0, "levelName": "Teste Ao Vivo", "levelDesc": b64("teste"), "levelVersion": 1, "levelLength": 1, "audioTrack": 0, "password": "0", "objects": 10, "levelString": level, "gameVersion": 22}))
    ok(lid >= 900000001, f"level uploaded {lid}")
    lst = gd("getGJLevels21", {"type": 0, "str": str(lid), "secret": "Wmfd2893gb7"}).split("#")
    ok(len(lst) == 5 and lst[0].startswith(f"1:{lid}:2:Teste Ao Vivo"), "search by ID")
    ok(lst[4] == sha1(str(lid)[0] + str(lid)[-1] + "0" + "0" + SALT), "list hash")
    dl = gd("downloadGJLevel22", {"levelID": lid, "secret": "Wmfd2893gb7"}).split("#")
    ok(dl[1] == gen_solo(level), "genSolo")
    ok(dl[2] == sha1(f"{A['id']},0,0,{lid},0,0,0,0" + SALT), "genSolo2")
    cid = gd("uploadGJComment21", {**auth(B), "comment": b64("ao vivo!"), "levelID": lid, "percent": 50})
    ok(cid.isdigit(), "comment")
    ok(gd("getGJComments21", {"levelID": lid, "page": 0}).endswith("#1:0:10"), "comment list")
    ok(gd("getGJSongInfo", {"songID": 467339}) == "-1", "Newgrounds IDs not answered by GeoBR")

    # Music Library upload through a signed Storage URL.
    up = api("songs/upload", {"accountID": B["id"], "gjp2": B["gjp2"], "name": "Teste Ao Vivo", "artist": "GeoBR"})
    ok(up["ok"] and 8000001 <= up["songID"] <= 9999999, f"song id {up}")
    mp3 = b"ID3\x03\x00\x00\x00\x00\x00\x00" + os.urandom(4086)
    status, _ = http(up["uploadUrl"], mp3, {"Content-Type": "audio/mpeg"}, "PUT")
    ok(status == 200, f"signed upload PUT ({status})")
    conf = api("songs/confirm", {"accountID": B["id"], "gjp2": B["gjp2"], "songID": up["songID"]})
    ok(conf.get("status") == "pending", f"song waits for moderation {conf}")
    ok(gd("getGJSongInfo", {"songID": up["songID"]}) == "-1", "pending song not served yet")

    # Linking the GDPS account to a GeoBR profile, then "another device".
    s1 = anon_session()
    r = api("session", {"accountID": A["id"], "gjp2": A["gjp2"], "accessToken": s1["access_token"]})
    ok(r.get("ok") and r.get("linked"), f"link to this device's profile {r}")
    s2 = anon_session()
    r = api("session", {"accountID": A["id"], "gjp2": A["gjp2"], "accessToken": s2["access_token"]})
    ok(r.get("ok") and r.get("user_id") == s1["user"]["id"] and r.get("refresh_token"), f"other device gets the linked profile {str(r)[:120]}")
    status, out = http(BASE + "/auth/v1/token?grant_type=refresh_token", json.dumps({"refresh_token": r["refresh_token"]}).encode(),
                       {"apikey": PUBLISHABLE, "Content-Type": "application/json"})
    ok(status == 200 and json.loads(out)["user"]["id"] == s1["user"]["id"], "issued session refreshes like the mod does")
    me = api("me", {"accountID": A["id"], "gjp2": A["gjp2"]})
    ok(me["ok"] and me["account"]["linked"] and me["permissions"] == [], "me: linked, no roles")
    ok(api("admin", {"accountID": A["id"], "gjp2": A["gjp2"], "action": "stats"})["ok"] is False, "no admin without a role")
    state = {"A": A, "B": B, "level": lid, "song": up["songID"], "users": [s1["user"]["id"], s2["user"]["id"]], "tag": tag}
    print(json.dumps(state))
    print(f"PASS live_gdps phase1: {checks} checks")


def phase2(state):
    A, B = state["A"], state["B"]
    adm = lambda action, **f: api("admin", {"accountID": A["id"], "gjp2": A["gjp2"], "action": action, **f})
    st = adm("stats")
    ok(st["ok"] and st["stats"]["accounts"] >= 2, f"admin stats {st}")
    ok(adm("levels.rate", levelID=state["level"], stars=4)["ok"], "rate")
    ok(adm("levels.feature", levelID=state["level"], epic=2)["ok"], "legendary")
    d = gd("downloadGJLevel22", {"levelID": state["level"]}).split("#")[0].split(":")
    kv = dict(zip(d[0::2], d[1::2]))
    ok(kv["18"] == "4" and kv["42"] == "2", "rating visible in GD")
    ev = adm("events.set", kind=0, levelID=state["level"])
    ok(ev["ok"], "daily set")
    daily = gd("getGJDailyLevel", {"type": 0}).split("|")
    ok(int(daily[0]) == ev["id"] and int(daily[1]) > 0, f"daily {daily}")
    ok(adm("events.remove", id=ev["id"])["ok"], "daily removed again")
    ok(gd("getGJDailyLevel", {"type": 0}) == "-1", "no daily after removal")
    if not state.get("song_done"):
        ok(adm("songs.review", songID=state["song"], status="approved")["ok"], "approve song")
        info = gd("getGJSongInfo", {"songID": state["song"]})
        url = urllib.parse.unquote(info.split("~|~10~|~")[1].split("~|~")[0])
        status, data = http(url)
        ok(status == 200 and data[:3] == b"ID3", "song served by the public bucket")
        ok(adm("songs.review", songID=state["song"], status="removed")["ok"], "remove song (file deleted from Storage)")
        # The public CDN may keep serving a cached copy for a while; GD no longer gets the URL.
    ok(gd("getGJSongInfo", {"songID": state["song"]}) == "-2", "removed song disabled")
    ok(adm("accounts.ban", target=B["id"], reason="teste")["ok"], "ban")
    ok(gd("uploadGJComment21", {"accountID": B["id"], "gjp2": B["gjp2"], "comment": b64("oi"), "levelID": state["level"]}) == "-1", "banned cannot post")
    log = adm("audit.list")
    acts = {e["action"] for e in log["entries"]}
    ok({"level.rate", "event.set", "song.approved", "song.removed", "account.ban"} <= acts, f"audit {acts}")
    print(f"PASS live_gdps phase2: {checks} checks")


if __name__ == "__main__":
    if sys.argv[1] == "phase1":
        phase1()
    else:
        phase2(json.loads(sys.argv[2]))
