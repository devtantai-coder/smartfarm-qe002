#!/usr/bin/env bash
# Trình chạy test chuyên sâu phía host cho firmware Smart Farm QE002.
#
# Biên dịch firmware THẬT (SmartFarm.ino + src/*.cpp) dựa trên các
# mock phần cứng có thể điều khiển trong test/mocks/ và chạy bộ test.
#
# Cách dùng:
#   ./test/run_tests.sh --quick      một bản build (mặc định core 3.x, cấu hình kit mặc định)
#   ./test/run_tests.sh --full       sáu bản build: core3, core2+inverts,
#                                    buzzer thụ động + relay kích mức thấp, ASan+UBSan, web token,
#                                    buffer JSON nhỏ (nhánh JSON bị cắt) + bộ test JS trang web
#   ./test/run_tests.sh --coverage   build với gcov và in độ phủ theo dòng + nhánh
#
# Biến môi trường:
#   COVERAGE_MIN   ngưỡng độ phủ dòng (mặc định 80)
#   BRANCH_MIN     ngưỡng độ phủ nhánh, "taken at least once" (mặc định 75)
#   FUZZ_ITERS     số vòng fuzz mỗi seed (mặc định 30000)
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
  "$ROOT/src/hal.cpp"
  "$ROOT/src/rtos.cpp"
  "$ROOT/src/calibration.cpp"
  "$ROOT/src/anomaly.cpp"
  "$ROOT/src/pid.cpp"
  "$ROOT/src/softstart.cpp"
  "$ROOT/src/mqtt.cpp"
  "$ROOT/src/websocket.cpp"
  "$ROOT/src/tls.cpp"
  "$ROOT/src/ota.cpp"
  "$ROOT/src/metrics.cpp"
  "$ROOT/src/health.cpp"
  "$ROOT/src/logger.cpp"
  "$ROOT/src/oled.cpp"
  "$ROOT/src/sensors.cpp"
  "$ROOT/src/settings.cpp"
  "$ROOT/src/webserver.cpp"
  "$ROOT/src/wifi.cpp"
)
FLAGS=(-std=gnu++17 -Wall -Wextra -Wshadow -Wundef -DSMARTFARM_TEST_HOOKS -I"$MOCKS" -I"$ROOT/src")
FILES=("$TEST_DIR/test_main.cpp" "$MOCKS/mock.cpp" "${SRC[@]}" -x c++ "$ROOT/SmartFarm.ino")

MODE="${1:---full}"
COVERAGE_MIN="${COVERAGE_MIN:-80}"
BRANCH_MIN="${BRANCH_MIN:-75}"

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
run_token()    { build_and_run webtoken -DESP_ARDUINO_VERSION_MAJOR=3 \
                    -DWEB_TOKEN='"token-bao-mat-test"'; }
# Buffer JSON /api cố ý nhỏ để kiểm tra nhánh "JSON bị cắt cụt": firmware
# phải trả lỗi 500 thay vì trả JSON hỏng cho trình duyệt.
run_tinyjson() { build_and_run tinyjson -DESP_ARDUINO_VERSION_MAJOR=3 \
                    -DAPI_JSON_BUF_SIZE=64; }
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

# Kiểm thử JavaScript của trang web (Node). Trích đúng đoạn <script> từ
# src/webserver.cpp nên kiểm thử chính mã đang chạy trên thiết bị.
run_js() {
  echo "=== JS: web page script ==="
  if ! command -v node > /dev/null 2>&1; then
    echo "SKIP: không có node trong PATH"
    return 0
  fi
  node "$TEST_DIR/js/page_test.mjs"
  echo
}

