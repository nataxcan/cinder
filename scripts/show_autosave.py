from pathlib import Path
for rel in [
    "bukkit.yml",
    "spigot.yml",
    "config/paper-global.yml",
    "config/paper-world-defaults.yml",
]:
    p = Path("/home/nataxcan/dev/folia-run") / rel
    print("=" * 20, rel)
    text = p.read_text()
    for i, line in enumerate(text.splitlines(), 1):
        if any(k in line.lower() for k in ("save", "auto", "worker", "chunk")):
            print(f"{i}:{line}")
