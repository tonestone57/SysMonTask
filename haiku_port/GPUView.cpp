#include "GPUView.h"
#include <stdio.h> // For snprintf

GPUView::GPUView(BRect frame)
    : BView(frame, "GPUView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW)
      // No B_PULSE_NEEDED if only static data displayed on attach
{
    SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

    BGroupLayout* layout = new BGroupLayout(B_VERTICAL);
    SetLayout(layout);

    BBox* infoBox = new BBox("GPUInfoBox");
    infoBox->SetLabel("Graphics Card Information");

    BGridLayout* grid = new BGridLayout(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);
    BLayoutBuilder::Grid<>(grid)
        .SetInsets(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
        .Add(fCardNameLabel = new BStringView("gpu_name_label", "Card Name:"), 0, 0)
        .Add(fCardNameValue = new BStringView("gpu_name_value", "N/A"), 1, 0)
        .Add(BSpaceLayoutItem::CreateGlue(), 2, 0) // Glue to push to left

        .Add(fChipsetLabel = new BStringView("gpu_chipset_label", "Chipset:"), 0, 1)
        .Add(fChipsetValue = new BStringView("gpu_chipset_value", "N/A"), 1, 1)
        .Add(BSpaceLayoutItem::CreateGlue(), 2, 1)

        .Add(fMemorySizeLabel = new BStringView("gpu_mem_label", "Memory Size:"), 0, 2)
        .Add(fMemorySizeValue = new BStringView("gpu_mem_value", "N/A"), 1, 2)
        .Add(BSpaceLayoutItem::CreateGlue(), 2, 2)

        .Add(fDacSpeedLabel = new BStringView("gpu_dac_label", "DAC Speed:"), 0, 3)
        .Add(fDacSpeedValue = new BStringView("gpu_dac_value", "N/A"), 1, 3)
        .Add(BSpaceLayoutItem::CreateGlue(), 2, 3)

        .Add(fDriverVersionLabel = new BStringView("gpu_driver_label", "Driver Version:"), 0, 4)
        .Add(fDriverVersionValue = new BStringView("gpu_driver_value", "N/A"), 1, 4)
        .Add(BSpaceLayoutItem::CreateGlue(), 2, 4);

    grid->SetColumnWeight(2, 1.0f); // Make the glue column expand for all rows

    infoBox->AddChild(grid->View());
    layout->AddView(infoBox);
    layout->AddGlue(); // Pushes the box to the top
}

GPUView::~GPUView()
{
    // Child views are auto-deleted
}

void GPUView::AttachedToWindow()
{
    UpdateData();
    BView::AttachedToWindow();
}

BString GPUView::FormatBytes(uint64 bytes) {
    BString str;
    double mb = bytes / (1024.0 * 1024.0);
    double gb = mb / 1024.0;

    if (gb >= 1.0) {
        str.SetToFormat("%.2f GiB", gb);
    } else { // For GPU memory, MiB is a common unit
        str.SetToFormat("%.0f MiB", mb);
    }
    return str;
}

void GPUView::UpdateData()
{
    BScreen screen(B_MAIN_SCREEN_ID);
    if (!screen.IsValid()) {
        fCardNameValue->SetText("Error: Invalid screen object");
        return;
    }

    accelerant_device_info deviceInfo;
    if (screen.GetDeviceInfo(&deviceInfo) == B_OK) {
        fCardNameValue->SetText(deviceInfo.name);
        fChipsetValue->SetText(deviceInfo.chipset);
        fMemorySizeValue->SetText(FormatBytes(deviceInfo.memory).String());

        char dacSpeedStr[32];
        snprintf(dacSpeedStr, sizeof(dacSpeedStr), "%lu MHz", deviceInfo.dac_speed / 1000); // DAC speed often in kHz
        fDacSpeedValue->SetText(dacSpeedStr);

        char versionStr[32];
        snprintf(versionStr, sizeof(versionStr), "%lu", deviceInfo.version);
        fDriverVersionValue->SetText(versionStr);

    } else {
        fCardNameValue->SetText("Error: Could not get device info");
        fChipsetValue->SetText("-");
        fMemorySizeValue->SetText("-");
        fDacSpeedValue->SetText("-");
        fDriverVersionValue->SetText("-");
    }

    // As this data is static, no need to Invalidate() or schedule further pulses
    // unless we find a way to get dynamic GPU stats.
}
