// Live screen-sharing check of the native engine against the LiveKit project in
// .env.local (LIVEKIT_URL, LIVEKIT_API_KEY, LIVEKIT_API_SECRET).
//
//   node native-desktop/tools/screen-live-check.mjs
//
// Mints two 3-minute tokens for a new throwaway room ("katsodi-native-
// check-<random>", never a real Katsodi room name), runs the tst_screen_live
// executable with them in its environment and prints its report. Secrets
// and tokens are never printed or written to disk.
import { spawnSync } from "node:child_process";
import { randomBytes } from "node:crypto";
import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { AccessToken, TrackSource } from "livekit-server-sdk";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const env = Object.fromEntries(
  readFileSync(path.join(root, ".env.local"), "utf8")
    .split(/\r?\n/)
    .filter((line) => /^[A-Z_]+=/.test(line))
    .map((line) => [line.slice(0, line.indexOf("=")), line.slice(line.indexOf("=") + 1).trim()]),
);
const { LIVEKIT_URL: url, LIVEKIT_API_KEY: key, LIVEKIT_API_SECRET: secret } = env;
if (!url?.startsWith("wss://") || !key || !secret) {
  console.error("LIVEKIT_URL (wss://), LIVEKIT_API_KEY and LIVEKIT_API_SECRET are needed in .env.local.");
  process.exit(1);
}
const room = `katsodi-native-check-${randomBytes(6).toString("hex")}`;
// The same grants as demoToken on the server: only the presenter may
// publish, and only a screen share. Data (shared-browser input and
// control) for the presenter, and for a viewer only while a host opened
// the demo channel: viewer B has it closed, viewer C open.
async function token(identity, publish, data = publish) {
  const access = new AccessToken(key, secret, { identity, ttl: 180 });
  access.addGrant({
    roomJoin: true,
    room,
    canPublish: publish,
    canSubscribe: true,
    canPublishData: data,
    canPublishSources: publish ? [TrackSource.SCREEN_SHARE, TrackSource.SCREEN_SHARE_AUDIO] : [],
  });
  return access.toJwt();
}
const exe = path.join(root, "native-desktop/build/windows-dev/tst_screen_live.exe");
const qt = path.resolve(root, "../KatsodiToolchain/Qt/6.11.3/msvc2022_64/bin");
console.log(`room ${room} on ${new URL(url).host}`);
const report = path.join(process.env.TEMP || root, `katsodi-screen-live-${process.pid}.txt`);
const result = spawnSync(exe, [...process.argv.slice(2), "-o", `${report},txt`], {
  stdio: "inherit",
  env: {
    ...process.env,
    PATH: `${qt};${process.env.PATH}`,
    KATSODI_LK_URL: url,
    KATSODI_LK_TOKEN_A: await token("native-presenter", true),
    KATSODI_LK_TOKEN_B: await token("native-viewer", false),
    KATSODI_LK_TOKEN_C: await token("native-viewer-open", false, true),
  },
});
if (result.error) console.error(result.error.message);
try {
  console.log(readFileSync(report, "utf8"));
} catch {
  console.error("no test report was written");
}
process.exit(result.status ?? 1);
