#include "DiskView.h"
#include <LayoutBuilder.h>
#include <StringView.h>
#include <stdio.h> // For snprintf
#include <device/scsi.h> // For device names, might not be needed for volume stats
#include <Directory.h>   // For iterating devices if needed
#include <Entry.h>
#include <SymLink.h>

#include <ColumnListView.h>
#include <ColumnTypes.h>


// Define column constants for BColumnListView
enum {
    kDeviceColumn,
    kMountPointColumn,
    kFSTypeColumn,
    kTotalSizeColumn,
    kFreeSizeColumn,
    kUsedSizeColumn, // Calculated: Total - Free
    kUsagePercentageColumn // Calculated
};


DiskView::DiskView(BRect frame)
    : BView(frame, "DiskView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW | B_PULSE_NEEDED)
{
    SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

    fDiskInfoBox = new BBox("DiskInfoBox");
    fDiskInfoBox->SetLabel("Disk Volumes");

    // Using BColumnListView for a structured display
    BRect clvRect = fDiskInfoBox->Bounds(); // Will be adjusted by layout
    clvRect.InsetBy(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);
    // Top edge needs to be below the label
    font_height fh;
    fDiskInfoBox->GetFontHeight(&fh);
    clvRect.top += fh.ascent + fh.descent + fh.leading + B_USE_DEFAULT_SPACING;


    BColumnListView* columnListView = new BColumnListView(clvRect, "disk_clv",
                                                          B_FOLLOW_ALL_SIDES,
                                                          B_WILL_DRAW | B_NAVIGABLE,
                                                          B_PLAIN_BORDER, true);

    columnListView->AddColumn(new BStringColumn("Device", 150, 50, 500, B_TRUNCATE_MIDDLE), kDeviceColumn);
    columnListView->AddColumn(new BStringColumn("Mount Point", 150, 50, 500, B_TRUNCATE_MIDDLE), kMountPointColumn);
    columnListView->AddColumn(new BStringColumn("FS Type", 80, 40, 200, B_TRUNCATE_END), kFSTypeColumn);
    columnListView->AddColumn(new BStringColumn("Total Size", 100, 50, 200, B_TRUNCATE_END, B_ALIGN_RIGHT), kTotalSizeColumn);
    columnListView->AddColumn(new BStringColumn("Used Size", 100, 50, 200, B_TRUNCATE_END, B_ALIGN_RIGHT), kUsedSizeColumn);
    columnListView->AddColumn(new BStringColumn("Free Size", 100, 50, 200, B_TRUNCATE_END, B_ALIGN_RIGHT), kFreeSizeColumn);
    columnListView->AddColumn(new BStringColumn("Usage %", 80, 40, 150, B_TRUNCATE_END, B_ALIGN_RIGHT), kUsagePercentageColumn);

    columnListView->SetSortColumn(columnListView->ColumnAt(kMountPointColumn), true, true); // Sort by Mount Point initially

    // Add the ColumnListView to the BBox using a layout
    BLayoutBuilder::Group<>(fDiskInfoBox, B_VERTICAL, 0)
        .SetInsets(B_USE_DEFAULT_SPACING, fh.ascent + fh.descent + fh.leading + B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
        .Add(columnListView);


    // Main layout for the DiskView itself
    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .SetInsets(0)
        .Add(fDiskInfoBox)
    .End();

    // fDiskListView will be the BColumnListView
    fDiskListView = columnListView;
}

DiskView::~DiskView()
{
    // fDiskListView is a child of fDiskInfoBox, which is a child of this view.
    // They will be deleted automatically.
}

void DiskView::AttachedToWindow()
{
    if (BColumnListView* clv = dynamic_cast<BColumnListView*>(fDiskListView)) {
         // Set target for selection messages if needed
    }
    UpdateData();
    BView::AttachedToWindow();
}

void DiskView::Pulse()
{
    UpdateData();
}

BString DiskView::FormatBytes(uint64 bytes) {
    BString str;
    double kb = bytes / 1024.0;
    double mb = kb / 1024.0;
    double gb = mb / 1024.0;

    if (gb >= 1.0) {
        str.SetToFormat("%.2f GiB", gb);
    } else if (mb >= 1.0) {
        str.SetToFormat("%.2f MiB", mb);
    } else { // Show KiB for smaller sizes as well, or directly bytes
        str.SetToFormat("%.0f KiB", kb); // For disk sizes, KiB is usually the minimum practical unit
    }
    return str;
}

void DiskView::GetDiskInfo(BVolume& volume, DiskInfo& info) {
    fs_info fsInfo;
    if (fs_stat_dev(volume.Device(), &fsInfo) == B_OK) {
        info.totalSize = fsInfo.total_blocks * fsInfo.block_size;
        info.freeSize = fsInfo.free_blocks * fsInfo.block_size;
        info.fileSystemType = fsInfo.fsh_name;

        // Get mount point
        BDirectory mountDir;
        volume.GetRootDirectory(&mountDir);
        BEntry mountEntry;
        mountDir.GetEntry(&mountEntry);
        BPath mountPath;
        mountEntry.GetPath(&mountPath);
        info.mountPoint = mountPath.Path();

        // Device name is a bit trickier, fs_info.device_name is often just a number.
        // We might need to iterate through /dev/disk or use other methods for a more user-friendly name.
        // For now, use what fs_info provides or a placeholder.
        info.deviceName = fsInfo.device_name; // This is often like "dev_#"
                                              // A more robust way involves querying the device itself.
                                              // For now, this is a simple starting point.
    } else {
        info.totalSize = 0;
        info.freeSize = 0;
        info.fileSystemType = "Error";
        info.mountPoint = "Error";
        info.deviceName = "Error";
    }
}


void DiskView::UpdateData()
{
    fLocker.Lock();

    BColumnListView* clv = dynamic_cast<BColumnListView*>(fDiskListView);
    if (!clv) {
        fLocker.Unlock();
        return;
    }

    // Store existing items to update or remove (simplistic update: clear and re-add)
    // A more optimized way would be to update existing rows and add/remove only changed ones.
    // For disk volumes, the list is usually stable, so clear-and-re-add is often acceptable.
    while (BRow* row = clv->RowAt(0)) {
        clv->RemoveRow(row);
        delete row;
    }

    BVolumeRoster volRoster;
    BVolume volume;
    volRoster.Rewind();

    while (volRoster.GetNextVolume(&volume) == B_OK) {
        if (volume.IsReadOnly() || volume.IsRemovable() || !volume.KnowsCapacity() || !volume.KnowsQuery()) {
            // Skip read-only, removable (like CD-ROMs unless specifically desired),
            // or volumes that don't report capacity or don't support queries (virtual FS etc.)
            // This logic might need refinement based on desired behavior.
            // For a system monitor, we usually care about fixed disks.
            // However, some removable media (USB drives) are relevant.
            // For now, let's list all that report capacity.
            if (!volume.KnowsCapacity()) continue;
        }

        DiskInfo currentDiskInfo;
        GetDiskInfo(volume, currentDiskInfo);

        if (currentDiskInfo.totalSize == 0) continue; // Skip if we couldn't get valid info

        BRow* row = new BRow();
        uint64 usedSize = currentDiskInfo.totalSize - currentDiskInfo.freeSize;
        double usagePercent = 0.0;
        if (currentDiskInfo.totalSize > 0) {
            usagePercent = (double)usedSize / currentDiskInfo.totalSize * 100.0;
        }
        char percentStr[16];
        snprintf(percentStr, sizeof(percentStr), "%.1f%%", usagePercent);

        row->SetField(new BStringField(currentDiskInfo.deviceName.String()), kDeviceColumn);
        row->SetField(new BStringField(currentDiskInfo.mountPoint.String()), kMountPointColumn);
        row->SetField(new BStringField(currentDiskInfo.fileSystemType.String()), kFSTypeColumn);
        row->SetField(new BStringField(FormatBytes(currentDiskInfo.totalSize).String()), kTotalSizeColumn);
        row->SetField(new BStringField(FormatBytes(usedSize).String()), kUsedSizeColumn);
        row->SetField(new BStringField(FormatBytes(currentDiskInfo.freeSize).String()), kFreeSizeColumn);
        row->SetField(new BStringField(percentStr), kUsagePercentageColumn);

        clv->AddRow(row);
    }

    // I/O statistics are harder. Haiku doesn't have a simple per-volume I/O counter accessible
    // through BVolume or fs_info. This often requires looking at raw device statistics,
    // which is more complex (e.g. /dev/disk/... and specific ioctls, or kernel interfaces).
    // For this iteration, we will focus on capacity and usage.
    // TODO: Research and implement disk I/O bytes/sec if feasible.

    fLocker.Unlock();
}

void DiskView::Draw(BRect updateRect)
{
    BView::Draw(updateRect);
    // Custom drawing if any (e.g. graphs for I/O, not implemented here)
}
