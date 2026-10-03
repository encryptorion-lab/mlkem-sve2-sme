"""Validate that the requested results directory is writable."""

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
    except PermissionError as exc:
        raise SystemExit(
            f"results directory is not writable: {path}. "
            "Choose a writable location with --output; no root privileges "
            "are required."
        ) from exc
