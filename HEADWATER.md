# Headwater Edition

A **private, additive fork** of CrossPoint that auto-loads the [Headwater](https://headwaterapp.com) daily issue onto the
device. Headwater is a YouTube-summary service whose backend exposes a per-user **OPDS catalog + EPUB generation**; this
fork removes the last manual step — getting each new issue *onto* the reader.

Everything in upstream CrossPoint is unchanged. This edition only **adds** an on-device sync path on top, so upstream
updates can be re-merged when wanted.

## What it adds

A **"Check Headwater"** entry on the Home screen — shown only when a Headwater feed is configured, and **pre-selected at
boot** so a new issue is one button-press away. Selecting it runs a headless `OpdsSyncActivity` that:

1. connects Wi-Fi from saved credentials (`WIFI_STORE`),
2. fetches the Headwater OPDS feed and downloads any issue not already on the SD card,
3. tears down Wi-Fi and silently restarts back to Home.

It is **skippable** (Back button, including mid-download) and bounded by a connect timeout (20 s) and an overall budget
(120 s).

### Phasing

- **v1 (shipped, both X3 and X4): one-press, user-triggered sync.** Launched only from the Home entry — never from
  `setup()`. Because nothing runs during boot, a crash or hang just reboots to Home with no automatic retry, so there is
  no boot-loop to guard against.
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
  [.cpp](src/activities/headwater/HeadwaterAppActivity.cpp) — the Headwater app shell: an offline, newest-first list of
  downloaded issues that opens the selected issue in the reader. The frame the channels view layers onto.

**Modified**
- [src/OpdsServerStore.h](src/OpdsServerStore.h) / [.cpp](src/OpdsServerStore.cpp) — `getHeadwaterServer()` (URL
  host-match).
- [src/network/HttpDownloader.h](src/network/HttpDownloader.h) / [.cpp](src/network/HttpDownloader.cpp) —
  `downloadToFileAtomic()` (`.part` → rename).
- [src/activities/home/HomeActivity.h](src/activities/home/HomeActivity.h) /
  [.cpp](src/activities/home/HomeActivity.cpp) and [src/activities/ActivityManager.h](src/activities/ActivityManager.h) —
  `HomeMenuItem::HEADWATER_SYNC` and `HEADWATER_APP`: two Headwater menu entries (sync pre-selected at boot, app below),
  dispatch.
- [lib/I18n/translations/english.yaml](lib/I18n/translations/english.yaml) — `STR_HEADWATER_*` strings.

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

- **App shell — SHIPPED.** A second Home entry "Headwater" (below "Check Headwater") opens `HeadwaterAppActivity`: a
  flat, newest-first list of the issues in `/Headwater`, opening the selected issue in the existing reader. This is the
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
