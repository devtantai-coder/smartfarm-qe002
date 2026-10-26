// Kiểm thử JavaScript của trang web (chạy bằng Node).
//
// Trang web nằm inline trong src/connectivity/webserver.cpp (ROOT_HTML) — file này TRÍCH
// đúng đoạn <script> đó ra để kiểm thử, không chép lại mã: một nguồn sự thật
// duy nhất, đúng như bộ test C++ với firmware.
//
// Chạy:  node test/js/page_test.mjs
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..', '..');

// ── Trích JavaScript từ trang web thật ──
const src = fs.readFileSync(path.join(root, 'src', 'connectivity', 'webserver.cpp'), 'utf8');
const htmlStart = src.indexOf('static const char ROOT_HTML[]');
if (htmlStart < 0) throw new Error('không tìm thấy ROOT_HTML trong src/connectivity/webserver.cpp');
const htmlEnd = src.indexOf(')rawliteral', htmlStart);
const html = src.slice(htmlStart, htmlEnd);
const s1 = html.indexOf('<script>');
const s2 = html.indexOf('</script>');
if (s1 < 0 || s2 < 0) throw new Error('không tìm thấy thẻ <script> trong ROOT_HTML');
const pageJs = html.slice(s1 + '<script>'.length, s2);

// ── Khung test tự viết (không dùng thư viện ngoài) ──
let gChecks = 0, gFails = 0;
function check(cond, msg) {
  gChecks++;
  if (!cond) { gFails++; console.log(`  FAIL: ${msg}`); }
}
function checkEq(a, b, msg) {
  check(a === b, `${msg} (got ${JSON.stringify(a)}, expected ${JSON.stringify(b)})`);
}

// Môi trường giả cho trình duyệt
function makeEnv(opts = {}) {
  const timers = [];
  const requests = [];
  const el = () => ({ textContent: '', className: '', style: {} });
  const env = {
    timers, requests,
    payload: opts.payload ?? { temp: 26.5, hum: 61, light: 800, soil: 650, water: 900, nonce: 1 },
    apiStatus: opts.apiStatus ?? 200,
    cmdStatus: opts.cmdStatus ?? 200,
    cmdStatusSeq: opts.cmdStatusSeq ?? null,   // danh sách mã trả lần lượt
    cmdCalls: 0,
  };
  env.location = { search: opts.search ?? '', pathname: '/' };
  env.sessionStorage = {
    store: {},
    setItem(k, v) { this.store[k] = v; },
    getItem(k) { return this.store[k] ?? null; },
  };
  env.history = { replaced: null, replaceState(_a, _b, p) { this.replaced = p; } };
  env.document = {
    hidden: false,
    getElementById: () => el(),
    addEventListener: () => {},
  };
  env.confirm = () => true;
  // setTimeout giả: ghi lại độ trễ, không tự chạy -> kiểm thử xác định (không phụ thuộc thời gian thực)
  env.setTimeout = (fn, ms) => { timers.push({ fn, ms }); return timers.length; };
  env.clearTimeout = () => {};
  env.fetch = async (u, init) => {
    requests.push({ url: u, headers: init?.headers ?? {}, signal: init?.signal });
    if (u.includes('/api')) {
      return { status: env.apiStatus, ok: env.apiStatus === 200, json: async () => ({ ...env.payload }) };
    }
    env.cmdCalls++;
    const st = env.cmdStatusSeq ? env.cmdStatusSeq[Math.min(env.cmdCalls - 1, env.cmdStatusSeq.length - 1)] : env.cmdStatus;
    return { status: st, ok: st === 200, json: async () => ({}) };
  };
  return env;
}

// Để các promise đang chờ hoàn tất (dùng setTimeout thật, không phải bản giả)
const realSetTimeout = globalThis.setTimeout;
const settle = async () => { for (let i = 0; i < 5; i++) await new Promise(r => realSetTimeout(r, 0)); };

