import subprocess
import json
import os

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

out_dir = "Scripts/reels_data"
os.makedirs(out_dir, exist_ok=True)

all_info = []

for idx, u in enumerate(urls):
    code = u.strip("/").split("/")[-1].split("?")[0]
    print(f"[{idx+1}/{len(urls)}] Processing {code}...")
    cmd = [
        "yt-dlp",
        "--skip-download",
        "--dump-json",
        u
    ]
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
        if res.returncode == 0 and res.stdout.strip():
            data = json.loads(res.stdout)
            item = {
                "id": data.get("id"),
                "uploader": data.get("uploader"),
                "title": data.get("title"),
                "description": data.get("description"),
                "categories": data.get("categories"),
                "tags": data.get("tags"),
                "duration": data.get("duration"),
                "width": data.get("width"),
                "height": data.get("height"),
                "thumbnail": data.get("thumbnail"),
                "url": u
            }
            all_info.append(item)
            print(f"  OK: {data.get('uploader')} - {data.get('title')[:60] if data.get('title') else 'No title'}")
        else:
            print(f"  Failed: {res.stderr[:100]}")
            all_info.append({"url": u, "code": code, "error": res.stderr[:100]})
    except Exception as e:
        print(f"  Ex: {e}")
        all_info.append({"url": u, "code": code, "error": str(e)})

with open("Scripts/reels_summary.json", "w", encoding="utf-8") as f:
    json.dump(all_info, f, ensure_ascii=False, indent=2)

print(f"Finished extracting {len(all_info)} reels.")
