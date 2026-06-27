#pragma once

// Single source of truth for where Headwater issues live on the SD card.
// OpdsSyncActivity writes issues here; HeadwaterAppActivity reads them back.
// Kept separate from the SD root so issues don't litter the file browser and so
// the Headwater app can scope to one place.
namespace headwater {
inline constexpr char ISSUES_DIR[] = "/Headwater";
}  // namespace headwater
