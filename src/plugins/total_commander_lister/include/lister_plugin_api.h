#pragma once

#include <cstdint>

namespace klogg::tc::lister {

// Result codes returned by lister entry points.
constexpr int kResultOk = 0;
constexpr int kResultError = 1;
constexpr int kResultNotImplemented = -1;

// Show flags passed to ListLoad/ListLoadNext.
constexpr int kShowWrapText = 1 << 0;          // lcp_wraptext
constexpr int kShowFitToWindow = 1 << 1;       // lcp_fittowindow
constexpr int kShowAlwaysLoad = 1 << 2;        // lcp_alwaysload
constexpr int kShowSearchIsFilter = 1 << 3;    // lcp_searchisfilter

// Search parameter flags passed to ListSearchText.
constexpr int kSearchMatchCase = 1 << 0;       // lcs_matchcase
constexpr int kSearchBackwards = 1 << 1;       // lcs_backwards
constexpr int kSearchRepeat = 1 << 2;          // lcs_repeatsearch
constexpr int kSearchRegex = 1 << 3;           // lcs_regex
constexpr int kSearchWholeWords = 1 << 4;      // lcs_wholewords (treated as plain string)

// Commands passed to ListSendCommand.
constexpr int kCommandCopy = 1;
constexpr int kCommandSelectAll = 2;
constexpr int kCommandNewParameters = 3;
constexpr int kCommandRefresh = 4;
constexpr int kCommandJumpToNext = 5;
constexpr int kCommandJumpToPrevious = 6;
constexpr int kCommandGetCapabilities = 7;
constexpr int kCommandNewParametersExtended = 8;

// Capabilities returned from kCommandGetCapabilities.
constexpr int kCapabilitySupportsTextSearch = 1 << 0;
constexpr int kCapabilityHandlesMultipleFiles = 1 << 1;
constexpr int kCapabilityHasOwnSearchDialog = 1 << 2;
constexpr int kCapabilitySupportsCopy = 1 << 3;
constexpr int kCapabilitySupportsSelectAll = 1 << 4;

} // namespace klogg::tc::lister
