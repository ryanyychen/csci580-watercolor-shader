"""Fetch pinned source dependencies for the SCons build."""

import argparse
from pathlib import Path
import subprocess

# Tags keep team members on matching source releases instead of a moving branch.
DEPENDENCIES = {
    "glfw": ("https://github.com/glfw/glfw.git", "3.4"),
    "glm": ("https://github.com/g-truc/glm.git", "1.0.1"),
    "glad": ("https://github.com/Dav1dde/glad.git", "v2.0.8"),
    "tinyobjloader": ("https://github.com/tinyobjloader/tinyobjloader.git", "v2.0.0rc13"),
}


def setup(directory):
    directory.mkdir(parents=True, exist_ok=True)
    for name, (url, tag) in DEPENDENCIES.items():
        destination = directory / (name + "-src")
        if destination.exists():
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
            # Download only the requested release; compilation is handled by SCons.
            subprocess.run(
                [
                    "git",
                    "clone",
                    "--depth",
                    "1",
                    "--branch",
                    tag,
                    "--single-branch",
                    url,
                    str(destination),
                ],
                check=True,
            )
    print(f"Dependencies ready in {directory}. Run python -m SCons.", flush=True)


if __name__ == "__main__":
    # Default to the repository location, regardless of the caller's working directory.
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--directory", type=Path, default=Path(__file__).resolve().parents[1] / ".deps"
    )
    args = parser.parse_args()
    setup(args.directory.resolve())
