#include "CPUView.h"
#include <stdio.h> // For sprintf
#include <String.h>
#include <SystemInfo.h> // For BSystemInfo, if used for modern Haiku CPU stats
#include <kernel/OS.h>  // For get_system_info, system_info, cpu_info

#include <string.h> // For memset, if used for initialization

CPUView::CPUView(BRect frame)
    : BView(frame, "CPUView", B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP, B_WILL_DRAW | B_PULSE_NEEDED),
      fPreviousIdleTime(NULL), // Initialize pointer
      fCpuCount(0),
      fFirstTime(true) {

    SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

    BGroupLayout* layout = new BGroupLayout(B_VERTICAL);
    SetLayout(layout);

    // Overall Usage Section
    BBox* overallBox = new BBox("OverallCPUBox");
    overallBox->SetLabel("Overall CPU Usage");
    BGridLayout* grid = new BGridLayout(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);
    BLayoutBuilder::Grid<>(grid)
        .SetInsets(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
        .Add(fOverallUsageLabel = new BStringView("overall_label", "Total Usage:"), 0, 0)
        .Add(fOverallUsageValue = new BStringView("overall_value", "0.0%"), 1, 0)
        .Add(BSpaceLayoutItem::CreateGlue(), 2, 0); // Add glue to push items to the left
    grid->SetColumnWeight(2, 1.0f); // Make the glue column expand
    overallBox->AddChild(grid->View());
    layout->AddView(overallBox);

    // Initialize CPU count and previous times
    system_info sysInfo;
    if (get_system_info(&sysInfo) == B_OK) {
        fPreviousSysInfo = sysInfo; // Store initial system_info
        fCpuCount = sysInfo.cpu_count;
        if (fCpuCount > 0) {
            fPreviousIdleTime = new bigtime_t[fCpuCount];
            // It's good practice to check if new returned NULL, though unlikely on modern systems
            // if (fPreviousIdleTime == NULL) { /* Handle allocation error */ }
            for (uint32 i = 0; i < fCpuCount; ++i) {
                fPreviousIdleTime[i] = sysInfo.cpu_infos[i].active_time; // Actually active time
            }
        } else {
            // No CPUs reported, or error. fPreviousIdleTime remains NULL.
            fOverallUsageValue->SetText("No CPU data");
        }
    } else {
        // Handle error - perhaps disable CPU view or show an error message
        fOverallUsageValue->SetText("Error fetching CPU info");
        // fPreviousIdleTime remains NULL, fCpuCount remains 0
    }

    // Placeholder for graphs - will be a separate custom view
    // BView* graphView = new BView(BRect(0,0,100,50), "cpuGraph", B_FOLLOW_ALL, B_WILL_DRAW);
    // graphView->SetViewColor(200, 200, 220); // Light blue-ish
    // layout->AddView(graphView);

    layout->AddGlue(); // Add glue to push everything to the top
}

CPUView::~CPUView() {
    // Any cleanup
    delete[] fPreviousIdleTime;
    fPreviousIdleTime = NULL; // Good practice
}

void CPUView::AttachedToWindow() {
    // Called when the view is added to a window
    UpdateData(); // Initial data fetch
    BView::AttachedToWindow();
}

void CPUView::Pulse() {
    // Called periodically by the window's pulse mechanism
    UpdateData();
}

void CPUView::GetCPUUsage(float* overallUsage) {
    if (fCpuCount == 0 || fPreviousIdleTime == NULL) {
        *overallUsage = -1.0f; // Indicate error or N/A, will be handled by UpdateData
        return;
    }

    system_info currentSysInfo;
    if (get_system_info(&currentSysInfo) != B_OK) {
        *overallUsage = -1.0f; // Indicate error
        return;
    }

    bigtime_t currentTime = system_time();

    if (fFirstTime) {
        fPreviousSysInfo = currentSysInfo;
		fPreviousTime = currentTime;
        fFirstTime = false;
        *overallUsage = 0.0f; // No data for first pulse
        // Update previous idle times for next calculation
        for (uint32 i = 0; i < fCpuCount; ++i) {
             fPreviousIdleTime[i] = currentSysInfo.cpu_infos[i].active_time;
        }
        return;
    }

    bigtime_t elapsedWallTime = currentTime - fPreviousTime;
	fPreviousTime = currentTime;

    if (elapsedWallTime <= 0) { // Time hasn't advanced or error
        *overallUsage = 0.0; // Or previous value
        return;
    }

    float totalDeltaActiveTime = 0;
    for (uint32 i = 0; i < fCpuCount; ++i) {
        bigtime_t deltaCoreActiveTime = currentSysInfo.cpu_infos[i].active_time - fPreviousIdleTime[i];
        totalDeltaActiveTime += deltaCoreActiveTime;
        fPreviousIdleTime[i] = currentSysInfo.cpu_infos[i].active_time; // Update for next round
    }

    // Overall usage: (total change in active time across all cores) / (wall time elapsed * number of cores)
    if (fCpuCount > 0) {
        *overallUsage = (float)totalDeltaActiveTime / (elapsedWallTime * fCpuCount) * 100.0f;
    } else {
        *overallUsage = 0.0f;
    }

    // Clamp usage between 0 and 100
    if (*overallUsage < 0.0f) *overallUsage = 0.0f;
    if (*overallUsage > 100.0f) *overallUsage = 100.0f; // Can happen due to timing/precision
}


void CPUView::UpdateData() {
    fLocker.Lock(); // Lock if data is accessed from other threads, good practice for Pulse

    float usage;
    GetCPUUsage(&usage);

    if (usage >= 0) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.1f%%", usage);
        fOverallUsageValue->SetText(buffer);
    } else {
        fOverallUsageValue->SetText("N/A");
    }

    // Invalidate(); // If we were drawing graphs, we'd invalidate to trigger Draw()
    fLocker.Unlock();
}

void CPUView::Draw(BRect updateRect) {
    // This is where graph drawing code would go.
    // For now, it's handled by child BStringViews.
    BView::Draw(updateRect);

    // Example: Draw a simple rectangle for the graph area
    // BRect graphArea = Bounds(); // Or a specific rect for the graph
    // graphArea.InsetBy(5,5);
    // SetHighColor(180, 180, 200);
    // FillRect(graphArea);
    // SetHighColor(0,0,0);
    // StrokeRect(graphArea);
}
