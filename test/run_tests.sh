#!/usr/bin/env bash
# Trình chạy test chuyên sâu phía host cho firmware Smart Farm QE002.
#
# Biên dịch firmware THẬT (SmartFarm.ino + src/*.cpp) dựa trên các
# mock phần cứng có thể điều khiển trong test/mocks/ và chạy bộ test.
#
# Cách dùng:
#   ./test/run_tests.sh --quick      một bản build (mặc định core 3.x, cấu hình kit mặc định)
#   ./test/run_tests.sh --full       bốn bản build: core3, core2+inverts,
#                                    buzzer thụ động + relay kích mức thấp, ASan+UBSan
#   ./test/run_tests.sh --coverage   build với gcov và in độ phủ theo dòng
#
# Yêu cầu: g++ với C++17, bash. Không cần toolchain ESP32.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_DIR="$ROOT/test"
MOCKS="$TEST_DIR/mocks"
BUILD="$TEST_DIR/build"
mkdir -p "$BUILD"

SRC=(
  "$ROOT/src/actuators.cpp"
  "$ROOT/src/config.cpp"
  "$ROOT/src/control.cpp"
  "$ROOT/src/demo.cpp"
  "$ROOT/src/globals.cpp"
  "$ROOT/src/logger.cpp"
  "$ROOT/src/oled.cpp"
  "$ROOT/src/sensors.cpp"
  "$ROOT/src/settings.cpp"
  "$ROOT/src/webserver.cpp"
  "$ROOT/src/wifi.cpp"
)
FLAGS=(-std=gnu++17 -Wall -Wextra -Wshadow -Wundef -I"$MOCKS" -I"$ROOT/src")
FILES=("$TEST_DIR/test_main.cpp" "$MOCKS/mock.cpp" "${SRC[@]}" -x c++ "$ROOT/SmartFarm.ino")

MODE="${1:---full}"
COVERAGE_MIN="${COVERAGE_MIN:-80}"

build_and_run() {
  local name="$1"; shift
  local bin="$BUILD/$name.bin"
  echo "=== Build: $name ==="
  g++ "${FLAGS[@]}" "$@" -o "$bin" "${FILES[@]}"
  "$bin"
  echo
}

run_default()  { build_and_run core3 -DESP_ARDUINO_VERSION_MAJOR=3; }
run_core2()    { build_and_run core2-invert -DESP_ARDUINO_VERSION_MAJOR=2 \
                   -DINVERT_LIGHT=1 -DINVERT_SOIL=1 -DINVERT_WATER=1; }
run_buzzer()   { build_and_run passive-lowrelay -DESP_ARDUINO_VERSION_MAJOR=3 \
                   -DBUZZER_PASSIVE=1 -DRELAY_ACTIVE_LOW=1; }
run_asan() {
  local bin="$BUILD/asan.bin"
  echo "=== Build: asan+ubsan ==="
  g++ "${FLAGS[@]}" -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DESP_ARDUINO_VERSION_MAJOR=3 -o "$bin" "${FILES[@]}"
  # Một số phiên desktop đặt biến LD_PRELOAD (ví dụ libgtk-nocsd); runtime
  # sanitizer phải là thư viện được nạp đầu tiên, nên bỏ LD_PRELOAD cho lần chạy này.
  env -u LD_PRELOAD "$bin"
  echo
}

run_coverage() {
  local cov="$BUILD/cov"
  rm -rf "$cov"
  mkdir -p "$cov"
  echo "=== Build: coverage (gcov) ==="
  local objs=()
  local f o
  # gcov tạo file ghi chú bằng cách thay phần mở rộng của file object bằng
  # .gcno, nên đặt tên object như "control.o" (không phải "control.cpp.o").
  for f in "$TEST_DIR/test_main.cpp" "$MOCKS/mock.cpp" "${SRC[@]}"; do
    o="$cov/$(basename "${f%.cpp}").o"
    g++ "${FLAGS[@]}" --coverage -c "$f" -o "$o"
    objs+=("$o")
  done
  g++ "${FLAGS[@]}" --coverage -c -x c++ "$ROOT/SmartFarm.ino" -o "$cov/SmartFarm.o"
  objs+=("$cov/SmartFarm.o")
  g++ --coverage -o "$cov/test.bin" "${objs[@]}"
  "$cov/test.bin" | tail -n 1
  echo

  echo "=== Coverage (production code: src/*.cpp + SmartFarm.ino) ==="
  ( cd "$cov"
    for f in "$ROOT/src/"*.cpp "$ROOT/SmartFarm.ino"; do
      gcov -o "$cov" "$f" > /dev/null 2>&1 || true
    done )

  local total=0 covered=0
  printf "%-30s %8s %8s %8s\n" "File" "Lines" "Hit" "Cover"
  local gc base stats hit tot pct
  for gc in "$cov"/*.gcov; do
    [ -e "$gc" ] || continue
    base="$(basename "$gc" .gcov)"
    case "$base" in
      actuators.cpp|config.cpp|control.cpp|demo.cpp|globals.cpp|logger.cpp|\
      oled.cpp|sensors.cpp|settings.cpp|webserver.cpp|wifi.cpp|SmartFarm.ino) ;;
      *) continue ;;
    esac
    stats="$(awk -F: '
      $2 ~ /^[ ]*[0-9]+$/ {
        line=$2 + 0;
        if (line == 0) next;
        c=$1; gsub(/ /,"",c);
        if (c == "-") next;              # không thực thi (chú thích, dòng trống, })
        total++;
        if (c != "#####" && c != "=====" && (c+0) > 0) covered++;
      }
      END { printf "%d %d", covered, total }
    ' "$gc")"
    hit="${stats%% *}"
    tot="${stats##* }"
    total=$((total + tot))
    covered=$((covered + hit))
    if [ "$tot" -gt 0 ]; then
      pct="$(awk -v c="$hit" -v t="$tot" 'BEGIN { printf "%.1f", 100.0*c/t }')"
    else
      pct="n/a"
    fi
    printf "%-30s %8d %8d %7s%%\n" "$base" "$tot" "$hit" "$pct"
  done

  local overall
  if [ "$total" -gt 0 ]; then
    overall="$(awk -v c="$covered" -v t="$total" 'BEGIN { printf "%.1f", 100.0*c/t }')"
  else
    overall="0.0"
  fi
  echo "--------------------------------------------------------"
  printf "%-30s %8d %8d %7s%%\n" "TOTAL" "$total" "$covered" "$overall"
  awk -v p="$overall" -v m="$COVERAGE_MIN" 'BEGIN { exit (p+0 >= m+0) ? 0 : 1 }' || {
    echo "ERROR: coverage ${overall}% is below the ${COVERAGE_MIN}% threshold (set COVERAGE_MIN to override)"
    exit 1
  }
}

case "$MODE" in
  --quick)
    run_default
    echo "QUICK: PASSED"
    ;;
  --full)
    run_default
    run_core2
    run_buzzer
    run_asan
    echo "FULL: ALL 4 BUILDS PASSED"
    ;;
  --coverage)
    run_coverage
    ;;
  *)
    echo "Usage: $0 [--quick|--full|--coverage]" >&2
    exit 2
    ;;
esac
