Name:          mod_gearman2
Version:       2.1.4
Release:       1%{?dist}
License:       GNU Public License version 2
Packager:      Sven Nierlein <sven.nierlein@consol.de>
Vendor:        Labs Consol
URL:           http://labs.consol.de/nagios/mod-gearman/
Source0:       mod_gearman2-%{version}.tar.gz
Group:         Applications/Monitoring
BuildRoot:     %{_tmppath}/%{name}-%{version}-root-%(%{__id_u} -n)
BuildRequires: autoconf, automake, ncurses-devel
BuildRequires: libtool, libtool-ltdl-devel, libevent-devel
BuildRequires: gearmand-devel
Summary:       Gearman module for Naemon
Requires(pre,post): /sbin/ldconfig
Requires:      gearmand, perl, logrotate
%if 0%{?el7}%{?fc20}%{?fc21}%{?fc22}
BuildRequires: systemd
%endif
%if 0%{?suse_version} < 1315
Requires(pre): shadow-utils
%endif

Provides:      mod_gearman2

%description
Mod Gearman is a new way of distributing active Naemon (and compatible cores)
checks across your network. It consists of two parts: There is a NEB module
which resides in the Naemon core and adds servicechecks, hostchecks and
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
%{__install} -D -m 0644 -p worker/daemon-systemd %{buildroot}%{_unitdir}/mod-gearman2-worker.service
mkdir -p %{buildroot}%{_sysconfdir}/sysconfig
echo "NICELEVEL=0" > %{buildroot}%{_sysconfdir}/sysconfig/mod-gearman2-worker
# remove SystemV init-script
%{__rm} -f %{buildroot}%{_initrddir}/mod-gearman2-worker
%endif



%pre
getent group naemon >/dev/null || groupadd -r naemon
getent passwd naemon >/dev/null || \
    useradd -r -g naemon -d %{_localstatedir}/mod_gearman2 -s /sbin/nologin \
    -c "naemon user" naemon
exit 0

%post
/sbin/ldconfig
%if 0%{?el7}%{?fc20}%{?fc21}%{?fc22}
# on first installation only
if [ $1 -eq 1 ] ; then
    systemctl enable mod-gearman2-worker
    systemctl start mod-gearman2-worker
else
    systemctl try-restart mod-gearman2-worker
fi
%else
if test $1 = 1; then
  /sbin/chkconfig --add mod-gearman2-worker
fi
%endif

%preun
%if 0%{?el7}%{?fc20}%{?fc21}%{?fc22}
systemctl disable mod-gearman2-worker
%else
if test $1 = 0; then
  /sbin/chkconfig --del mod-gearman2-worker
fi
%endif

%postun
/sbin/ldconfig

%clean
%{__rm} -rf %{buildroot}

%files
%if 0%{?el7}%{?fc20}%{?fc21}%{?fc22}
  %attr(0644,root,root) %{_unitdir}/mod-gearman2-worker.service
  %attr(0644,root,root) %{_sysconfdir}/sysconfig/mod-gearman2-worker
%else
  %attr(755,root,root) %{_initrddir}/mod-gearman2-worker
%endif

%config(noreplace) %{_sysconfdir}/mod_gearman2/module.conf
%config(noreplace) %{_sysconfdir}/mod_gearman2/worker.conf
%config(noreplace) %{_sysconfdir}/logrotate.d/mod_gearman2

%{_datadir}/mod_gearman2/standalone_worker.conf
%{_datadir}/mod_gearman2/shared.conf
%{_datadir}/mod_gearman2/mod_gearman_p1.pl
%{_datadir}/mod_gearman2/gearman_proxy.pl

%{_bindir}/check_gearman2
%{_bindir}/gearman_top2
%{_bindir}/mod_gearman2_worker
%{_bindir}/send_gearman2
%{_bindir}/send_multi2
%{_bindir}/mod_gearman2_mini_epn

%{_libdir}/mod_gearman2/mod_gearman2.o

%attr(755,naemon,root) %{_localstatedir}/mod_gearman2
%attr(755,naemon,root) %{_localstatedir}/log/mod_gearman2

%defattr(-,root,root)
%docdir %{_defaultdocdir}

%changelog
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
