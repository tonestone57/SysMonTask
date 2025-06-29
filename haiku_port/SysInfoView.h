#ifndef SYSINFOVIEW_H
#define SYSINFOVIEW_H

#include <View.h>
#include <StringView.h>
#include <Box.h>
#include <GridLayout.h>
#include <GroupLayout.h>
#include <ScrollView.h> // In case content becomes too long

// Forward declare BTextView if used for multi-line disk info
class BTextView;

class SysInfoView : public BView {
public:
    SysInfoView(BRect frame);
    ~SysInfoView();

    virtual void AttachedToWindow();

private:
    void CreateLayout(); // Helper to set up the UI elements
    void LoadData();     // Helper to fetch and display data

    BString FormatBytes(uint64 bytes, int precision = 1);
    BString FormatHertz(uint64 hertz);
    BString FormatUptime(bigtime_t bootTime);
    BString GetCPUFeatureFlags();


    // --- OS Information ---
    BStringView* fKernelNameLabel;
    BStringView* fKernelNameValue;
    BStringView* fKernelVersionLabel;
    BStringView* fKernelVersionValue;
    BStringView* fKernelBuildLabel;
    BStringView* fKernelBuildValue;
    BStringView* fCPUArchLabel;
    BStringView* fCPUArchValue;
    BStringView* fUptimeLabel;
    BStringView* fUptimeValue;

    // --- CPU Information ---
    BStringView* fCPUModelLabel;
    BStringView* fCPUModelValue;
    BStringView* fCPUCoresLabel;
    BStringView* fCPUCoresValue;
    BStringView* fCPUClockSpeedLabel;
    BStringView* fCPUClockSpeedValue;
    BStringView* fCPUFeaturesLabel; // For SIMD etc.
    BStringView* fCPUFeaturesValue;
    // Cache Info
    BStringView* fL1CacheLabel;
    BStringView* fL1CacheValue;
    BStringView* fL2CacheLabel;
    BStringView* fL2CacheValue;
    BStringView* fL3CacheLabel;
    BStringView* fL3CacheValue;

    // --- Graphics Information ---
    BStringView* fGPUTypeLabel; // e.g. Card Name from accelerant
    BStringView* fGPUTypeValue;
    BStringView* fGPUDriverLabel;
    BStringView* fGPUDriverValue;
    BStringView* fGPUVRAMLabel;
    BStringView* fGPUVRAMValue;
    BStringView* fScreenResolutionLabel;
    BStringView* fScreenResolutionValue;

    // --- Memory Information ---
    BStringView* fTotalRAMLabel;
    BStringView* fTotalRAMValue;

    // --- Disk Information ---
    // This might be better as a BTextView or dynamically added BStringViews
    // if there are many volumes. For simplicity, start with one BStringView
    // and concatenate info, or use a BTextView.
    BTextView*   fDiskInfoTextView; // For multi-line disk info
    BScrollView* fDiskInfoScrollView;


    // Main container for all sections
    BBox* fMainSectionsBox;
};

#endif // SYSINFOVIEW_H
