#ifndef DISKVIEW_H
#define DISKVIEW_H

#include <View.h>
#include <StringView.h>
#include <Box.h>
#include <ListView.h> // For listing multiple disks/partitions
#include <ScrollView.h>
#include <Locker.h>
#include <Path.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <fs_info.h>

struct DiskInfo {
    BString     deviceName;     // e.g., /dev/disk/ata/0/master/raw
    BString     mountPoint;     // e.g., /boot or /Haiku
    BString     fileSystemType; // e.g., bfs, ntfs
    uint64      totalSize;
    uint64      freeSize;
    // Add more if needed, e.g. read/write stats (might be harder to get per volume)
    // For overall disk activity, might need to monitor specific device nodes
};

class DiskView : public BView {
public:
    DiskView(BRect frame);
    ~DiskView();

    virtual void AttachedToWindow();
    virtual void Pulse();
    virtual void Draw(BRect updateRect);

private:
    void UpdateData();
    BString FormatBytes(uint64 bytes);
    void GetDiskInfo(BVolume& volume, DiskInfo& info);

    // For simplicity, we'll display info for all mounted volumes.
    // A BListView could be used to list disks, and selecting one shows details.
    // Or, a BColumnListView for a table of disks.
    // For now, let's create a simple text area for each disk.

    BBox*       fDiskInfoBox; // A container for all disk info
    BScrollView* fScrollView; // If content might exceed view bounds
    BView*      fDiskListView; // A simple view to hold multiple StringViews or custom items

    // We will dynamically add BStringViews to fDiskListView for each disk.
    // This requires careful management of these views if they are numerous or change.
    // A BColumnListView is more robust for dynamic lists.

    BLocker fLocker;
};

#endif // DISKVIEW_H
