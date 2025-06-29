#include "ProcessView.h"
#include <LayoutBuilder.h>
#include <ColumnTypes.h>
#include <Button.h>
#include <kernel/image.h> // For image_info, get_next_image_info for path
#include <pwd.h>          // For getpwuid
#include <signal.h>       // For kill()
#include <Alert.h>
#include <Roster.h>       // For be_roster->Launch to open terminal
#include <Path.h>         // For BPath to find terminal

#include <set> // To keep track of active PIDs for removal

// Define column constants for BColumnListView
enum {
    kPIDColumn,
    kProcessNameColumn,
    kCPUUsageColumn,
    kMemoryUsageColumn,
    kThreadCountColumn,
    kUserNameColumn,
    kProcessPathColumn
};

// Context Menu Message
const uint32 MSG_KILL_PROCESS = 'kill';
const uint32 MSG_OPEN_TERMINAL_AT_PATH = 'otap'; // If we can get process path

ProcessView::ProcessView(BRect frame)
    : BView(frame, "ProcessView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW | B_PULSE_NEEDED),
      fLastPulseSystemTime(0)
{
    SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

    BBox* procBox = new BBox("ProcessListBox");
    procBox->SetLabel("Processes");

    BRect clvRect = procBox->Bounds();
    font_height fh;
    procBox->GetFontHeight(&fh);
    clvRect.top += fh.ascent + fh.descent + fh.leading + B_USE_DEFAULT_SPACING;
    clvRect.InsetBy(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);

    fProcessListView = new BColumnListView(clvRect, "process_clv",
                                           B_FOLLOW_ALL_SIDES,
                                           B_WILL_DRAW | B_NAVIGABLE,
                                           B_PLAIN_BORDER, true);

    fProcessListView->AddColumn(new BIntegerField("PID", 60, 30, 100), kPIDColumn);
    fProcessListView->AddColumn(new BStringColumn("Name", 180, 50, 500, B_TRUNCATE_END), kProcessNameColumn);
    fProcessListView->AddColumn(new BStringColumn("CPU %", 70, 40, 100, B_TRUNCATE_END, B_ALIGN_RIGHT), kCPUUsageColumn);
    fProcessListView->AddColumn(new BStringColumn("Memory", 100, 50, 200, B_TRUNCATE_END, B_ALIGN_RIGHT), kMemoryUsageColumn);
    fProcessListView->AddColumn(new BIntegerField("Threads", 60, 30, 100, B_ALIGN_RIGHT), kThreadCountColumn);
    fProcessListView->AddColumn(new BStringColumn("User", 80, 40, 150, B_TRUNCATE_END), kUserNameColumn);
    // fProcessListView->AddColumn(new BStringColumn("Path", 250, 50, 800, B_TRUNCATE_MIDDLE), kProcessPathColumn); // Path can be long

    fProcessListView->SetSortColumn(fProcessListView->ColumnAt(kCPUUsageColumn), false, false); // Sort by CPU descending

    // Context Menu
    fContextMenu = new BPopUpMenu("ProcessContext", false, false);
    fContextMenu->AddItem(new BMenuItem("Kill Process", new BMessage(MSG_KILL_PROCESS)));
    // fContextMenu->AddItem(new BMenuItem("Open Terminal Here", new BMessage(MSG_OPEN_TERMINAL_AT_PATH))); // If path is available
    fProcessListView->SetTarget(this); // For context menu messages if invoked directly on CLV
                                       // However, CLV handles context menu via MouseDown hook.

    BLayoutBuilder::Group<>(procBox, B_VERTICAL, 0)
        .SetInsets(B_USE_DEFAULT_SPACING, fh.ascent + fh.descent + fh.leading + B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
        .Add(fProcessListView);

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .SetInsets(0)
        .Add(procBox)
    .End();
}

ProcessView::~ProcessView()
{
    delete fContextMenu;
    // fProcessListView is child of procBox, auto-deleted
}

void ProcessView::AttachedToWindow()
{
    fProcessListView->SetTarget(this); // For selection messages and context menu
    UpdateData();
    fLastPulseSystemTime = system_time(); // Initialize for CPU calc
    BView::AttachedToWindow();
}

void ProcessView::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case MSG_KILL_PROCESS:
            KillSelectedProcess();
            break;
        // case MSG_OPEN_TERMINAL_AT_PATH:
            // Implement if path is reliably obtained and useful
            // break;
        default:
            BView::MessageReceived(message);
            break;
    }
}


void ProcessView::Pulse()
{
    UpdateData();
}

BString ProcessView::FormatBytes(uint64 bytes) {
    BString str;
    double kb = bytes / 1024.0;
    double mb = kb / 1024.0;

    if (mb >= 1.0) {
        str.SetToFormat("%.1f MiB", mb);
    } else if (kb >= 1.0) {
        str.SetToFormat("%.0f KiB", kb);
    } else {
        str.SetToFormat("%llu B", bytes);
    }
    return str;
}

