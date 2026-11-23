import urllib.request
import re
import json

urls = [
    "https://www.instagram.com/reel/DdyoLjxMGIn/",
    "https://www.instagram.com/reel/DfsRJzVRjle/",
    "https://www.instagram.com/p/DfwUQnQFFFv/",
    "https://www.instagram.com/reel/DfSKm_ktq3X/",
    "https://www.instagram.com/reel/Dfsqd9xKtVX/",
    "https://www.instagram.com/reel/DegUTTrNRXy/",
    "https://www.instagram.com/reel/Dd_KoU5sCVF/",
    "https://www.instagram.com/reel/DeryzA1tQSY/",
    "https://www.instagram.com/reel/Dfk8A68qKU5/",
    "https://www.instagram.com/reel/DecK6XlMvKG/",
    "https://www.instagram.com/reel/DecEVfkTuMO/",
    "https://www.instagram.com/reel/DfiXDOzKVwL/",
    "https://www.instagram.com/reel/DfE2ZyJtcgf/",
    "https://www.instagram.com/reel/Dde3CuJyB_7/",
    "https://www.instagram.com/reel/DfqIhihR2Cz/",
    "https://www.instagram.com/reel/Df1wavxqsyg/",
    "https://www.instagram.com/reel/Dej2oUmBEpv/",
    "https://www.instagram.com/reel/DewJpH1qzrt/",
    "https://www.instagram.com/reel/DemyiDgi5mQ/",
    "https://www.instagram.com/reel/DfCRnEuNxXb/",
    "https://www.instagram.com/reel/DejijtPzhYJ/",
    "https://www.instagram.com/reel/DfnZrrIqxZW/",
    "https://www.instagram.com/reel/DdevHBXtAE7/",
    "https://www.instagram.com/reel/Dfx75yrI5Up/",
    "https://www.instagram.com/reel/De68rXXSnbv/",
    "https://www.instagram.com/reel/DfW5PhoKHoo/",
    "https://www.instagram.com/reel/DfngeKFq2s0/",
    "https://www.instagram.com/reel/DfP5tH_s9E4/",
    "https://www.instagram.com/reel/DbImmjJjMaa/",
    "https://www.instagram.com/reel/DfsU5Q2OrYb/",
    "https://www.instagram.com/reel/De0EzqPMLG6/",
    "https://www.instagram.com/reel/DdjFYnCNaoN/",
    "https://www.instagram.com/reel/DbsRWl1tDbg/",
    "https://www.instagram.com/reel/De8b2CcJatN/"
]

headers = {
    'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36',
    'Accept-Language': 'en-US,en;q=0.9',
}

results = []
for u in urls:
    code = u.split('/')[-2]
    req = urllib.request.Request(u, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=6) as resp:
            html = resp.read().decode('utf-8', errors='ignore')
            title = re.search(r'<title>(.*?)</title>', html)
            desc = re.search(r'<meta property="og:description" content="(.*?)"', html) or re.search(r'<meta name="description" content="(.*?)"', html)
            og_title = re.search(r'<meta property="og:title" content="(.*?)"', html)
            t_str = title.group(1) if title else ""
            d_str = desc.group(1) if desc else ""
            ot_str = og_title.group(1) if og_title else ""
            results.append({
                "code": code,
                "url": u,
                "title": t_str,
                "og_title": ot_str,
                "desc": d_str[:200]
            })
            print(f"[{code}] {t_str} | {d_str[:120]}")
    except Exception as e:
        results.append({"code": code, "url": u, "error": str(e)})
        print(f"[{code}] Error: {e}")

with open("Scripts/reels_meta.json", "w", encoding="utf-8") as f:
    json.dump(results, f, ensure_ascii=False, indent=2)
