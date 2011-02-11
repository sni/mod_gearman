Name:          mod_gearman
Version:       1.0
Release:       3
License:       GNU Public License version 2
Packager:      Olivier Raginel <babar@cern.ch>
Vendor:        Icinga team
URL:           http://labs.consol.de/nagios/mod-gearman/
Prefix:        /opt/mod_gearman
Source:        http://labs.consol.de/wp-content/uploads/2010/09/mod_gearman-%{version}.tar.gz
Group:         Applications/Monitoring
Requires:      gearmand
BuildRoot:     %{_tmppath}/%{name}-%{version}-root-%(%{__id_u} -n)
BuildRequires: autoconf, automake
BuildRequires: libtool, libevent-devel, ncurses-devel
Summary:       Gearman module for Nagios / Icinga
Requires(pre,post): /sbin/ldconfig

Provides:      mod_gearman

%description
From the web page (http://labs.consol.de/nagios/mod-gearman/):

Mod Gearman is a new way of distributing active Nagios checks
across your network. It consists of two parts: There is a NEB
module which resides in the Nagios core and adds servicechecks,
hostchecks and eventhandler to a Gearman queue. There can be
multiple equal gearman servers. The counterpart is one or more
worker clients for the checks itself. They can be bound to host
and servicegroups.

%prep
%setup -q
[ -f ./configure ] || ./autogen.sh

%configure

%build
%{__make} %{_smp_mflags}

%install
%{__rm} -rf %{buildroot}
%{__make} install install-config DESTDIR="%{buildroot}" AM_INSTALL_PROGRAM_FLAGS=""

%clean
%{__rm} -rf %{buildroot}

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
%docdir %{_defaultdocdir}
%{_prefix}
%{_sysconfdir}
%defattr(-,nagios,root)
%{_localstatedir}

%changelog
* Fri Feb 11 2011 Sven Nierlein <sven@consol.de>
- Adapted spec file for SLES11

* Wed Oct 13 2010 Olivier Raginel <babar@cern.ch>
- First build, on Scientific Linux CERN 5.5
