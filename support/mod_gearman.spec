%if ! %{defined _fillupdir}
%define _fillupdir %{_localstatedir}/adm/fillup-templates
%endif

Name:          mod_gearman
Version:       5.1.6
Release:       1%{?dist}
License:       GPL-2.0-or-later
Packager:      Sven Nierlein <sven.nierlein@consol.de>
Vendor:        Labs Consol
URL:           http://labs.consol.de/nagios/mod-gearman/
Source0:       mod_gearman-%{version}.tar.gz
Group:         System/Monitoring
Summary:       Mod-Gearman module for Naemon
Requires:      libgearman, perl, logrotate, openssl
BuildRequires: autoconf, automake, gcc-c++, pkgconfig, ncurses-devel
BuildRequires: libtool, libtool-ltdl-devel, libevent-devel, openssl-devel
BuildRequires: libgearman-devel
BuildRequires: naemon-devel >= 1.4.3
BuildRequires: perl
%if %{defined suse_version}
%else
BuildRequires: perl-devel, perl-ExtUtils-Embed, perl-Test-Simple, perl-Time-HiRes
%endif
BuildRequires: gearmand
BuildRequires: systemd
%{?systemd_requires}

%description
Mod Gearman is a new way of distributing active Naemon (and compatible cores)
checks across your network. It consists of two parts: There is a NEB module
which resides in the Naemon core and adds servicechecks, hostchecks and
eventhandler to a Gearman queue. There can be multiple equal gearman servers.
The counterpart is one or more worker clients for the checks itself. They can
be bound to host and servicegroups.

%prep
%setup -q

%build
test -f configure || ./autogen.sh
%configure \
     --datadir="%{_datadir}" \
     --datarootdir="%{_datadir}" \
     --localstatedir="%{_localstatedir}" \
     --sysconfdir="%{_sysconfdir}" \
     --with-init-dir="%{_initrddir}" \
     --enable-embedded-perl

%{__make} %{_smp_mflags}

%install
%{__rm} -rf %{buildroot}
%{__make} install \
     install-config \
     DESTDIR="%{buildroot}" \
     AM_INSTALL_PROGRAM_FLAGS=""

# Install systemd entry
%{__install} -D -m 0644 -p worker/daemon-systemd %{buildroot}%{_unitdir}/mod-gearman-worker.service

%if %{defined suse_version}
mkdir -p %{buildroot}%{_fillupdir}
touch %{buildroot}%{_fillupdir}/sysconfig.mod-gearman-worker
%else
mkdir -p %{buildroot}%{_sysconfdir}/sysconfig
touch %{buildroot}%{_sysconfdir}/sysconfig/mod-gearman-worker
%endif

# remove SystemV init-script
%{__rm} -f %{buildroot}%{_initrddir}/mod-gearman-worker



%pre
getent group naemon >/dev/null || groupadd -r naemon
getent passwd naemon >/dev/null || \
    useradd -r -g naemon -d %{_localstatedir}/mod_gearman -s /sbin/nologin \
    -c "naemon user" naemon
%if %{defined suse_version}
%service_add_pre mod-gearman-worker.service
%endif

%post
%if %{defined suse_version}
%service_add_post mod-gearman-worker.service
%if 0%{?suse_version} < 1230
%{fillup_and_insserv -y mod-gearman-worker}
%else
%fillup_only -n mod-gearman-worker
%endif
%else
%systemd_post mod-gearman-worker.service
%endif

%preun
%if %{defined suse_version}
%service_del_preun mod-gearman-worker.service
%else
%systemd_preun mod-gearman-worker.service
%endif

%postun
%if %{defined suse_version}
%service_del_postun mod-gearman-worker.service
%else
%systemd_postun mod-gearman-worker.service
%endif

%clean
%{__rm} -rf %{buildroot}

%files
%attr(0644,root,root) %{_unitdir}/mod-gearman-worker.service
%if %{defined suse_version}
%{_fillupdir}/sysconfig.mod-gearman-worker
%else
%config(noreplace) %{_sysconfdir}/sysconfig/mod-gearman-worker
%endif
%dir %{_sysconfdir}/mod_gearman
%config(noreplace) %{_sysconfdir}/mod_gearman/module.conf
%config(noreplace) %{_sysconfdir}/mod_gearman/worker.conf
%config(noreplace) %{_sysconfdir}/logrotate.d/mod_gearman

%dir %{_datadir}/mod_gearman
%{_datadir}/mod_gearman/mod_gearman_p1.pl

%{_bindir}/check_gearman
%{_bindir}/gearman_top
%{_bindir}/mod_gearman_worker
%{_bindir}/send_gearman
%{_bindir}/send_multi
%{_bindir}/mod_gearman_mini_epn

%dir %{_libdir}/mod_gearman
%{_libdir}/mod_gearman/mod_gearman_naemon.o

%attr(755,naemon,root) %{_localstatedir}/mod_gearman
%attr(755,naemon,root) %{_localstatedir}/log/mod_gearman

%defattr(-,root,root)
%docdir %{_defaultdocdir}

%changelog
* Mon Nov 18 2024 Sven Nierlein <sven@consol.de>
- set minimum naemon to 1.4.3

* Sat Jun 12 2021 Sven Nierlein <sven@consol.de>
- remove init.d
- build with upstream gearmand

* Wed Jan 29 2020 Sven Nierlein <sven@consol.de>
- removed gearman_proxy

* Wed Feb 17 2016 Sven Nierlein <sven@consol.de>
- prepare for mod-gearman 3

* Mon Sep 08 2014 Sven Nierlein <sven@consol.de>
- released mod-gearman 2.0

* Thu May 08 2014 Sven Nierlein <sven@consol.de>
- renamed to mod-gearman2

* Sun Feb 16 2014 Sven Nierlein <sven@consol.de>
- provides naemon mod-gearman now

* Thu Oct 31 2013 Sven Nierlein <sven@consol.de>
- added mini_epn

* Mon Nov 19 2012 Ricardo Maraschini <ricardo.maraschini@opservices.com.br>
- added logrotate configuration file

* Fri Apr 06 2012 Sven Nierlein <sven@consol.de>
- added gearman_proxy to package

* Thu Jan 19 2012 Sven Nierlein <sven@consol.de>
- enabled embedded Perl

* Mon Jun 06 2011 Michael Friedrich <michael.friedrich@univie.ac.at>
- reworked spec file to fit fhs compliance in /etc/mod_gearman
- moved extras/*conf from localestatedir to sysconfdir
- added config noreplace to config targets
- removed custom gearmand init script, interferes with gearmand dependency on rhel

* Fri Feb 11 2011 Sven Nierlein <sven@consol.de>
- Adapted spec file for SLES11

* Wed Oct 13 2010 Olivier Raginel <babar@cern.ch>
- First build, on Scientific Linux CERN 5.5
