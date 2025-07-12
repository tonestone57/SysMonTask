#ifndef CPUVIEW_H
#define CPUVIEW_H

#include <View.h>
#include <StringView.h>
#include <Box.h>
#include <GridLayout.h>
#include <GroupLayout.h>
#include <SpaceLayoutItem.h>
#include <Locker.h>
#include <kernel/OS.h> // For system_info, get_system_info

// Forward declaration
class BStringView;

class CPUView : public BView {
public:
    CPUView(BRect frame);
    ~CPUView();

    virtual void AttachedToWindow();
    virtual void Pulse(); // Called periodically to update data
    virtual void Draw(BRect updateRect); // For drawing graphs later

private:
    void UpdateData();    // Fetches and updates CPU data
    void GetCPUUsage(float* usage); // Gets overall CPU usage

    BStringView* fOverallUsageLabel;
    BStringView* fOverallUsageValue;
    // We will add more detailed views (per-core, graphs) later

    // For calculating CPU usage
    system_info fPreviousSysInfo;
    bigtime_t*  fPreviousIdleTime; // Dynamically allocated array
    uint32      fCpuCount;         // Number of CPUs
    bool        fFirstTime;
    bigtime_t   fPreviousTime;

    BLocker fLocker; // For thread safety if needed for data updates
};

#endif // CPUVIEW_H
