"""Pick a writable local results directory."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def ensure_writable(path: Path) -> Path:
    path = path.resolve()
    try:
        path.mkdir(parents=True, exist_ok=True)
        probe = path / ".write_probe"
        probe.write_text("ok")
        probe.unlink()
        return path
    except PermissionError:
        pass

    try:
        relative = path.relative_to(ROOT / "results")
    except ValueError:
        relative = Path(path.name)
    fallback = (ROOT / "results-local" / relative).resolve()
    fallback.mkdir(parents=True, exist_ok=True)
    print(
        f"warning: {path} is not writable. "
        f"Writing to {fallback}. Both root result trees are local, ignored "
        f"outputs; no root privileges are required.",
        flush=True,
    )
    return fallback
