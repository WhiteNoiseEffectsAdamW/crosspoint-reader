# Headwater Edition

A **private, additive fork** of CrossPoint that auto-loads the [Headwater](https://headwaterapp.com) daily issue onto the
device. Headwater is a YouTube-summary service whose backend exposes a per-user **OPDS catalog + EPUB generation**; this
fork removes the last manual step — getting each new issue *onto* the reader.

Everything in upstream CrossPoint is unchanged. This edition only **adds** an on-device sync path on top, so upstream
updates can be re-merged when wanted.

## What it adds

A **"Headwater"** entry on the Home screen — shown only when a Headwater feed is configured, and **pre-selected at
boot**. Selecting it opens `HeadwaterAppActivity`, which shows a "Check Headwater" item at the top of the list followed
by downloaded issues. Selecting "Check Headwater" runs a headless `OpdsSyncActivity` that:

1. connects Wi-Fi from saved credentials (`WIFI_STORE`),
2. fetches the Headwater OPDS feed and downloads any issue not already on the SD card,
3. tears down Wi-Fi and silently restarts back to Home.

It is **skippable** (Back button, including mid-download) and bounded by a connect timeout (20 s) and an overall budget
(120 s).

### Phasing

- **v1 (shipped, both X3 and X4): two-press, user-triggered sync.** Boot pre-selects the "Headwater" Home entry;
  one press opens the app with "Check Headwater" pre-selected; a second press launches sync. Never runs from `setup()`,
  so a crash just reboots to Home with no automatic retry.
- **v2 (deferred, X3 only): fully-automatic on-wake sync.** A gate in `setup()` using the X3's battery-backed DS3231 RTC
  (epoch-delta cadence, `configTzTime()` SNTP, attempt-counter quarantine). Not yet implemented.

## Design decisions

- **One feed, not two.** The backend serves a single per-user OPDS root (token-keyed). The daily digest *and*
  user-selected videos both arrive as acquisition entries; per-video title editing is backend-side via the OPDS
  `<title>`. The device just downloads what's new.
- **Feed identity.** The Headwater feed is the first saved server in `OPDS_STORE` whose URL host contains
  `headwaterapp.com` (the per-user token is embedded in that URL, entered once on-device). No new storage/schema.
- **Why X4 can't auto-sync.** On battery, deep sleep is a **full power-off** via a GPIO13 latch MOSFET
  (`lib/hal/HalPowerManager.cpp`) — the MCU, internal RTC, and RTC memory are all lost, and the only battery wake is the
  power button. So the X4 keeps no wall clock across sleep, making an offline "is it a new day?" gate impossible there.
  Only the X3's coin-cell-backed DS3231 keeps real time. Hence v1 is one-press on both; v2 auto is X3-only.
- **Atomic downloads.** Issues download to `<name>.epub.part` and are renamed to `.epub` only on full completion, so a
  truncated or aborted download never leaves a partial file in the library.
- **Idempotency by filename.** A feed entry is skipped if `sanitizeFilename(author + " - " + title).epub` already exists.
  **Backend requirement:** each daily issue's title must be unique (e.g. date-stamped), or a new issue with an identical
  title would be treated as "already have it" and skipped.

## Files

**New**
- [src/activities/network/OpdsSyncActivity.h](src/activities/network/OpdsSyncActivity.h) /
  [.cpp](src/activities/network/OpdsSyncActivity.cpp) — the headless sync flow.
- [src/activities/headwater/HeadwaterPaths.h](src/activities/headwater/HeadwaterPaths.h) — single source of truth for
  the `/Headwater` issues folder, shared by the sync (writer) and the app (reader).
- [src/activities/headwater/HeadwaterAppActivity.h](src/activities/headwater/HeadwaterAppActivity.h) /
  [.cpp](src/activities/headwater/HeadwaterAppActivity.cpp) — the Headwater app shell: an offline list with a
  "Check Headwater" sync item on top and the downloaded issues (newest-first) below, opening the selected issue in the
  reader. The frame the channels view layers onto.
- [src/components/icons/headwater.h](src/components/icons/headwater.h) — the 32×32 three-wave Headwater menu icon.
- [src/images/HeadwaterEdition.h](src/images/HeadwaterEdition.h) — the "Headwater Edition" boot wordmark, a 1-bit raster
  of EB Garamond Regular (wght 400, 0.01em tracking) baked at 24px (192×18). Per the Headwater design team spec.

**Modified**
- [src/OpdsServerStore.h](src/OpdsServerStore.h) / [.cpp](src/OpdsServerStore.cpp) — `getHeadwaterServer()` (URL
  host-match).
- [src/network/HttpDownloader.h](src/network/HttpDownloader.h) / [.cpp](src/network/HttpDownloader.cpp) —
  `downloadToFileAtomic()` (`.part` → rename).
- [src/activities/home/HomeActivity.h](src/activities/home/HomeActivity.h) /
  [.cpp](src/activities/home/HomeActivity.cpp) and [src/activities/ActivityManager.h](src/activities/ActivityManager.h) —
  `HomeMenuItem::HEADWATER_APP`: single Headwater home entry (pre-selected at boot), dispatch.
- [lib/I18n/translations/english.yaml](lib/I18n/translations/english.yaml) — `STR_HEADWATER_*` strings.
- [src/components/themes/BaseTheme.h](src/components/themes/BaseTheme.h) (`UIIcon::Headwater`) and
  [src/components/themes/lyra/LyraTheme.cpp](src/components/themes/lyra/LyraTheme.cpp) (`iconForName` 32px case) —
  register the wave icon. (Lyra is the only theme that draws menu icons; the others ignore `rowIcon`.)
- [src/activities/boot_sleep/BootActivity.cpp](src/activities/boot_sleep/BootActivity.cpp) — draws the "Headwater Edition"
  wordmark as a secondary attribution line under the CrossPoint mark. (`drawImage` is an opaque byte-copy, so x is
  byte-aligned to a multiple of 8.)

## Build

```bash
pio run -e default
```

(Requires PlatformIO — see [Development quick start](README.md#development-quick-start). The web flasher only installs
official upstream releases, so this fork must be built from source.)

## Setup on device

Add your personal Headwater feed URL (with token) as an OPDS server via the web settings UI or on-device OPDS settings.
The sync auto-detects it by the `headwaterapp.com` host.

## Headwater app (cross-day channel browsing)

A dedicated Headwater browsing experience layered on the v1 sync. Design (locked in dialogue):

- **App shell — SHIPPED.** A single "Headwater" Home entry (pre-selected at boot) opens `HeadwaterAppActivity`: a
  flat list with "Check Headwater" at the top (launches sync) followed by downloaded issues newest-first. This is the
  frame the manifest-driven views below layer onto. Everything below this point is still **gated on the backend
  manifest** — see [HEADWATER_BACKEND.md](HEADWATER_BACKEND.md).
- **Stored unit = the daily digest EPUB** (plus user "custom export" EPUBs). Each summary is authored once and
  lives in exactly one artifact — "channel" is a *view*, never a re-stored per-channel copy. No duplicate bytes.
- **Backend dependency — per-issue manifest.** Each synced issue ships a small sidecar manifest: per summary
  `{channel, videoTitle, anchor, date, new?}`. The device merges manifests into a rolling **cross-issue index**
  (`channel -> [{digest file, anchor, date, title, read?}]`) on the SD card. Structured data in — no fragile
  on-device title parsing.
- **Headwater Home** — sectioned list of issues across days (Today / Daily digests / Saved videos) with unread
  state. Covers only on a "today" hero if issues have distinct art; otherwise typographic.
- **Channels view** — index-driven: channel -> its items across days; select an item -> the reader opens the
  owning digest at that anchor; on Back the list **auto-advances to the next unread item** (Option A). The reader
  engine is left untouched.
- **Read state** — per-item read flags maintained device-side (mark on open / scroll-past); "new since last sync"
  comes from the manifest.
- **Retention / pruning** — the sync keeps N days/issues and prunes old digests together with their index entries.
- **Deferred (Option B)** — a seamless cross-file reading queue ("next" flows across EPUB files) needs invasive
  changes to the reader engine; revisit only if Option A leaves something wanting.

Sequence: build only after v1's on-device sync is verified and the backend manifest exists.

## Open items

- [x] First clean compile (`pio run -e default`) — **passes** (RAM 30.9%, Flash 79.9%).
- [x] On-device verification — happy path **verified**, idempotency (re-run = no duplicate) **verified**. Still to confirm: skip mid-download, no-Wi-Fi error.
- [x] Headwater app shell — second Home entry + offline issues list opening in the reader. Compiles; on-device check pending.
- [ ] Confirm the backend gives each daily issue a unique title (see idempotency note above).
- [ ] Backend handoff (see [HEADWATER_BACKEND.md](HEADWATER_BACKEND.md)): unique titles, per-summary TOC entries, per-issue manifest — gates the channels view.
- [ ] Headwater app — cross-issue index (from manifest) + channels view (Option A) + read-state + Today/Digests/Saved sectioning + retention.
- [ ] v2: X3 automatic on-wake sync.
- [ ] Optional: QR pairing to replace cumbersome on-device token entry.
