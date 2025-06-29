// Implementation for MemView - Memory Monitoring Tab

#include "MemView.h"
#include <Box.h>
#include <GridLayout.h>
#include <GroupLayoutBuilder.h>
#include <SpaceLayoutItem.h>
#include <StringView.h>
#include <stdio.h> // For snprintf
#include <string.h> // For memset
#include <kernel/OS.h> // For get_system_info, system_info
#include <InterfaceDefs.h> // For ui_color, B_PANEL_BACKGROUND_COLOR

// GraphView Implementation
GraphView::GraphView(BRect frame, const char* name, const float* historyData, const int* historyIndex, const uint64* totalValue)
    : BView(frame, name, B_FOLLOW_ALL_SIDES, B_WILL_DRAW | B_PULSE_NEEDED), // B_PULSE_NEEDED if it self-updates, otherwise relies on parent Invalidate
      fHistoryData(historyData),
      fHistoryIndex(historyIndex),
      fTotalValue(totalValue) {
    SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR)); // Or a different background for the graph
    // Set a default graph color, can be changed
    fGraphColor = (rgb_color){ 0, 128, 255, 255 }; // Blue
}

void GraphView::AttachedToWindow()
{
    // If the graph view needed its own pulse or specific setup when added to window.
    // For now, it will be updated when MemView calls Invalidate on it.
    BView::AttachedToWindow();
}

void GraphView::Draw(BRect updateRect)
{
    SetLowColor(ViewColor()); // Match background for FillRect
    FillRect(updateRect, B_SOLID_LOW);

    if (!fHistoryData || !fHistoryIndex || !fTotalValue || *fTotalValue == 0) {
        // Draw "N/A" or some placeholder if data is not ready
        BFont font;
        GetFont(&font);
        font_height fh;
        font.GetHeight(&fh);
        const char* message = "Graph N/A";
        float stringWidth = font.StringWidth(message);
        BPoint textPoint(Bounds().Width() / 2 - stringWidth / 2, Bounds().Height() / 2 + fh.ascent / 2);
        DrawString(message, textPoint);
        return;
    }

    SetHighColor(fGraphColor);

    BRect bounds = Bounds();
    float barWidth = bounds.Width() / HISTORY_SIZE;
    int currentIndex = *fHistoryIndex;

    for (int i = 0; i < HISTORY_SIZE; i++) {
        // Calculate index in history array, wrapping around
        int historyIdx = (currentIndex + i) % HISTORY_SIZE;
        float value = fHistoryData[historyIdx]; // This is already a percentage 0-100

        if (value < 0) continue; // Skip uninitialized data points

        float barHeight = (value / 100.0f) * bounds.Height(); // Scale percentage to view height
        if (barHeight < 0) barHeight = 0;
        if (barHeight > bounds.Height()) barHeight = bounds.Height();

        BRect barRect;
        barRect.left = bounds.left + i * barWidth;
        barRect.right = barRect.left + barWidth -1; // -1 for a small gap or exact fit
        barRect.top = bounds.bottom - barHeight;
        barRect.bottom = bounds.bottom;

        FillRect(barRect);
    }

    // Draw a border around the graph area
    SetHighColor(0,0,0); // Black border
    StrokeRect(bounds);
}


