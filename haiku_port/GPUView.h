#ifndef GPUVIEW_H
#define GPUVIEW_H

#include <View.h>
#include <StringView.h>
#include <Box.h>
#include <Screen.h> // For BScreen and accelerant_device_info
#include <GraphicsDefs.h> // For accelerant_device_info (usually included by Screen.h)
#include <GridLayout.h>
#include <GroupLayout.h>
#include <SpaceLayoutItem.h>

class GPUView : public BView {
public:
    GPUView(BRect frame);
    ~GPUView();

    virtual void AttachedToWindow();
    // Pulse might not be needed if only static info is displayed
    // virtual void Pulse();

private:
    void UpdateData(); // Fetches and updates GPU info
    BString FormatBytes(uint64 bytes);

    // --- StringViews for displaying GPU Info ---
    BStringView* fCardNameLabel;
    BStringView* fCardNameValue;
    BStringView* fChipsetLabel;
    BStringView* fChipsetValue;
    BStringView* fMemorySizeLabel;
    BStringView* fMemorySizeValue;
    BStringView* fDacSpeedLabel;
    BStringView* fDacSpeedValue;
    BStringView* fDriverVersionLabel; // From accelerant_device_info.version
    BStringView* fDriverVersionValue;

    // Add more if relevant information is found in accelerant_device_info
    // BStringView* fErrorLabel; // To display errors if info cannot be fetched
};

#endif // GPUVIEW_H
