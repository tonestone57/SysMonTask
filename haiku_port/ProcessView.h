#ifndef PROCESSVIEW_H
#define PROCESSVIEW_H

#include <View.h>
#include <ColumnListView.h>
#include <Locker.h>
#include <String.h>
#include <kernel/OS.h> // For team_info, thread_info, get_team_info etc.
#include <PopUpMenu.h>
#include <MenuItem.h>

// For CPU usage calculation
#include <map>

struct ProcessInfo {
    team_id     id;
    BString     name;
    BString     path; // Full path to executable
    int32       threadCount;
    int32       areaCount; // Number of memory areas
    uint64      memoryUsageBytes; // Sum of area sizes
    float       cpuUsage; // Percentage
    uid_t       userID;
    BString     userName;

    // For CPU usage calculation over time
    bigtime_t   totalKernelTime;
    bigtime_t   totalUserTime;
    bigtime_t   lastPollKernelTime;
    bigtime_t   lastPollUserTime;
    bigtime_t   lastPollSystemTime; // system_time() at last poll for this process

    ProcessInfo() : id(0), threadCount(0), areaCount(0), memoryUsageBytes(0), cpuUsage(0.0f),
                    userID(0), totalKernelTime(0), totalUserTime(0),
                    lastPollKernelTime(0), lastPollUserTime(0), lastPollSystemTime(0) {}
};


// Map to store previous CPU times for processes for usage calculation.
// Key: team_id, Value: ProcessInfo (containing previous times)
typedef std::map<team_id, ProcessInfo> ProcessTimeMap;


class ProcessView : public BView {
public:
    ProcessView(BRect frame);
    ~ProcessView();

    virtual void AttachedToWindow();
    virtual void Pulse();
    virtual void MessageReceived(BMessage* message); // For context menu actions

private:
    void UpdateData();
    BString FormatBytes(uint64 bytes);
    BString GetUserName(uid_t uid);
    void ShowContextMenu(BPoint screenPoint);
    void KillSelectedProcess();


    BColumnListView*    fProcessListView;
    BPopUpMenu*         fContextMenu;
    BLocker             fLocker;

    ProcessTimeMap      fProcessTimeMap; // Stores info for CPU usage calculation
    bigtime_t           fLastPulseSystemTime; // system_time() at the last global pulse
};

#endif // PROCESSVIEW_H
