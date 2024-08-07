%define name Frodo
%define version 4.2
%define release 1

Summary: Commodore 64 emulator
Name: %{name}
Version: %{version}
Release: %{release}
License: GPLv2
Group: Applications/Emulators
Source: %{name}-%{version}.tar.gz
URL: https://frodo.cebix.net
BuildRoot: %{_tmppath}/%{name}-root
Prefix: %{_prefix}

%description
Frodo is a free, portable Commodore 64 emulator that runs on a variety
of platforms, with a focus on the exact reproduction of special graphical
effects possible on the C64.

Frodo comes in two flavours: The "normal" Frodo with a line-based
emulation, and the single-cycle emulation "Frodo SC" that is slower
but far more compatible.

%prep
%setup -q

%build
CFLAGS=${RPM_OPT_FLAGS} CXXFLAGS=${RPM_OPT_FLAGS} ./configure --prefix=%{_prefix}
make

%install
rm -rf ${RPM_BUILD_ROOT}
make DESTDIR=${RPM_BUILD_ROOT} install

%clean
rm -rf ${RPM_BUILD_ROOT}

%files
%defattr(-,root,root)
%doc CHANGES COPYING
%doc docs/*.html
%{_bindir}/Frodo
%{_bindir}/FrodoSC
%{_datadir}/Frodo/Frodo.glade
