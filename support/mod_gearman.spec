Name:          mod_gearman
Version:       1.5.0
Release:       1%{?dist}
License:       GNU Public License version 2
Packager:      Sven Nierlein <sven.nierlein@consol.de>
Vendor:        Labs Consol
URL:           http://labs.consol.de/nagios/mod-gearman/
Source0:       mod_gearman-%{version}.tar.gz
Group:         Applications/Monitoring
BuildRoot:     %{_tmppath}/%{name}-%{version}-root-%(%{__id_u} -n)
BuildRequires: autoconf, automake, ncurses-devel
BuildRequires: libtool, libtool-ltdl-devel, libevent-devel
BuildRequires: gearmand-devel
Summary:       Gearman module for Nagios
Requires(pre,post): /sbin/ldconfig
Requires(pre): shadow-utils
Requires:      gearmand, perl, logrotate
%if 0%{?el7}%{?fc20}%{?fc21}%{?fc22}
BuildRequires: systemd
%endif

Provides:      mod_gearman

%description
Mod Gearman is a new way of distributing active Nagios (and compatible cores)
checks across your network. It consists of two parts: There is a NEB module
which resides in the Nagios core and adds servicechecks, hostchecks and
eventhandler to a Gearman queue. There can be multiple equal gearman servers.
The counterpart is one or more worker clients for the checks itself. They can
be bound to host and servicegroups.

%prep
%setup -q
[ -f ./configure ] || ./autogen.sh

%build
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

# remove custom gearmand initscript
%{__rm} -f %{buildroot}/%{_initrddir}/gearmand

%if 0%{?el7}%{?fc20}%{?fc21}%{?fc22}
# Install systemd entry
%{__install} -D -m 0644 -p worker/daemon-systemd %{buildroot}%{_unitdir}/mod_gearman_worker.service
mkdir -p %{buildroot}%{_sysconfdir}/sysconfig
touch %{buildroot}%{_sysconfdir}/sysconfig/mod-gearman-worker
# remove SystemV init-script
%{__rm} -f %{buildroot}%{_initrddir}/mod_gearman_worker
%endif



%pre
getent group nagios >/dev/null || groupadd -r nagios
getent passwd nagios >/dev/null || \
    useradd -r -g nagios -d %{_localstatedir}/mod_gearman -s /sbin/nologin \
    -c "nagios user" nagios
exit 0

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%clean
%{__rm} -rf %{buildroot}

%files
%if 0%{?el7}%{?fc20}%{?fc21}%{?fc22}
  %attr(0644,root,root) %{_unitdir}/mod_gearman_worker.service
  %attr(0644,root,root) %{_sysconfdir}/sysconfig/mod-gearman-worker
%else
  %attr(755,root,root) %{_initrddir}/mod_gearman_worker
%endif
%config(noreplace) %{_sysconfdir}/mod_gearman/mod_gearman_neb.conf
%config(noreplace) %{_sysconfdir}/mod_gearman/mod_gearman_worker.conf
%config(noreplace) %{_sysconfdir}/logrotate.d/mod_gearman

%{_datadir}/mod_gearman/standalone_worker.conf
%{_datadir}/mod_gearman/shared.conf
%{_datadir}/mod_gearman/mod_gearman_p1.pl
%{_datadir}/mod_gearman/gearman_proxy.pl

%{_bindir}/check_gearman
%{_bindir}/gearman_top
%{_bindir}/mod_gearman_worker
%{_bindir}/send_gearman
%{_bindir}/send_multi
%{_bindir}/mod_gearman_mini_epn

%{_libdir}/mod_gearman/mod_gearman.o

%attr(755,nagios,root) %{_localstatedir}/mod_gearman
%attr(755,nagios,root) %{_localstatedir}/log/mod_gearman

%defattr(-,root,root)
%docdir %{_defaultdocdir}

%changelog
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
