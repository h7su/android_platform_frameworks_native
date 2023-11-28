/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>
#include <binder/Binder.h>
#include <sys/types.h>
#include <fstream>
#include <list>
#include <regex>

#include <binderdebug/BinderDebug.h>

namespace android {

static std::string contextToString(BinderDebugContext context) {
    switch (context) {
        case BinderDebugContext::BINDER:
            return "binder";
        case BinderDebugContext::HWBINDER:
            return "hwbinder";
        case BinderDebugContext::VNDBINDER:
            return "vndbinder";
        default:
            return std::string();
    }
}

static status_t scanBinderContext(pid_t pid, const std::string& contextName,
                                  std::function<void(const std::string&)> eachLine) {
    std::ifstream ifs("/dev/binderfs/binder_logs/proc/" + std::to_string(pid));
    if (!ifs.is_open()) {
        ifs.open("/d/binder/proc/" + std::to_string(pid));
        if (!ifs.is_open()) {
            return -errno;
        }
    }

    bool allContexts = contextName.empty();
    bool isDesiredContext = false;
    std::string line;
    while (getline(ifs, line)) {
        if (!allContexts) {
            if (base::StartsWith(line, "context")) {
                isDesiredContext = base::Split(line, " ").back() == contextName;
                continue;
            }
            if (!isDesiredContext) {
                continue;
            }
        }
        eachLine(line);
    }
    return OK;
}

// Examples of what we are looking at:
// node 66730: u00007590061890e0 c0000759036130950 pri 0:120 hs 1 hw 1 ls 0 lw 0 is 2 iw 2 tr 1 proc 2300 1790
// thread 2999: l 00 need_return 1 tr 0
status_t getBinderPidInfo(BinderDebugContext context, pid_t pid, BinderPidInfo* pidInfo) {
    std::smatch match;
    static const std::regex kReferencePrefix("^\\s*node \\d+:\\s+u([0-9a-f]+)\\s+c([0-9a-f]+)\\s+");
    static const std::regex kThreadPrefix("^\\s*thread \\d+:\\s+l\\s+(\\d)(\\d)");
    std::string contextStr = contextToString(context);
    status_t ret = scanBinderContext(pid, contextStr, [&](const std::string& line) {
        if (base::StartsWith(line, "  node")) {
            std::vector<std::string> splitString = base::Tokenize(line, " ");
            bool pids = false;
            uint64_t ptr = 0;
            for (const auto& token : splitString) {
                if (base::StartsWith(token, "u")) {
                    const std::string ptrString = "0x" + token.substr(1);
                    if (!::android::base::ParseUint(ptrString.c_str(), &ptr)) {
                        LOG(ERROR) << "Failed to parse pointer: " << ptrString;
                        return;
                    }
                } else {
                    // The last numbers in the line after "proc" are all client PIDs
                    if (token == "proc") {
                        pids = true;
                    } else if (pids) {
                        int32_t pid;
                        if (!::android::base::ParseInt(token, &pid)) {
                            LOG(ERROR) << "Failed to parse pid int: " << token;
                            return;
                        }
                        if (ptr == 0) {
                            LOG(ERROR) << "We failed to parse the pointer, so we can't add the refPids";
                            return;
                        }
                        pidInfo->refPids[ptr].push_back(pid);
                    }
                }
            }
        } else if (base::StartsWith(line, "  thread")) {
            auto pos = line.find("l ");
            if (pos != std::string::npos) {
                // "1" is waiting in binder driver
                // "2" is poll. It's impossible to tell if these are in use.
                //     and HIDL default code doesn't use it.
                bool isInUse = line.at(pos + 2) != '1';
                // "0" is a thread that has called into binder
                // "1" is looper thread
                // "2" is main looper thread
                bool isBinderThread = line.at(pos + 3) != '0';
                if (!isBinderThread) {
                    return;
                }
                if (isInUse) {
                    pidInfo->threadUsage++;
                }

                pidInfo->threadCount++;
            }
        }
    });
    return ret;
}

// Examples of what we are looking at:
// ref 52493: desc 910 node 52492 s 1 w 1 d 0000000000000000
// node 29413: u00007803fc982e80 c000078042c982210 pri 0:139 hs 1 hw 1 ls 0 lw 0 is 2 iw 2 tr 1 proc 488 683
status_t getBinderClientPids(BinderDebugContext context, pid_t pid, pid_t servicePid,
                             int32_t handle, std::vector<pid_t>* pids) {
    std::smatch match;
    static const std::regex kNodeNumber("^\\s+ref \\d+:\\s+desc\\s+(\\d+)\\s+node\\s+(\\d+).*");
    std::string contextStr = contextToString(context);
    int32_t node;
    status_t ret = scanBinderContext(pid, contextStr, [&](const std::string& line) {
        if (!base::StartsWith(line, "  ref")) return;

        std::vector<std::string> splitString = base::Tokenize(line, " ");
        if (splitString.size() < 12) {
            LOG(ERROR) << "Failed to parse binder_logs ref entry. Expecting size greater than 11, but got: " << splitString.size();
            return;
        }
        int32_t desc;
        if (!::android::base::ParseInt(splitString[3].c_str(), &desc)) {
            LOG(ERROR) << "Failed to parse desc int: " << splitString[3];
            return;
        }
        if (handle != desc) {
            return;
        }
        if (!::android::base::ParseInt(splitString[5].c_str(), &node)) {
            LOG(ERROR) << "Failed to parse node int: " << splitString[5];
            return;
        }
        LOG(INFO) << "Parsed the node: " << node;
    });
    if (ret != OK) {
        return ret;
    }

    ret = scanBinderContext(servicePid, contextStr, [&](const std::string& line) {
        if (!base::StartsWith(line, "  node")) return;

        std::vector<std::string> splitString = base::Tokenize(line, " ");
        if (splitString.size() < 21) {
            LOG(ERROR) << "Failed to parse binder_logs node entry. Expecting size greater than 20, but got: " << splitString.size();
            return;
        }

        // remove the colon
        const std::string nodeString = splitString[1].substr(0, splitString[1].size() - 1);
        int32_t matchedNode;
        if (!::android::base::ParseInt(nodeString.c_str(), &matchedNode)) {
            LOG(ERROR) << "Failed to parse node int: " << nodeString;
            return;
        }

        if (node != matchedNode) {
            return;
        }
        bool pidsSection = false;
        for (const auto& token : splitString) {
            if (token == "proc") {
                pidsSection = true;
            } else if (pidsSection == true) {
                int32_t pid;
                if (!::android::base::ParseInt(token.c_str(), &pid)) {
                    LOG(ERROR) << "Failed to parse PID int: " << token;
                    return;
                }
                pids->push_back(pid);
            }
        }
    });
    return ret;
}

#define contains(val, l) (std::find(l.begin(), l.end(), val) != l.end())

static inline status_t findBinderTransactions(int pid, const std::string& contextStr,
                                              std::list<int>& workingPids,
                                              BinderTransactionInfo& workInfo) {
    // First, mark current target pid as scanned pid.
    workInfo.scannedPids.push_back(pid);
    status_t ret = scanBinderContext(pid, contextStr, [&](const std::string& line) {
        std::smatch match;
        const static std::regex kBinderTransaction(
                "\\s*(outgoing|incoming|pending)" // group1: direction
                "\\s+transaction\\s+-?\\d+:\\s+-?\\w+"
                "\\s+from\\s+-?\\d+:-?\\d+"
                "\\s+to\\s+(-?\\d+):-?\\d+.*"); // group2: to pid
        if (std::regex_match(line, match, kBinderTransaction)) {
            std::string direction = match[1].str();
            int toPid = stoi(match[2].str());
            // Add new target pid if found a new outgoing pid.
            if (toPid != 0 && direction == "outgoing") {
                if (!contains(toPid, workInfo.scannedPids) && !contains(toPid, workingPids)) {
                    workingPids.push_back(toPid);
                }
            }
            // Add raw transactions info line to log string.
            workInfo.trLines.push_back(line);
        }
    });
    return ret;
}

// Examples of what we are looking at:
// outgoing transaction 879906: 0000000000000000 from 16129:16129 to 18658:18723 code 3 flags 10 pri
// 0:110 r1 incoming transaction 899093: 0000000000000000 from 20205:20236 to 16129:16386 code 2
// flags 12 pri 0:130 r1 node 743337 size 212:8 data 0000000000000000 pending transaction 1067148:
// 0000000000000000 from 0:0 to 23076:0 code 1 flags 11 pri 0:120 r0 node 1034234 size 824:0 data
// 0000000000000000
status_t getBinderTransactions(BinderDebugContext context, pid_t startPid,
                               BinderTransactionInfo& workInfo) {
    std::string contextStr = contextToString(context);
    workInfo.scannedPids.clear();
    workInfo.trLines.clear();

    std::list<int> workingPids;
    status_t status = OK;

    // Scan binder transactions chain start from startPid.
    workingPids.push_back(startPid);
    while (!workingPids.empty()) {
        // Pick first one and remove from workingPids.
        int currentPid = workingPids.front();
        workingPids.pop_front();
        // workingPids, workInfo.scannedPids and workInfo.trLines will be modified.
        status_t ret = findBinderTransactions(currentPid, contextStr, workingPids, workInfo);
        if (ret != OK) {
            LOG(ERROR) << "Failed to get the binder transaction info of pid "
                       << std::to_string(currentPid) << ", " << statusToString(ret);
            if (status == OK) {
                status = ret;
            }
        }
    }
    return status;
}

} // namespace  android
