#pragma once
#include <string>
#include <vector>

namespace headwater {

struct ManifestItem {
  std::string channelId;
  std::string channel;
  std::string videoId;
  std::string videoTitle;
  std::string anchor;  // must match the EPUB TOC href for this summary
  std::string date;
};

struct Manifest {
  std::string issueId;
  std::string issueDate;
  std::vector<ManifestItem> items;
};

// Extract and parse the per-issue manifest from an EPUB zip.
// Returns false when the manifest entry is absent or the JSON is malformed.
bool loadManifest(const std::string& epubPath, Manifest& out);

}  // namespace headwater
