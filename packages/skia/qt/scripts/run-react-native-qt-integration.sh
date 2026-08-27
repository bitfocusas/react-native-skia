#!/usr/bin/env bash
# Validate Qt RNSk bridge against installed React Native Qt support in its
# persistent amd64 toolchain container. Artifact stays external.
set -euo pipefail

fail() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

[[ $# == 1 ]] || fail "usage: $0 /absolute/path/to/linux-skia-artifact"
artifact_dir="$1"
[[ "$artifact_dir" = /* && -s "$artifact_dir/lib/libskia.a" ]] ||
  fail "artifact must contain lib/libskia.a"
[[ -d "$artifact_dir/modules" && -d "$artifact_dir/src" ]] ||
  fail "artifact must contain locked modules and src headers for RNSkManager"

repo_root="$(git rev-parse --show-toplevel)"
rn_worktree="${RNSK_QT_REACT_NATIVE_WORKTREE:-$repo_root/../expo-qt6-wayland-deb}"
container="${RNSK_QT_REACT_NATIVE_CONTAINER:-react-native-qt-qt6-amd64-worktree}"
support_dir="${RNSK_QT_REACT_NATIVE_SUPPORT_DIR:-/work/rn-support-install}"
stage="/work/rnskia-qt-react-native-integration"

[[ -d "$rn_worktree" ]] || fail "missing React Native Qt sibling worktree: $rn_worktree"
[[ "$(docker inspect --format '{{.State.Running}}' "$container" 2>/dev/null)" == true ]] ||
  fail "persistent React Native Qt container is not running: $container"
mounted_worktree="$(docker inspect --format '{{range .Mounts}}{{if eq .Destination "/src"}}{{.Source}}{{end}}{{end}}' "$container")"
[[ "$(cd "$mounted_worktree" && pwd -P)" == "$(cd "$rn_worktree" && pwd -P)" ]] ||
  fail "container /src mount does not match React Native Qt sibling worktree"
docker exec "$container" test -s "$support_dir/lib/libreactnativeqt_core.so"
docker exec "$container" test -s "$support_dir/lib/libhermesvm.so"

docker exec "$container" rm -rf "$stage"
docker exec "$container" mkdir -p "$stage/source/packages" "$stage/skia"
docker cp "$repo_root/packages/skia" "$container:$stage/source/packages/"
docker cp "$artifact_dir/." "$container:$stage/skia/"
docker exec "$container" bash -lc "
  set -euo pipefail
  cmake -S '$stage/source/packages/skia/qt' -B '$stage/build' -G Ninja \\
    -DCMAKE_BUILD_TYPE=Release \\
    -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 \\
    -DRNSK_QT_BUILD_SMOKE=OFF -DRNSK_QT_BUILD_REACT_NATIVE_SMOKE=ON \\
    -DRNSK_QT_SKIA_ROOT='$stage/skia' \\
    -DRNSK_QT_SKIA_SYSTEM_LIBRARIES='icuuc;icui18n;icudata' \\
    -DRNSK_QT_REACT_NATIVE_INCLUDE_DIRS='$support_dir/include;$support_dir/include/ReactCommon;$support_dir/include/ReactCommon/callinvoker;$support_dir/include/ReactCommon/jsi;$support_dir/include/ReactCommon/react/nativemodule/core;$support_dir/include/ReactCommon/runtimeexecutor' \\
    -DRNSK_QT_REACT_NATIVE_LIBRARIES='$support_dir/lib/libreactnativeqt_core.so;$support_dir/lib/libhermesvm.so'
  cmake --build '$stage/build' --target rnskia_qt_react_native_smoke --parallel '${RNSK_QT_SKIA_JOBS:-4}'
  ctest --test-dir '$stage/build' -R '^rnskia_qt_react_native_smoke$' --output-on-failure
"
