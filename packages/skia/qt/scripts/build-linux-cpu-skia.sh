#!/usr/bin/env bash
# Build locked Linux x86_64 CPU-raster Skia, then run Qt CMake/CTest smoke.
set -euo pipefail

readonly DEFAULT_IMAGE="react-native-qt@sha256:88a75994099e6e91034fd29def5a2cf719aa5d5d8254e7d1af9b14c8f4dee8a6"
readonly GN_SHA256="8fa12442a4d0fc4d754e89ec732aa015cdaf8d4bcbbedf68468c652531ea482c"

fail() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

inside_container() {
  local work stage
  work="$(mktemp -d /tmp/rnskia-qt-skia.XXXXXX)"
  stage="$(mktemp -d "${RNSK_QT_ARTIFACT_DIR}.tmp.XXXXXX")"
  trap "rm -rf '$work' '$stage'" EXIT

  cp -a /source/externals/skia/. "$work/skia"
  cp /source/packages/skia/qt/scripts/skia-linux-cpu.gn "$work/args.gn"

  cd "$work/skia"
  # Skottie/SVG/paragraph module builds need these two locked DEPS checkouts.
  # Fetch only enabled CPU-module dependencies; sync-deps fetches every Skia
  # platform dependency and needlessly reaches GPU/tool repositories.
  sync_dep() {
    local url="$1" revision="$2" directory="$3"
    git init -q "$directory"
    git -C "$directory" remote add origin "$url"
    git -C "$directory" fetch -q --depth 1 origin "$revision"
    git -C "$directory" checkout -q FETCH_HEAD
  }
  sync_dep https://chromium.googlesource.com/external/github.com/libexpat/libexpat.git \
    6154446fccefbf3ca644894f598969113b0c7bcd third_party/externals/expat
  sync_dep https://chromium.googlesource.com/external/github.com/harfbuzz/harfbuzz.git \
    9cb1fee51069b206effb4736e443b038d230789d third_party/externals/harfbuzz
  sync_dep https://chromium.googlesource.com/chromium/deps/libjpeg_turbo.git \
    e14cbfaa85529d47f9f55b0f104a579c1061f9ad third_party/externals/libjpeg-turbo
  sync_dep https://skia.googlesource.com/third_party/libpng.git \
    d5515b5b8be3901aac04e5bd8bd5c89f287bcd33 third_party/externals/libpng
  sync_dep https://chromium.googlesource.com/webm/libwebp.git \
    845d5476a866141ba35ac133f856fa62f0b7445f third_party/externals/libwebp
  sync_dep https://chromium.googlesource.com/chromium/src/third_party/zlib \
    646b7f569718921d7d4b5b8e22572ff6c76f2596 third_party/externals/zlib
  python3 bin/fetch-gn
  printf '%s  %s\n' "$GN_SHA256" bin/gn | sha256sum --check --status ||
    fail "Skia GN checksum mismatch"
  rm -rf out
  mkdir out
  cp "$work/args.gn" out/args.gn
  bin/gn gen out
  # Common RNSkManager binds Skottie, SVG, and paragraph APIs. Build their
  # CPU-only Skia modules, then merge archive members into artifact's single
  # libskia.a contract so normal CMake consumers do not manage GN internals.
  ninja -C out -j"${RNSK_QT_SKIA_JOBS:-$(nproc)}" \
    skia skottie svg skparagraph
  {
    printf 'CREATE %s\n' "$work/libskia-rn.a"
    find out -type f -name '*.a' -print | while IFS= read -r library; do
      printf 'ADDLIB %s\n' "$library"
    done
    printf 'SAVE\nEND\n'
  } | ar -M

  install -d "$stage/lib"
  # Common React Native Skia JSI headers include Skia module and private
  # headers. Preserve them from same locked revision as libskia.a.
  cp -a include "$stage/include"
  cp -a modules "$stage/modules"
  cp -a src "$stage/src"
  install -m 0644 "$work/libskia-rn.a" "$stage/lib/libskia.a"
  install -m 0644 "$work/args.gn" "$stage/skia-args.gn"

  cmake -S /source/packages/skia/qt -B "$work/qt-build" -G Ninja \
    -DRNSK_QT_SKIA_ROOT="$stage" \
    -DRNSK_QT_SKIA_SYSTEM_LIBRARIES='icuuc;icui18n;icudata'
  cmake --build "$work/qt-build" -j"${RNSK_QT_SKIA_JOBS:-$(nproc)}"
  ctest --test-dir "$work/qt-build" --output-on-failure

  RNSK_QT_SKIA_STAGE="$stage" \
  RNSK_QT_SKIA_REVISION="$RNSK_QT_SKIA_REVISION" \
  RNSK_QT_DOCKER_IMAGE="$RNSK_QT_DOCKER_IMAGE" \
  RNSK_QT_GN_SHA256="$GN_SHA256" \
  python3 - <<'PY'
import hashlib
import json
import os
from pathlib import Path

root = Path(os.environ["RNSK_QT_SKIA_STAGE"])


def tree_sha256(directory: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(path for path in directory.rglob("*") if path.is_file()):
        digest.update(str(path.relative_to(directory)).encode() + b"\0")
        digest.update(hashlib.sha256(path.read_bytes()).digest())
    return digest.hexdigest()


manifest = {
    "dockerImage": os.environ["RNSK_QT_DOCKER_IMAGE"],
    "gnSha256": os.environ["RNSK_QT_GN_SHA256"],
    "skiaRevision": os.environ["RNSK_QT_SKIA_REVISION"],
    "target": "linux-x86_64-cpu-raster",
    "files": {
        "include": tree_sha256(root / "include"),
        "modules": tree_sha256(root / "modules"),
        "src": tree_sha256(root / "src"),
        "lib/libskia.a": hashlib.sha256((root / "lib/libskia.a").read_bytes()).hexdigest(),
        "skia-args.gn": hashlib.sha256((root / "skia-args.gn").read_bytes()).hexdigest(),
    },
}
(root / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
PY

  mv "$stage" "$RNSK_QT_ARTIFACT_DIR"
  trap - EXIT
  rm -rf "$work"
  printf 'Artifact: %s\n' "$RNSK_QT_ARTIFACT_DIR"
}

if [[ "${1:-}" == "--in-container" ]]; then
  [[ $# == 1 ]] || fail "--in-container takes no extra arguments"
  [[ -n "${RNSK_QT_ARTIFACT_DIR:-}" ]] || fail "missing artifact directory"
  [[ -n "${RNSK_QT_SKIA_REVISION:-}" ]] || fail "missing Skia revision"
  [[ -n "${RNSK_QT_DOCKER_IMAGE:-}" ]] || fail "missing Docker image"
  inside_container
  exit 0
fi

[[ $# == 1 ]] || fail "usage: $0 /absolute/path/to/artifact"
artifact_dir="$1"
[[ "$artifact_dir" = /* ]] || fail "artifact directory must be absolute"
[[ ! -e "$artifact_dir" ]] || fail "artifact directory already exists: $artifact_dir"
artifact_parent="$(dirname "$artifact_dir")"
artifact_name="$(basename "$artifact_dir")"
[[ "$artifact_name" != "." && "$artifact_name" != ".." ]] || fail "invalid artifact directory"
mkdir -p "$artifact_parent"

repo_root="$(git rev-parse --show-toplevel)"
expected_skia_revision="$(git -C "$repo_root" ls-tree HEAD -- externals/skia | awk '{print $3}')"
[[ "$expected_skia_revision" =~ ^[0-9a-f]{40}$ ]] || fail "externals/skia is not a locked gitlink"
[[ -d "$repo_root/externals/skia/.git" || -f "$repo_root/externals/skia/.git" ]] ||
  fail "initialize externals/skia: git submodule update --init externals/skia"
actual_skia_revision="$(git -C "$repo_root/externals/skia" rev-parse HEAD)"
[[ "$actual_skia_revision" == "$expected_skia_revision" ]] ||
  fail "externals/skia revision differs from superproject lock"
git -C "$repo_root/externals/skia" diff --quiet || fail "externals/skia has tracked changes"
git -C "$repo_root/externals/skia" diff --cached --quiet || fail "externals/skia has staged changes"

image="${RNSK_QT_DOCKER_IMAGE:-$DEFAULT_IMAGE}"
[[ "$image" == *@sha256:* ]] || fail "RNSK_QT_DOCKER_IMAGE must use immutable @sha256 digest"
docker image inspect "$image" >/dev/null || fail "Qt toolchain image unavailable: $image"

docker run --rm --platform linux/amd64 \
  --mount "type=bind,src=$repo_root,dst=/source,readonly" \
  --mount "type=bind,src=$artifact_parent,dst=/artifacts" \
  -e "RNSK_QT_ARTIFACT_DIR=/artifacts/$artifact_name" \
  -e "RNSK_QT_SKIA_REVISION=$actual_skia_revision" \
  -e "RNSK_QT_DOCKER_IMAGE=$image" \
  -e "RNSK_QT_SKIA_JOBS=${RNSK_QT_SKIA_JOBS:-}" \
  "$image" \
  bash /source/packages/skia/qt/scripts/build-linux-cpu-skia.sh --in-container