// Nạp trang vào một môi trường giả, trả về { env, api }
function loadPage(opts = {}) {
  const env = makeEnv(opts);
  const saved = {};
  for (const k of ['window', 'document', 'location', 'sessionStorage', 'history', 'confirm',
                   'setTimeout', 'clearTimeout', 'fetch']) {
    saved[k] = globalThis[k];
  }
  globalThis.window = globalThis;
  Object.assign(globalThis, env);
  // eval GIÁN TIẾP (0,eval) để các khai báo function/let của trang rơi vào
  // phạm vi toàn cục -> bộ test gọi được chính hàm của trang web thật.
  (0, eval)(pageJs);
  const api = {
    env,
    settle,
    restore() { for (const k of Object.keys(saved)) globalThis[k] = saved[k]; },
    // chạy một lượt poll đã hẹn. Lọc theo đúng hàm pollTick — trang cũng dùng
    // setTimeout cho timer huỷ request (AbortController), không phải poll.
    async runPoll() {
      const i = env.timers.findIndex(t => t.fn === globalThis.pollTick);
      if (i < 0) return null;
      const t = env.timers.splice(i, 1)[0];
      await t.fn();
      return t.ms;
    },
    nextDelay() {
      const t = env.timers.filter(x => x.fn === globalThis.pollTick);
      return t.length ? t[t.length - 1].ms : null;
    },
  };
  return api;
}

// ============================================================
// 1. Poll thích ứng: nhanh khi dữ liệu đổi, giãm tần số khi đứng yên
// ============================================================
{
  const api = loadPage();
  const env = api.env;
  await api.settle();                       // chờ lần /api đầu tiên hoàn tất

  checkEq(env.requests.length, 1, 'trang gọi /api ngay khi mở');
  checkEq(api.nextDelay(), 2000, 'lịch poll đầu tiên là 2 giây');

  // 3 vòng không có thay đổi -> chuyển sang 5 giây
  await api.runPoll();                 // stable=1
  checkEq(api.nextDelay(), 2000, 'vòng 1 vẫn poll 2 giây');
  await api.runPoll();                 // stable=2
  checkEq(api.nextDelay(), 2000, 'vòng 2 vẫn poll 2 giây');
  await api.runPoll();                 // stable=3 -> giảm tần số
  checkEq(api.nextDelay(), 5000, 'vòng 3 chuyển sang poll 5 giây');

  await api.runPoll();                 // vẫn không đổi
  checkEq(api.nextDelay(), 5000, 'vòng 4 vẫn poll 5 giây');

  // Dữ liệu đổi -> quay lại 2 giây
  env.payload = { ...env.payload, temp: 30.0 };
  await api.runPoll();
  checkEq(api.nextDelay(), 2000, 'khi dữ liệu đổi thì poll nhanh lại 2 giây');

  api.restore();
}

// ============================================================
// 2. Token: đọc từ URL -> sessionStorage -> gửi qua header, và bỏ token khỏi URL
// ============================================================
{
  const api = loadPage({ search: '?token=mat-khau-abc' });
  const env = api.env;
  checkEq(env.sessionStorage.store.sfToken, 'mat-khau-abc', 'token lưu vào sessionStorage');
  checkEq(env.history.replaced, '/', 'token bị gỡ khỏi URL (history.replaceState)');
  checkEq(env.requests[0].headers['X-Auth-Token'], 'mat-khau-abc', 'gửi token qua header X-Auth-Token');
  api.restore();
}

// ============================================================
// 3. Nonce cũ (403) -> tự lấy nonce mới rồi thử lại, không kẹt 403
// ============================================================
{
  const api = loadPage({ cmdStatusSeq: [403, 200] });
  const env = api.env;
  await api.settle();                       // nonce đã lấy từ /api
  // Lần gọi lệnh đầu bị 403 (nonce cũ), client phải lấy nonce mới rồi thử lại
  const res = await globalThis.call('/toggle?id=pump');
  checkEq(res.status, 200, 'lệnh được thử lại sau khi gặp 403');
  checkEq(env.cmdCalls, 2, 'lệnh được gửi lại đúng một lần');
  api.restore();
}

// ============================================================
// 4. Mọi request đều có AbortSignal (timeout) để giao diện không treo
// ============================================================
{
  const api = loadPage();
  const env = api.env;
  await api.settle();
  check(env.requests[0].signal !== undefined, 'request /api có AbortSignal để huỷ khi treo');
  api.restore();
}

// ============================================================
// 5. Không gọi chồng request: poll khi đang có request thì bỏ qua
// ============================================================
{
  const api = loadPage();
  const env = api.env;
  await api.settle();
  const before = env.requests.length;
  // Gọi fetchData() lần nữa ngay khi một lần đang bay -> không được gọi thêm
  const p = globalThis.fetchData();       // chiếm cờ "polling"
  await globalThis.fetchData();
  checkEq(env.requests.length, before + 1, 'request thứ hai bị bỏ qua khi đang bay');
  await p;
  api.restore();
}

console.log(`=== ${gChecks} checks, ${gFails} failures ===`);
process.exit(gFails ? 1 : 0);
