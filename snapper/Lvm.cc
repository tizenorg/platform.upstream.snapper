/*
 * Copyright (c) [2011-2013] Novell, Inc.
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail, you may
 * find current contact information at www.novell.com.
 */


#include "config.h"

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <boost/algorithm/string.hpp>

#include "snapper/Log.h"
#include "snapper/Filesystem.h"
#include "snapper/Lvm.h"
#include "snapper/Snapper.h"
#include "snapper/SnapperTmpl.h"
#include "snapper/SystemCmd.h"
#include "snapper/SnapperDefines.h"
#include "snapper/Regex.h"


namespace snapper
{

    Filesystem*
    Lvm::create(const string& fstype, const string& subvolume)
    {
	Regex rx("^lvm\\(([_a-z0-9]+)\\)$");
	if (rx.match(fstype))
	    return new Lvm(subvolume, rx.cap(1));

	return NULL;
    }


    Lvm::Lvm(const string& subvolume, const string& mount_type)
	: Filesystem(subvolume), mount_type(mount_type)
    {
	if (access(LVCREATEBIN, X_OK) != 0)
	{
	    throw ProgramNotInstalledException(LVCREATEBIN " not installed");
	}

	if (access(LVSBIN, X_OK) != 0)
	{
	    throw ProgramNotInstalledException(LVSBIN " not installed");
	}

	bool found = false;
	MtabData mtab_data;

	if (!getMtabData(subvolume, found, mtab_data))
	    throw InvalidConfigException();

	if (!found)
	{
	    y2err("filesystem not mounted");
	    throw InvalidConfigException();
	}

	if (!detectThinVolumeNames(mtab_data))
	    throw InvalidConfigException();

	mount_options = filter_mount_options(mtab_data.options);
	if (mount_type == "xfs")
	    mount_options.push_back("nouuid");
    }


    void
    Lvm::createConfig() const
    {
	SDir subvolume_dir = openSubvolumeDir();

	int r1 = subvolume_dir.mkdir(".snapshots", 0750);
	if (r1 != 0 && errno != EEXIST)
	{
	    y2err("mkdir failed errno:" << errno << " (" << strerror(errno) << ")");
	    throw CreateConfigFailedException("mkdir failed");
	}
    }


    void
    Lvm::deleteConfig() const
    {
	SDir subvolume_dir = openSubvolumeDir();

	int r1 = subvolume_dir.unlink(".snapshots", AT_REMOVEDIR);
	if (r1 != 0)
	{
	    y2err("rmdir failed errno:" << errno << " (" << strerror(errno) << ")");
	    throw DeleteConfigFailedException("rmdir failed");
	}
    }


    string
    Lvm::snapshotDir(unsigned int num) const
    {
	return (subvolume == "/" ? "" : subvolume) + "/.snapshots/" + decString(num) +
	    "/snapshot";
    }


    SDir
    Lvm::openInfosDir() const
    {
	SDir subvolume_dir = openSubvolumeDir();
	SDir infos_dir(subvolume_dir, ".snapshots");

	struct stat stat;
	if (infos_dir.stat(&stat) != 0)
	{
	    throw IOErrorException();
	}

	if (stat.st_uid != 0)
	{
	    y2err(".snapshots must have owner root");
	    throw IOErrorException();
	}

	if (stat.st_gid != 0 && stat.st_mode & S_IWGRP)
	{
	    y2err(".snapshots must have group root or must not be group-writable");
	    throw IOErrorException();
	}

	if (stat.st_mode & S_IWOTH)
	{
	    y2err(".snapshots must not be world-writable");
	    throw IOErrorException();
	}

	return infos_dir;
    }


    SDir
    Lvm::openSnapshotDir(unsigned int num) const
    {
	SDir info_dir = openInfoDir(num);
	SDir snapshot_dir(info_dir, "snapshot");

	return snapshot_dir;
    }


    string
    Lvm::snapshotLvName(unsigned int num) const
    {
	return lv_name + "-snapshot" + decString(num);
    }


    void
    Lvm::createSnapshot(unsigned int num) const
    {
	SystemCmd cmd(LVCREATEBIN " --permission r --snapshot --name " +
		      quote(snapshotLvName(num)) + " " + quote(vg_name + "/" + lv_name));
	if (cmd.retcode() != 0)
	    throw CreateSnapshotFailedException();

	SDir info_dir = openInfoDir(num);
	int r1 = info_dir.mkdir("snapshot", 0755);
	if (r1 != 0 && errno != EEXIST)
	{
	    y2err("mkdir failed errno:" << errno << " (" << strerror(errno) << ")");
	    throw CreateSnapshotFailedException();
	}
    }


    void
    Lvm::deleteSnapshot(unsigned int num) const
    {
	SystemCmd cmd(LVREMOVEBIN " --force " + quote(vg_name + "/" + snapshotLvName(num)));
	if (cmd.retcode() != 0)
	    throw DeleteSnapshotFailedException();

	SDir info_dir = openInfoDir(num);
	info_dir.unlink("snapshot", AT_REMOVEDIR);

	SDir infos_dir = openInfosDir();
	infos_dir.unlink(decString(num), AT_REMOVEDIR);
    }


    bool
    Lvm::isSnapshotMounted(unsigned int num) const
    {
	bool mounted = false;
	MtabData mtab_data;

	if (!getMtabData(snapshotDir(num), mounted, mtab_data))
	    throw IsSnapshotMountedFailedException();

	return mounted;
    }


    void
    Lvm::mountSnapshot(unsigned int num) const
    {
	if (isSnapshotMounted(num))
	    return;

	SDir snapshot_dir = openSnapshotDir(num);

	if (!mount(getDevice(num), snapshot_dir.fd(), mount_type, mount_options))
	    throw MountSnapshotFailedException();
    }


    void
    Lvm::umountSnapshot(unsigned int num) const
    {
	if (!isSnapshotMounted(num))
	    return;

	SDir info_dir = openInfoDir(num);

	if (!umount(info_dir.fd(), "snapshot"))
	    throw UmountSnapshotFailedException();
    }


    bool
    Lvm::checkSnapshot(unsigned int num) const
    {
	struct stat stat;
	int r1 = ::stat(getDevice(num).c_str(), &stat);
	return r1 == 0 && S_ISBLK(stat.st_mode);
    }


    bool
    Lvm::detectThinVolumeNames(const MtabData& mtab_data)
    {
	Regex rx("^/dev/mapper/(.+[^-])-([^-].+)$");
	if (!rx.match(mtab_data.device))
	{
	    y2err("could not detect lvm names from '" << mtab_data.device << "'");
	    return false;
	}

	vg_name = boost::replace_all_copy(rx.cap(1), "--", "-");
	lv_name = boost::replace_all_copy(rx.cap(2), "--", "-");

	SystemCmd cmd(LVSBIN " -o segtype --noheadings " + quote(vg_name + "/" + lv_name));

	if (cmd.retcode() != 0) {
	    y2err("could not detect segment type infromation from: " << vg_name << "/" << lv_name);
	    return false;
	}

	string segtype = boost::trim_copy(cmd.getLine(0));

	if (segtype.compare("thin")) {
	    y2err(vg_name << "/" << lv_name << " is not a LVM thin volume");
	    return false;
	}

	return true;
    }

    string
    Lvm::getDevice(unsigned int num) const
    {
	return "/dev/mapper/" + boost::replace_all_copy(vg_name, "-", "--") + "-" +
	    boost::replace_all_copy(snapshotLvName(num), "-", "--");
    }

}
