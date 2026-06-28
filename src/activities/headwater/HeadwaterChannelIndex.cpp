#include "HeadwaterChannelIndex.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <map>

#include "HeadwaterManifest.h"
#include "HeadwaterPaths.h"

namespace headwater {

bool buildChannelIndex(ChannelIndex& out) {
  out.channels.clear();

  auto dir = Storage.open(ISSUES_DIR);
  if (!dir || !dir.isDirectory()) return false;

  std::map<std::string, Channel> byId;

  char nameBuf[256];
  dir.rewindDirectory();
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.isDirectory()) continue;
    file.getName(nameBuf, sizeof(nameBuf));
    const std::string filename{nameBuf};
    if (!FsHelpers::hasEpubExtension(filename)) continue;

    Manifest manifest;
    if (!loadManifest(std::string(ISSUES_DIR) + "/" + filename, manifest)) continue;

    for (const auto& item : manifest.items) {
      auto& ch = byId[item.channelId];
      if (ch.channelId.empty()) {
        ch.channelId   = item.channelId;
        ch.displayName = item.channel.empty() ? item.channelId : item.channel;
      }
      ch.entries.push_back({filename, item.anchor, item.videoTitle, item.date});
    }
  }

  if (byId.empty()) return false;

  out.channels.reserve(byId.size());
  for (auto& [id, ch] : byId) {
    std::sort(ch.entries.begin(), ch.entries.end(),
              [](const ChannelEntry& a, const ChannelEntry& b) { return a.date > b.date; });
    out.channels.push_back(std::move(ch));
  }
  std::sort(out.channels.begin(), out.channels.end(),
            [](const Channel& a, const Channel& b) { return a.displayName < b.displayName; });

  LOG_DBG("HWCIDX", "Built index: %zu channels", out.channels.size());
  return true;
}

}  // namespace headwater
