"""Fetch pinned source dependencies for the SCons build."""

import argparse
from pathlib import Path
import subprocess
import tempfile
import time
import uuid

# Tags keep team members on matching source releases instead of a moving branch.
DEPENDENCIES = {
    "glfw": ("https://github.com/glfw/glfw.git", "3.4"),
    "glm": ("https://github.com/g-truc/glm.git", "1.0.1"),
    "glad": ("https://github.com/Dav1dde/glad.git", "v2.0.8"),
    "imgui": ("https://github.com/ocornut/imgui.git", "v1.92.5"),
    "tinyobjloader": ("https://github.com/tinyobjloader/tinyobjloader.git", "v2.0.0rc13"),
    "eigen": ("https://gitlab.com/libeigen/eigen.git", "3.4.0"),
    "assimp": ("https://github.com/assimp/assimp.git", "v5.4.3"),
}


def clone_dependency(url, tag, destination):
    # Stage each attempt separately: interrupted downloads must not look like
    # installed dependencies on the next run. Never change global Git settings.
    for attempt in range(3):
        try:
            with tempfile.TemporaryDirectory(
                prefix=destination.name + "-download-", dir=destination.parent
            ) as staging:
                checkout = Path(staging) / "checkout"
                subprocess.run(
                    [
                        "git", "-c", "http.version=HTTP/1.1", "clone",
                        "--depth", "1", "--branch", tag, "--single-branch",
                        url, str(checkout),
                    ],
                    check=True,
                )
                checkout.rename(destination)
            return
        except subprocess.CalledProcessError:
            if attempt == 2:
                raise
            print(f"Download interrupted; retrying {destination.name} ({attempt + 2}/3)",
                  flush=True)
            time.sleep(2 ** attempt)


def setup(directory):
    directory.mkdir(parents=True, exist_ok=True)
    for name, (url, tag) in DEPENDENCIES.items():
        destination = directory / (name + "-src")
        if (destination / ".git").is_dir():
            head_check = subprocess.run(
                ["git", "-C", str(destination), "rev-parse", "--verify", "HEAD"],
                capture_output=True,
            )
            if head_check.returncode != 0:
                # Preserve remnants from an older failed clone instead of deleting
                # them, then allow a clean download into the expected location.
                backup = destination.with_name(destination.name + "-incomplete-" + uuid.uuid4().hex[:8])
                destination.rename(backup)
                print(f"Preserved incomplete checkout at {backup}", flush=True)
        if destination.exists():
            if not (destination / ".git").exists():
                raise RuntimeError(f"{destination} is not a Git checkout; use a different --directory")
            # Do not reset or overwrite an existing checkout, even on a version mismatch.
            head = subprocess.check_output(
                ["git", "-C", str(destination), "rev-parse", "HEAD"], text=True
            ).strip()
            # Dereference annotated tags so both values identify commits.
            pinned = subprocess.check_output(
                ["git", "-C", str(destination), "rev-parse", tag + "^{commit}"], text=True
            ).strip()
            if head != pinned:
                raise RuntimeError(f"{destination} is not at {tag}; use a different --directory")
            print(f"{name}: already at {tag}", flush=True)
        else:
            clone_dependency(url, tag, destination)
    print(f"Dependencies ready in {directory}. Run python -m SCons.", flush=True)


if __name__ == "__main__":
    # Default to the repository location, regardless of the caller's working directory.
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--directory", type=Path, default=Path(__file__).resolve().parents[1] / ".deps"
    )
    args = parser.parse_args()
    setup(args.directory.resolve())
