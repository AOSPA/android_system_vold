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

#include <sys/mount.h>
#include <unistd.h>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include <logwrap/logwrap.h>

#include "Ntfs.h"
#include "Utils.h"

using android::base::StringPrintf;

namespace android {
namespace vold {
namespace ntfs {

static const char* kFsckPath = "/system/bin/fsck.ntfs";
static const char* kMkfsPath = "/system/bin/mkfs.ntfs";

bool IsSupported() {
    return access(kFsckPath, X_OK) == 0 && access(kMkfsPath, X_OK) == 0 &&
           IsFilesystemSupported("ntfs");
}

status_t Check(const std::string& source) {
    std::vector<std::string> cmd;
    cmd.push_back(kFsckPath);
    cmd.push_back(source);

    int rc = ForkExecvp(cmd, nullptr, sFsckUntrustedContext);
    if (rc == 0) {
        LOG(INFO) << "Check NTFS OK";
        return 0;
    } else {
        LOG(ERROR) << "Check NTFS failed (code " << rc << ")";
        errno = EIO;
        return -1;
    }
}

status_t Mount(const std::string& source, const std::string& target, bool ro, bool remount,
               bool executable, int ownerUid, int ownerGid, int permMask, bool /*createLost*/) {
    auto mountData = android::base::StringPrintf("nls=utf8,uid=%d,gid=%d,fmask=%o,dmask=%o",
                                                 ownerUid, ownerGid, permMask, permMask);
    unsigned long flags = MS_NODEV | MS_NOSUID | MS_DIRSYNC | MS_NOATIME;
    flags |= (executable ? 0 : MS_NOEXEC);
    flags |= (ro ? MS_RDONLY : 0);
    flags |= (remount ? MS_REMOUNT : 0);

    int rc = mount(source.c_str(), target.c_str(), "ntfs", flags, mountData.c_str());
    if (rc != 0 && !ro && errno == EROFS) {
        PLOG(ERROR) << "Mounting " << source << " failed; attempting read-only";
        flags |= MS_RDONLY;
        rc = mount(source.c_str(), target.c_str(), "ntfs", flags, mountData.c_str());
    }
    return rc;
}

status_t Format(const std::string& source, unsigned int numSectors) {
    if (access(kMkfsPath, X_OK)) {
        PLOG(ERROR) << "Problem accessing " << kMkfsPath;
        return -1;
    }

    std::vector<std::string> cmd;
    cmd.push_back(kMkfsPath);
    cmd.push_back(source);
    if (numSectors) {
        cmd.push_back(android::base::StringPrintf("%u", numSectors));
    }

    int rc = ForkExecvp(cmd);
    if (rc == 0) {
        LOG(INFO) << "Filesystem formatted";
    } else {
        LOG(ERROR) << "Format failed (code " << rc << ")";
        errno = EIO;
        rc = -1;
    }
    return rc;
}

}  // namespace ntfs
}  // namespace vold
}  // namespace android