BString ProcessView::GetUserName(uid_t uid) {
    struct passwd* pw = getpwuid(uid);
    if (pw) {
        return BString(pw->pw_name);
    }
    BString idStr;
    idStr << uid;
    return idStr; // Return UID as string if name not found
}


void ProcessView::ShowContextMenu(BPoint screenPoint) {
    BRow* selectedRow = fProcessListView->CurrentSelection();
    if (!selectedRow) return;

    // Enable/disable items based on selection if needed
    // Example: fContextMenu->FindItem(MSG_KILL_PROCESS)->SetEnabled(canKill);

    fContextMenu->SetTargetForItems(this); // Messages will be sent to this ProcessView
    fProcessListView->ConvertToScreen(&screenPoint);
    fContextMenu->Go(screenPoint, true, true, true); // async, stay open for clicks
}


void ProcessView::KillSelectedProcess() {
    BRow* selectedRow = fProcessListView->CurrentSelection();
    if (!selectedRow) return;

    BIntegerField* pidField = dynamic_cast<BIntegerField*>(selectedRow->GetField(kPIDColumn));
    if (!pidField) return;

    team_id team = pidField->Value();

    BString alertMsg;
    alertMsg.SetToFormat("Are you sure you want to kill process %d (%s)?",
                         (int)team,
                         ((BStringField*)selectedRow->GetField(kProcessNameColumn))->String());
    BAlert* confirmAlert = new BAlert("Confirm Kill", alertMsg.String(), "Kill", "Cancel",
                                      NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
    int32 button_index = confirmAlert->Go();

    if (button_index == 0) { // "Kill" button
        if (kill_team(team) != B_OK) { // or send_signal(team, SIGTERM); then SIGKILL
            BAlert* errAlert = new BAlert("Error", "Failed to kill process.", "OK",
                                          NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
            errAlert->Go();
        } else {
            // Optionally remove from list immediately or wait for next Pulse update
            fProcessListView->RemoveRow(selectedRow);
            delete selectedRow;
            fProcessTimeMap.erase(team); // Remove from CPU tracking map
        }
    }
}


void ProcessView::UpdateData()
{
    fLocker.Lock();

    // --- CPU Usage Calculation Setup ---
    bigtime_t currentSystemTime = system_time();
    bigtime_t systemTimeDelta = currentSystemTime - fLastPulseSystemTime;
    if (systemTimeDelta <= 0) systemTimeDelta = 1; // Avoid division by zero or negative if time jumped

    system_info sysInfo; // For CPU count
    get_system_info(&sysInfo);
    float totalPossibleCoreTime = sysInfo.cpu_count * systemTimeDelta;
    if (totalPossibleCoreTime <=0) totalPossibleCoreTime = 1.0f; // Safety

    // --- Prepare for Update: Mark existing rows or use a set of current PIDs ---
    std::set<team_id> activePIDsThisPulse;


    // --- Iterate Through Teams (Processes) ---
    int32 cookie = 0;
    team_info teamInfo;
    while (get_next_team_info(&cookie, &teamInfo) == B_OK) {
        activePIDsThisPulse.insert(teamInfo.team);

        ProcessInfo currentProc;
        currentProc.id = teamInfo.team;
        strncpy(currentProc.name.LockBuffer(B_OS_NAME_LENGTH), teamInfo.args, B_OS_NAME_LENGTH);
        currentProc.name.UnlockBuffer();
        // teamInfo.args often contains full path + arguments. We might want to parse just the executable name.
        // For simplicity, using args directly first. A more robust way is to get image_info.
        image_info imgInfo;
        int32 imgCookie = 0;
        if (get_next_image_info(teamInfo.team, &imgCookie, &imgInfo) == B_OK) {
            currentProc.name = BPath(&imgInfo.name).Leaf(); // Get executable name
            currentProc.path = imgInfo.name;
        } else {
             // Fallback if no image info (e.g. kernel threads or special processes)
            if (currentProc.name.Length() == 0) currentProc.name = "system_daemon";
        }


        currentProc.threadCount = teamInfo.thread_count;
        currentProc.areaCount = teamInfo.area_count; // Not directly memory usage yet
        currentProc.userID = teamInfo.uid;
        currentProc.userName = GetUserName(currentProc.userID);
        currentProc.totalKernelTime = teamInfo.kernel_time;
        currentProc.totalUserTime = teamInfo.user_time;


        // --- Calculate Memory Usage ---
        // Sum sizes of all areas owned by the team
        // This is a simplification; actual resident/shared memory is more complex.
        currentProc.memoryUsageBytes = 0;
        area_info areaInfo;
        int32 areaCookie = 0;
        while (get_next_area_info(teamInfo.team, &areaCookie, &areaInfo) == B_OK) {
            currentProc.memoryUsageBytes += areaInfo.ram_size; // ram_size is closer to resident
        }


        // --- Calculate CPU Usage ---
        float cpuPercent = 0.0f;
        if (fProcessTimeMap.count(currentProc.id)) {
            ProcessInfo& prevProc = fProcessTimeMap[currentProc.id];
            bigtime_t procKernelDelta = currentProc.totalKernelTime - prevProc.totalKernelTime;
            bigtime_t procUserDelta = currentProc.totalUserTime - prevProc.totalUserTime;
            bigtime_t procTotalTimeDelta = procKernelDelta + procUserDelta;

            if (procTotalTimeDelta < 0) procTotalTimeDelta = 0; // Time can jump backwards if process restarts with same PID quickly

            cpuPercent = (float)procTotalTimeDelta / totalPossibleCoreTime * 100.0f;
            // This uses total system time delta * cpu_count as denominator.
            // Alternative: (float)procTotalTimeDelta / systemTimeDelta * 100.0f; (for single core equivalent percent)
            // For multi-core view (like top), the first is more standard.
            if (cpuPercent < 0.0f) cpuPercent = 0.0f;
            // if (cpuPercent > 100.0f * sysInfo.cpu_count) cpuPercent = 100.0f * sysInfo.cpu_count; // Clamp to max possible
            if (cpuPercent > 100.0f) cpuPercent = 100.0f; // If using the per-core normalized percentage
        }
        currentProc.cpuUsage = cpuPercent;

        // Store current times for next calculation
        fProcessTimeMap[currentProc.id].totalKernelTime = currentProc.totalKernelTime;
        fProcessTimeMap[currentProc.id].totalUserTime = currentProc.totalUserTime;
        // Other fields of ProcessInfo in map are updated if needed, or just use it for times.


        // --- Find or Create Row in BColumnListView ---
        BRow* row = NULL;
        for (int32 i = 0; (row = fProcessListView->RowAt(i)) != NULL; i++) {
            BIntegerField* pidField = dynamic_cast<BIntegerField*>(row->GetField(kPIDColumn));
            if (pidField && pidField->Value() == currentProc.id) {
                break; // Found existing row
            }
        }

        if (row == NULL) { // New process, add a row
            row = new BRow();
            row->SetField(new BIntegerField(currentProc.id), kPIDColumn);
            row->SetField(new BStringField(currentProc.name.String()), kProcessNameColumn);

            char cpuStr[16]; // For CPU %
            snprintf(cpuStr, sizeof(cpuStr), "%.1f", currentProc.cpuUsage);
            row->SetField(new BStringField(cpuStr), kCPUUsageColumn);

            row->SetField(new BStringField(FormatBytes(currentProc.memoryUsageBytes).String()), kMemoryUsageColumn);
            row->SetField(new BIntegerField(currentProc.threadCount), kThreadCountColumn);
            row->SetField(new BStringField(currentProc.userName.String()), kUserNameColumn);
            // row->SetField(new BStringField(currentProc.path.String()), kProcessPathColumn);
            fProcessListView->AddRow(row);
        } else { // Existing process, update fields
            ((BStringField*)row->GetField(kProcessNameColumn))->SetString(currentProc.name.String());

            char cpuStr[16];
            snprintf(cpuStr, sizeof(cpuStr), "%.1f", currentProc.cpuUsage);
            ((BStringField*)row->GetField(kCPUUsageColumn))->SetString(cpuStr);

            ((BStringField*)row->GetField(kMemoryUsageColumn))->SetString(FormatBytes(currentProc.memoryUsageBytes).String());
            ((BIntegerField*)row->GetField(kThreadCountColumn))->SetValue(currentProc.threadCount);
            ((BStringField*)row->GetField(kUserNameColumn))->SetString(currentProc.userName.String());
            // ((BStringField*)row->GetField(kProcessPathColumn))->SetString(currentProc.path.String());
            fProcessListView->UpdateRow(row);
        }
    }

    // --- Remove Rows for Processes That No Longer Exist ---
    for (int32 i = 0; (i < fProcessListView->CountRows()); ) {
        BRow* row = fProcessListView->RowAt(i);
        BIntegerField* pidField = dynamic_cast<BIntegerField*>(row->GetField(kPIDColumn));
        if (pidField) {
            if (activePIDsThisPulse.find(pidField->Value()) == activePIDsThisPulse.end()) {
                // PID in list view is not in current active PIDs, remove it
                fProcessTimeMap.erase(pidField->Value()); // Clean up CPU map
                fProcessListView->RemoveRow(row);
                delete row;
                continue; // Don't increment i, as rows shifted
            }
        }
        i++;
    }

    // Re-sort if needed (e.g., if sort column is CPU and values changed)
    fProcessListView->SortItems();


    fLastPulseSystemTime = currentSystemTime;
    fLocker.Unlock();
}
