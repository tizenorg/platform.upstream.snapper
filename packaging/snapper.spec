#
# spec file for package snapper
#
# Copyright (c) 2013 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

Name:           snapper
Version:        0.1.5
Release:        0
Source:         snapper-%{version}.tar.gz
Source1001: 	snapper.manifest
BuildRequires:  gettext-tools
BuildRequires:  sed
BuildRequires:  boost-devel
BuildRequires:  doxygen
BuildRequires:  gcc-c++
BuildRequires:  libtool
BuildRequires:  libxml2-devel
BuildRequires:  pkg-config
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  libzypp(plugin:commit)
Requires:       diffutils
Requires:       libsnapper = %version
Recommends:     snapper-zypp-plugin
Supplements:    btrfs-progs
Summary:        Tool for filesystem snapshot management
License:        GPL-2.0
Group:          System/Utilities
Url:            http://en.opensuse.org/Portal:Snapper

%description
This package contains snapper, a tool for filesystem snapshot management.

%package -n snapper-zypp-plugin
Requires:       dbus-python
Requires:       snapper
Requires:       zypp-plugin-python
Requires:       libzypp(plugin:commit)
Summary:        A zypp commit plugin for calling snapper
Group:          System/Utilities

%description -n snapper-zypp-plugin
This package contains a plugin for zypp that makes filesystem snapshots with
snapper during commits.


%package -n libsnapper-devel
Requires:       boost-devel
Requires:       gcc-c++
Requires:       libsnapper = %version
Requires:       libstdc++-devel
Requires:       libxml2-devel
Summary:        Header files and documentation for libsnapper
Group:          System/Development

%description -n libsnapper-devel
This package contains header files and documentation for developing with
libsnapper.


%package -n libsnapper
Summary:        Library for filesystem snapshot management
Group:          System/Libraries
Requires:       util-linux

%description -n libsnapper
This package contains libsnapper, a library for filesystem snapshot management.


%prep
%setup -n snapper-%{version}
cp %{SOURCE1001} .

%build
export CFLAGS="$RPM_OPT_FLAGS -DNDEBUG"
export CXXFLAGS="$RPM_OPT_FLAGS -DNDEBUG"

%reconfigure --disable-ext4 --disable-silent-rules --disable-pam
make %{?jobs:-j%jobs}

%install
%make_install

mkdir -p $RPM_BUILD_ROOT/usr/lib/systemd/system/
mkdir -p $RPM_BUILD_ROOT/usr/lib/systemd/system/multi-user.target.wants

install -D scripts/snapper-hourly $RPM_BUILD_ROOT/usr/sbin/snapper-hourly
install -D scripts/snapper-daily $RPM_BUILD_ROOT/usr/sbin/snapper-daily
install -D data/snapper-hourly.timer $RPM_BUILD_ROOT/usr/lib/systemd/system/snapper-hourly.timer
install -D data/snapper-hourly.service $RPM_BUILD_ROOT/usr/lib/systemd/system/snapper-hourly.service
install -D data/snapper-daily.timer $RPM_BUILD_ROOT/usr/lib/systemd/system/snapper-daily.timer
install -D data/snapper-daily.service $RPM_BUILD_ROOT/usr/lib/systemd/system/snapper-daily.service
install -D data/sysconfig.snapper $RPM_BUILD_ROOT/etc/sysconfig/snapper
install -D $RPM_BUILD_ROOT/etc/snapper/config-templates/default $RPM_BUILD_ROOT/etc/snapper/configs/root

sed -i 's|ALLOW_USERS=""|ALLOW_USERS="root"|' $RPM_BUILD_ROOT/etc/snapper/configs/root
sed -i 's|SNAPPER_CONFIGS=""|SNAPPER_CONFIGS="root"|' $RPM_BUILD_ROOT/etc/sysconfig/snapper

%install_service multi-user.target.wants snapper-hourly.timer
%install_service multi-user.target.wants snapper-daily.timer

rm -rf %{buildroot}%{_datadir}/doc/snapper

%{find_lang} snapper

%post -n libsnapper  -p /sbin/ldconfig

%postun -n libsnapper -p /sbin/ldconfig


%lang_package

%files
%manifest %{name}.manifest
%defattr(-,root,root)
%license COPYING
%{_prefix}/bin/snapper
%{_prefix}/sbin/snapperd
%{_prefix}/sbin/snapper-hourly
%{_prefix}/sbin/snapper-daily
%config /etc/dbus-1/system.d/org.opensuse.Snapper.conf
%{_prefix}/share/dbus-1/system-services/org.opensuse.Snapper.service
%{_unitdir}/snapper-hourly.timer
%{_unitdir}/multi-user.target.wants/snapper-hourly.timer
%{_unitdir}/snapper-hourly.service
%{_unitdir}/snapper-daily.timer
%{_unitdir}/multi-user.target.wants/snapper-daily.timer
%{_unitdir}/snapper-daily.service

%files -n libsnapper
%manifest %{name}.manifest
%defattr(-,root,root)
%license COPYING
%{_libdir}/libsnapper.so.*
%dir %{_sysconfdir}/snapper
%dir %{_sysconfdir}/snapper/configs
%config(noreplace) %{_sysconfdir}/snapper/configs/root
%dir %{_sysconfdir}/snapper/config-templates
%config(noreplace) %{_sysconfdir}/snapper/config-templates/default
%dir %{_sysconfdir}/snapper/filters
%config(noreplace) %{_sysconfdir}/snapper/filters/*.txt
%config(noreplace) %{_sysconfdir}/sysconfig/snapper

%files -n libsnapper-devel
%manifest %{name}.manifest
%defattr(-,root,root)
%{_libdir}/libsnapper.so
%{_prefix}/include/snapper


%files -n snapper-zypp-plugin
%manifest %{name}.manifest
%defattr(-,root,root)
/usr/lib/zypp/plugins/commit/snapper.py*

