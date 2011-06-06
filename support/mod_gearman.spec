Name:          mod_gearman
Version:       1.0.6
Release:       1%{?dist}
License:       GNU Public License version 2
Packager:      Michael Friedrich <michael.friedrich@univie.ac.at>
Vendor:        Icinga Team
URL:           http://labs.consol.de/nagios/mod-gearman/
Source0:        mod_gearman-%{version}.tar.gz
#Source1:       https://github.com/sni/mod_gearman/tarball/v%{version}
Group:         Applications/Monitoring
Requires:      gearmand
BuildRoot:     %{_tmppath}/%{name}-%{version}-root-%(%{__id_u} -n)
BuildRequires: autoconf, automake
BuildRequires: libtool, libevent-devel, ncurses-devel
Summary:       Gearman module for Icinga/Nagios
Requires(pre,post): /sbin/ldconfig

Provides:      mod_gearman

%description
Mod Gearman is a new way of distributing active Icinga/Nagios
checks across your network. It consists of two parts: There is
a NEB module which resides in the Icinga/Nagios core and adds
servicechecks, hostchecks and eventhandler to a Gearman queue.
There can be multiple equal gearman servers. The counterpart
is one or more worker clients for the checks itself. They can
be bound to host and servicegroups.

%prep
%setup -q
[ -f ./configure ] || ./autogen.sh

%build
%configure \
     --datadir="%{_datadir}" \
     --datarootdir="%{_datadir}" \
     --localstatedir="%{_localstatedir}" \
     --sysconfdir="%{_sysconfdir}/mod_gearman" \
     --with-init-dir="%{_initrddir}"

%{__make} %{_smp_mflags}

%install
%{__rm} -rf %{buildroot}
%{__make} install \
     install-config \
     DESTDIR="%{buildroot}" \
     AM_INSTALL_PROGRAM_FLAGS=""

#move shared config from datadir to sysconfdir
mv %{buildroot}/%{_datadir}/mod_gearman/shared.conf %{buildroot}/%{_sysconfdir}/mod_gearman/shared.conf
mv %{buildroot}/%{_datadir}/mod_gearman/standalone_worker.conf %{buildroot}/%{_sysconfdir}/mod_gearman/standalone_worker.conf

# remove custom gearmand initscript
%{__rm} -f %{buildroot}/%{_initrddir}/gearmand

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%clean
%{__rm} -rf %{buildroot}

%files
%attr(755,root,root) %{_initrddir}/mod_gearman_worker
%config(noreplace) %{_sysconfdir}/mod_gearman/mod_gearman.conf
%config(noreplace) %{_sysconfdir}/mod_gearman/shared.conf
%config(noreplace) %{_sysconfdir}/mod_gearman/standalone_worker.conf

%{_bindir}/check_gearman
%{_bindir}/gearman_top
%{_bindir}/mod_gearman_worker
%{_bindir}/send_gearman
%{_bindir}/send_multi

%{_libdir}/mod_gearman/mod_gearman.o
%{_libdir}/mod_gearman/mod_gearman.so

%attr(755,nagios,root) %{_localstatedir}/mod_gearman

%defattr(-,root,root)
%docdir %{_defaultdocdir}

%changelog
* Mon Jun 06 2011 Michael Friedrich <michael.friedrich@univie.ac.at>
- reworked spec file to fit fhs compliance in /etc/mod_gearman
- moved extras/*conf from localestatedir to sysconfdir
- added config noreplace to config targets
- removed custom gearmand init script, interferes with gearmand dependency on rhel

* Fri Feb 11 2011 Sven Nierlein <sven@consol.de>
- Adapted spec file for SLES11

* Wed Oct 13 2010 Olivier Raginel <babar@cern.ch>
- First build, on Scientific Linux CERN 5.5
