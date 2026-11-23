#!/usr/bin/env python3
"""Tiny OpenAI-compatible client for the smartapi provider.
Usage:
  python ask_smartapi.py --model claude-fable-5-2 --prompt prompt.txt --out answer.md
Key is read from Scripts/_secrets/smartapi.key (gitignored).
"""
import argparse, json, sys, time, urllib.request, pathlib

ENDPOINT = "https://api.smartapi.pro/v1/chat/completions"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="claude-fable-5-2")
    ap.add_argument("--prompt", required=True, help="prompt file, or - for stdin")
    ap.add_argument("--out", default="-")
    ap.add_argument("--max-tokens", type=int, default=8000)
    ap.add_argument("--temperature", type=float, default=0.3)
    ap.add_argument("--timeout", type=int, default=900)
    a = ap.parse_args()

    key = pathlib.Path(__file__).parent.joinpath("_secrets", "smartapi.key").read_text().strip()
    prompt = sys.stdin.read() if a.prompt == "-" else pathlib.Path(a.prompt).read_text(encoding="utf-8")

    body = json.dumps({
        "model": a.model,
        "messages": [{"role": "user", "content": prompt}],
        "max_tokens": a.max_tokens,
        "temperature": a.temperature,
    }).encode("utf-8")
    req = urllib.request.Request(ENDPOINT, data=body, headers={
        "Authorization": "Bearer " + key,
        "Content-Type": "application/json",
    })
    t0 = time.time()
    for attempt in range(3):
        with urllib.request.urlopen(req, timeout=a.timeout) as r:
            resp = json.load(r)
        ch = resp["choices"][0]
        if (ch["message"].get("content") or "").strip():
            break
        print(f"[ask_smartapi] attempt {attempt+1}: empty content (finish={ch.get('finish_reason')}), retrying...", flush=True)
        time.sleep(5)
    ch = resp["choices"][0]
    text = ch["message"].get("content") or ch["message"].get("reasoning_content") or ""
    finish = ch.get("finish_reason", "?")
    if not text.strip():
        text = f"(empty content, finish_reason={finish})"
    usage = resp.get("usage", {})
    header = f"<!-- model={a.model} in={usage.get('prompt_tokens')} out={usage.get('completion_tokens')} finish={finish} secs={time.time()-t0:.0f} -->\n\n"
    if a.out == "-":
        print(header + text)
    else:
        pathlib.Path(a.out).write_text(header + text, encoding="utf-8")
        print(f"[ask_smartapi] {a.out} <- {a.model} in={usage.get('prompt_tokens')} out={usage.get('completion_tokens')}")

if __name__ == "__main__":
    main()