// MemView Implementation
MemView::MemView(BRect frame)
    : BView(frame, "MemoryView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW | B_PULSE_NEEDED),
      fCacheGraphView(NULL),
      fCacheHistoryIndex(0),
      fTotalSystemMemory(0)
{
    SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    memset(fCacheHistory, 0, sizeof(fCacheHistory)); // Initialize history to 0

    // --- Total Memory ---
    fTotalMemLabel = new BStringView("total_mem_label", "Total Memory:");
    fTotalMemValue = new BStringView("total_mem_value", "N/A");

    // --- Used Memory ---
    fUsedMemLabel = new BStringView("used_mem_label", "Used Memory:");
    fUsedMemValue = new BStringView("used_mem_value", "N/A");

    // --- Free Memory (Calculated or from system) ---
    // Haiku's system_info provides used_pages and max_pages. Free is max - used.
    // Some systems provide "free" directly, others "available" which is more useful.
    // For simplicity, we can show used/total. If "free" is specifically desired:
    fFreeMemLabel = new BStringView("free_mem_label", "Free Memory:");
    fFreeMemValue = new BStringView("free_mem_value", "N/A");


    // --- Cached Memory ---
    // system_info has block_cache_pages and cached_pages (file system cache)
    fCachedMemLabel = new BStringView("cached_mem_label", "Cached Memory:");
    fCachedMemValue = new BStringView("cached_mem_value", "N/A");


    BBox* statsBox = new BBox("MemoryStatsBox");
    statsBox->SetLabel("Memory Statistics");

    BGridLayout* gridLayout = new BGridLayout(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);
    BLayoutBuilder::Grid<>(gridLayout)
        .SetInsets(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
        .Add(fTotalMemLabel, 0, 0)
        .Add(fTotalMemValue, 1, 0)
        .Add(BSpaceLayoutItem::CreateGlue(), 2, 0) // Glue to push to left

        .Add(fUsedMemLabel, 0, 1)
        .Add(fUsedMemValue, 1, 1)
        .Add(BSpaceLayoutItem::CreateGlue(), 2, 1)

        .Add(fFreeMemLabel, 0, 2)
        .Add(fFreeMemValue, 1, 2)
        .Add(BSpaceLayoutItem::CreateGlue(), 2, 2)

        .Add(fCachedMemLabel, 0, 3)
        .Add(fCachedMemValue, 1, 3)
        .Add(BSpaceLayoutItem::CreateGlue(), 2, 3);

    gridLayout->SetColumnWeight(2, 1.0f); // Make the glue column expand

    statsBox->AddChild(gridLayout->View());

    // --- Cache Graph View ---
    // Calculate a reasonable frame for the graph view.
    // For example, make it take the remaining width and a fixed height.
    BRect graphFrame = Bounds(); // Start with full view bounds
    graphFrame.top = statsBox->Frame().bottom + B_USE_DEFAULT_SPACING; // Position below the stats box
    graphFrame.bottom = graphFrame.top + 80; // Fixed height for the graph, adjust as needed
    // In a more complex layout, you'd use BLayoutBuilder for this too.
    // For now, let's ensure it's added to a BGroupLayout.

    fCacheGraphView = new GraphView(BRect(0,0,10,10), "cacheGraph", fCacheHistory, &fCacheHistoryIndex, &fTotalSystemMemory);
    // The BRect(0,0,10,10) is a placeholder, actual size managed by layout.
    fCacheGraphView->SetExplicitMinSize(BSize(0, 60)); // Min height for graph
    fCacheGraphView->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 100)); // Max height for graph


    BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
        .SetInsets(B_USE_DEFAULT_SPACING)
        .Add(statsBox)
        .Add(fCacheGraphView)
        .AddGlue(); // Pushes content to the top, graph will expand horizontally.
}

MemView::~MemView()
{
    // Child views are typically deleted by their parent or the window
}

void MemView::AttachedToWindow()
{
    UpdateData();
    BView::AttachedToWindow();
}

void MemView::Pulse()
{
    UpdateData();
}

BString MemView::FormatBytes(uint64 bytes) {
    BString str;
    double kb = bytes / 1024.0;
    double mb = kb / 1024.0;
    double gb = mb / 1024.0;

    if (gb >= 1.0) {
        str.SetToFormat("%.2f GiB", gb);
    } else if (mb >= 1.0) {
        str.SetToFormat("%.2f MiB", mb);
    } else if (kb >= 1.0) {
        str.SetToFormat("%.2f KiB", kb);
    } else {
        str.SetToFormat("%llu Bytes", bytes);
    }
    return str;
}

void MemView::UpdateData()
{
    fLocker.Lock();

    system_info sysInfo;
    if (get_system_info(&sysInfo) == B_OK) {
        uint64 totalBytes = (uint64)sysInfo.max_pages * B_PAGE_SIZE;
        uint64 usedBytes = (uint64)sysInfo.used_pages * B_PAGE_SIZE;
        uint64 freeBytes = totalBytes - usedBytes; // Calculated free

        // Cached memory: Haiku has `cached_pages` (file system cache) and `block_cache_pages` (block I/O cache)
        // We can sum them or show them separately. For simplicity, sum them for "Cached".
        uint64 cachedBytes = ((uint64)sysInfo.cached_pages + (uint64)sysInfo.block_cache_pages) * B_PAGE_SIZE;


        fTotalMemValue->SetText(FormatBytes(totalBytes).String());
        fUsedMemValue->SetText(FormatBytes(usedBytes).String());
        fFreeMemValue->SetText(FormatBytes(freeBytes).String());
        fCachedMemValue->SetText(FormatBytes(cachedBytes).String());

        fTotalSystemMemory = totalBytes; // Store for graph context or percentage calculation

        // Update cache history for graph
        if (fTotalSystemMemory > 0) {
            float cachePercent = (float)cachedBytes / fTotalSystemMemory * 100.0f;
            if (cachePercent < 0.0f) cachePercent = 0.0f;
            if (cachePercent > 100.0f) cachePercent = 100.0f; // Clamp
            fCacheHistory[fCacheHistoryIndex] = cachePercent;
        } else {
            fCacheHistory[fCacheHistoryIndex] = 0; // Or some error indicator like -1 if graph handles it
        }
        fCacheHistoryIndex = (fCacheHistoryIndex + 1) % HISTORY_SIZE;

        if (fCacheGraphView) {
            fCacheGraphView->Invalidate(); // Trigger graph redraw
        }

    } else {
        fTotalSystemMemory = 0; // Reset if error
        fTotalMemValue->SetText("Error");
        fUsedMemValue->SetText("Error");
        fFreeMemValue->SetText("Error");
        fCachedMemValue->SetText("Error");
    }

    fLocker.Unlock();
}

// Graph drawing would go into Draw() method if we add a live graph later.
// void MemView::Draw(BRect updateRect) {
// BView::Draw(updateRect);
// Draw graph using fMemHistory data
// }
