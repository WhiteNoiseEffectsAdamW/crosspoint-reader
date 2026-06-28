#include "HeadwaterManifest.h"

#include <ArduinoJson.h>
#include <Logging.h>
#include <ZipFile.h>

namespace headwater {

static constexpr char MANIFEST_ENTRY[] = "OEBPS/headwater-manifest.json";

bool loadManifest(const std::string& epubPath, Manifest& out) {
  ZipFile zip(epubPath);
  size_t size = 0;
  uint8_t* raw = zip.readFileToMemory(MANIFEST_ENTRY, &size, /*trailingNullByte=*/true);
  if (!raw) {
    return false;  // absent — EPUB built before the backend shipped manifests
  }

  JsonDocument doc;
  const auto err = deserializeJson(doc, reinterpret_cast<char*>(raw));
  free(raw);
  if (err) {
    LOG_ERR("HWMFST", "JSON parse error in %s: %s", epubPath.c_str(), err.c_str());
    return false;
  }

  out.issueId   = doc["issue"]["id"].as<const char*>() ?: "";
  out.issueDate = doc["issue"]["date"].as<const char*>() ?: "";
  out.items.clear();

  for (JsonObjectConst item : doc["items"].as<JsonArrayConst>()) {
    const char* channelId = item["channelId"] | "";
    const char* anchor    = item["anchor"] | "";
    if (!*channelId || !*anchor) continue;

    ManifestItem mi;
    mi.channelId  = channelId;
    mi.channel    = item["channel"] | "";
    mi.videoId    = item["videoId"] | "";
    mi.videoTitle = item["videoTitle"] | "";
    mi.anchor     = anchor;
    mi.date       = item["date"] | "";
    out.items.push_back(std::move(mi));
  }

  LOG_DBG("HWMFST", "%s: %zu items", out.issueId.c_str(), out.items.size());
  return true;
}

}  // namespace headwater
