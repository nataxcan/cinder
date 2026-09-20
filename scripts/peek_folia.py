from pathlib import Path
root = Path("/home/nataxcan/dev/folia-run")
print("exists", root.exists())
for p in sorted(root.rglob("*")):
    if p.is_file() and p.suffix in {".yml", ".yaml", ".properties", ".toml"}:
        print("FILE", p.relative_to(root), p.stat().st_size)