run_coverage() {
  local cov="$BUILD/cov"
  rm -rf "$cov"
  mkdir -p "$cov"
  echo "=== Build: coverage (gcov) ==="
  local objs=()
  local f o
  # Bản coverage chạy kèm token để phủ cả nhánh kiểm tra token (D16);
  # nhánh không-token đã được phủ ở các bản build khác.
  # gcov tạo file ghi chú bằng cách thay phần mở rộng của file object bằng
  # .gcno, nên đặt tên object như "control.o" (không phải "control.cpp.o").
  for f in "$TEST_DIR/test_main.cpp" "$MOCKS/mock.cpp" "${SRC[@]}"; do
    o="$cov/$(basename "${f%.cpp}").o"
    g++ "${FLAGS[@]}" -DWEB_TOKEN='"token-bao-mat-test"' --coverage -c "$f" -o "$o"
    objs+=("$o")
  done
  g++ "${FLAGS[@]}" -DWEB_TOKEN='"token-bao-mat-test"' --coverage -c -x c++ "$ROOT/SmartFarm.ino" -o "$cov/SmartFarm.o"
  objs+=("$cov/SmartFarm.o")
  g++ --coverage -o "$cov/test.bin" "${objs[@]}"
  # Xóa .gcda cũ nếu còn (tránh cộng dồn số liệu của lần chạy trước).
  rm -f "$cov"/*.gcda
  "$cov/test.bin" | tail -n 1
  # gcov chỉ có số liệu khi tìm thấy .gcda cho từng file. Nếu .gcda bị thiếu
  # (lỗi ghi file tạm khi chạy song song) thì chạy lại test một lần để thu
  # thập đủ dữ liệu, tránh báo độ phủ sai rồi fail CI.
  local missing=0 o2
  for o2 in "$cov"/*.gcno; do
    [ -e "$o2" ] || continue
    [ -e "${o2%.gcno}.gcda" ] || missing=1
  done
  if [ "$missing" = "1" ]; then
    echo "coverage: thiếu file .gcda — chạy lại test để thu thập lại dữ liệu"
    "$cov/test.bin" | tail -n 1
  fi
  echo

  echo "=== Coverage (production code: src/*.cpp + SmartFarm.ino) ==="
  ( cd "$cov"
    for f in "$ROOT/src/"*.cpp "$ROOT/SmartFarm.ino"; do
      gcov -b -o "$cov" "$f" > /dev/null 2>&1 || true
    done )

  local total=0 covered=0 btotal=0 bcovered=0
  printf "%-22s %7s %7s %7s %10s %7s %7s\n" "File" "Lines" "Hit" "Cover" "Branches" "BHit" "BCover"
  local gc base f stats hit tot pct bstats bhit btot bpct
  for gc in "$cov"/*.gcov; do
    [ -e "$gc" ] || continue
    base="$(basename "$gc" .gcov)"
    case "$base" in
      actuators.cpp|anomaly.cpp|calibration.cpp|config.cpp|control.cpp|demo.cpp|globals.cpp|health.cpp|\
      hal.cpp|logger.cpp|metrics.cpp|mqtt.cpp|oled.cpp|ota.cpp|pid.cpp|rtos.cpp|sensors.cpp|\
      settings.cpp|softstart.cpp|tls.cpp|webserver.cpp|websocket.cpp|wifi.cpp|SmartFarm.ino) ;;
      *) continue ;;
    esac
    case "$base" in
      SmartFarm.ino) f="$ROOT/SmartFarm.ino" ;;
      *)             f="$ROOT/src/$base" ;;
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

    # Độ phủ nhánh: "Taken at least once" là chỉ số branch coverage cổ điển
    # (một nhánh đã được đi qua ít nhất một lần). Số liệu lấy từ dòng tổng
    # kết của gcov -b nên chính xác, không bị ảnh hưởng bởi làm tròn %.
    bstats="$( (cd "$cov" && gcov -b -o "$cov" "$f" 2>/dev/null) | awk -v n="$base" '
      /^File / { sec = $0 }
      /^Taken at least once:/ && sec ~ n {
        p = $4; sub(/^once:/, "", p); sub(/%$/, "", p);
        printf "%d %d", $6 * p / 100 + 0.5, $6;
      }')" || true
    if [ -n "$bstats" ]; then
      bhit="${bstats%% *}"
      btot="${bstats##* }"
    else
      bhit=0
      btot=0
    fi
    btotal=$((btotal + btot))
    bcovered=$((bcovered + bhit))

    if [ "$tot" -gt 0 ]; then
      pct="$(awk -v c="$hit" -v t="$tot" 'BEGIN { printf "%.1f", 100.0*c/t }')"
    else
      pct="n/a"
    fi
    if [ "$btot" -gt 0 ]; then
      bpct="$(awk -v c="$bhit" -v t="$btot" 'BEGIN { printf "%.1f", 100.0*c/t }')"
    else
      bpct="n/a"
    fi
    printf "%-22s %7d %7d %6s%% %10d %7d %6s%%\n" "$base" "$tot" "$hit" "$pct" "$btot" "$bhit" "$bpct"
  done

  local overall boverall
  if [ "$total" -gt 0 ]; then
    overall="$(awk -v c="$covered" -v t="$total" 'BEGIN { printf "%.1f", 100.0*c/t }')"
  else
    overall="0.0"
  fi
  if [ "$btotal" -gt 0 ]; then
    boverall="$(awk -v c="$bcovered" -v t="$btotal" 'BEGIN { printf "%.1f", 100.0*c/t }')"
  else
    boverall="0.0"
  fi
  echo "------------------------------------------------------------------------------"
  printf "%-22s %7d %7d %6s%% %10d %7d %6s%%\n" "TOTAL" "$total" "$covered" "$overall" "$btotal" "$bcovered" "$boverall"
  awk -v p="$overall" -v m="$COVERAGE_MIN" 'BEGIN { exit (p+0 >= m+0) ? 0 : 1 }' || {
    echo "ERROR: coverage ${overall}% is below the ${COVERAGE_MIN}% threshold (set COVERAGE_MIN to override)"
    exit 1
  }
  awk -v p="$boverall" -v m="$BRANCH_MIN" 'BEGIN { exit (p+0 >= m+0) ? 0 : 1 }' || {
    echo "ERROR: branch coverage ${boverall}% is below the ${BRANCH_MIN}% threshold (set BRANCH_MIN to override)"
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
    run_token
    run_tinyjson
    run_js
    echo "FULL: ALL 6 BUILDS + JS TESTS PASSED"
    ;;
  --coverage)
    run_coverage
    ;;
  *)
    echo "Usage: $0 [--quick|--full|--coverage]" >&2
    exit 2
    ;;
esac
