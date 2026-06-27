# Headwater Backend — Handoff & Requirements

This is the **handoff list for the Headwater backend team**. The device-side Headwater
edition (v1 sync) is shipped and working; the **v-next Headwater App** (cross-day channel
browsing on the reader) is **gated on the backend changes below**. None of these are
shipped yet — this doc is the contract.

See [HEADWATER.md](HEADWATER.md) for the device-side design.

---

## TL;DR — what the backend must deliver

1. **Unique, date-stamped issue titles** in the OPDS feed (idempotency). *(Blocks v1 robustness — do first.)*
2. **One TOC/chapter entry per video summary** inside each daily-digest EPUB, each with a stable anchor. *(Enables deep-linking.)*
3. **A per-issue manifest** describing each summary: `{channel, videoTitle, anchor, date, new}`. *(Enables the channels view.)*

Items 2 + 3 together unblock the cross-day Headwater App. Item 1 is independent and should land first.

---

## 1. Unique, date-stamped issue titles  (priority: now)

The device decides "do I already have this issue?" purely by filename, derived from the OPDS
`<title>`: it skips download if `sanitizeFilename(author + " - " + title).epub` already exists
on the SD card.

**Requirement:** every daily issue must have a **globally unique title**, ideally date-stamped,
e.g. `Headwater Daily — 2026-06-27`. If two different issues ever share a title, the second is
silently treated as "already downloaded" and skipped.

This is the only hard requirement for v1 to be robust. Confirm the current feed already does this.

---

## 2. EPUB authoring — one chapter per summary  (priority: gates the app)

The reader deep-links by **spine index + anchor** (verified: `Epub::getTocItem(i)` →
`{spineIndex, anchor, title, level}`; the reader navigates to exactly that). For the channels
view to "open the digest at a specific summary," each summary must be addressable.

**Requirement:** in each daily-digest EPUB, author **each video summary as its own TOC entry**
(a chapter / nav point) with a **stable anchor** — either its own spine document
(`summary-<videoId>.xhtml`) or a fragment id (`<section id="v-<videoId>">`) referenced from the
EPUB nav/TOC.

- The anchor must be **stable across rebuilds** of the same issue (use the YouTube video id, not a hash of content).
- The anchor in the manifest (item 3) must exactly match what the EPUB's nav/TOC points to, so the device can resolve manifest entry → TOC entry → spine position.

This keeps the device's reader engine untouched — we reuse the existing chapter-jump path.

---

## 3. Per-issue manifest  (priority: gates the app)

The device can read an EPUB's TOC, but the TOC alone doesn't carry **channel grouping**,
**publish date per video**, or **"new since last sync"** state. The manifest supplies that
structured metadata so the device never has to parse titles or guess.

### Schema

One manifest **per issue**, listing every summary in that issue:

```json
{
  "issue": {
    "id": "2026-06-27",
    "title": "Headwater Daily — 2026-06-27",
    "date": "2026-06-27"
  },
  "items": [
    {
      "channel": "Veritasium",
      "videoTitle": "The Surprising Physics of ...",
      "videoId": "abc123",
      "anchor": "summary-abc123.xhtml",
      "date": "2026-06-26",
      "new": true
    }
  ]
}
```

Field notes:
- `channel` — display name; the device groups across issues by this. Keep spelling/casing stable (it's the grouping key). A stable `channelId` alongside the display name would be even better if cheap.
- `videoTitle` — shown in the channels list.
- `anchor` — **must match** the EPUB TOC target for this summary (see item 2). Spine path and/or `#fragment`.
- `date` — the video's publish date (or summary date); used for sorting within a channel.
- `new` — true if this is newly added in this issue vs. the prior one. (Device also infers "new since last sync" itself, so this is a hint, not load-bearing.)

### Delivery mechanism — pick one (backend's choice; device adapts)

The current OPDS parser only captures `title / author / href / id` (no custom link rels), so
the simplest options are:

- **Option A (recommended): embed the manifest inside the EPUB** at a fixed path, e.g.
  `OEBPS/headwater-manifest.json` or `META-INF/headwater.json`. It travels atomically with the
  issue (no second request, no sync mismatch), and the device already unzips EPUBs. *(Device-side
  TODO: confirm the EPUB lib can extract an arbitrary entry by path.)*
- **Option B: sidecar URL by convention** — same URL as the acquisition link with a `.json`
  extension, or a dedicated `…/manifest/<issueId>.json` endpoint. Requires the device to fetch a
  second file per issue and a firm URL convention.
- **Option C: custom OPDS `<link rel="…/headwater-manifest" href="…">`** on each acquisition
  entry. Cleanest semantically but requires extending the device's OPDS parser to capture the rel.

Recommendation: **Option A** unless there's a reason the manifest must change without
re-issuing the EPUB.

---

## Open questions for the backend team

- Does the current feed already give each issue a unique date-stamped title? (item 1)
- Can the EPUB generator emit one TOC entry per summary with a stable, video-id-based anchor? (item 2)
- Preferred manifest delivery — embed in EPUB (A), sidecar URL (B), or OPDS link rel (C)?
- Is there a stable `channelId` we can use as the grouping key, or only the display name?
- "Saved videos" vs "daily digest": are user-saved videos delivered in the same daily EPUB, or
  as separate acquisition entries? (Affects the device's Today / Digests / Saved sectioning.)

---

## What the device will do with this (for context)

On each sync, the device merges every issue's manifest into a rolling **cross-issue index** on
the SD card: `channel -> [{ digestFile, anchor, date, videoTitle, read? }]`. The Headwater App
then renders a channels view from that index; selecting an item opens the owning digest EPUB at
the anchor, and on Back auto-advances to the next unread item. Read state is maintained
device-side. Old issues (and their index entries) are pruned by a retention window.
