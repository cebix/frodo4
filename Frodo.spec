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
BuildRequires: gcc-c++
BuildRequires: SDL2-devel >= 2.30
BuildRequires: gtk3-devel >= 3.24
BuildRequires: desktop-file-utils
BuildRoot: %{_tmppath}/%{name}-root
Prefix: %{_prefix}

%description
Frodo is a free, portable Commodore 64 emulator that runs on a variety
of platforms, with a focus on the exact reproduction of special graphical
effects possible on the C64.

Frodo comes in two flavours: "Frodo SC" which is a cycle-exact emulation,
and the original, line-based Frodo which is less compatible but runs better
on slower machines.

%prep
%setup -q

%build
CFLAGS=${RPM_OPT_FLAGS} CXXFLAGS=${RPM_OPT_FLAGS} ./configure --prefix=%{_prefix}
make

%install
rm -rf ${RPM_BUILD_ROOT}
make DESTDIR=${RPM_BUILD_ROOT} install

%post
update-desktop-database &> /dev/null || :

%postun
update-desktop-database &> /dev/null || :

%files
%defattr(-,root,root)
%doc CHANGES COPYING
%doc docs/*.html
%{_bindir}/Frodo
%{_bindir}/FrodoSC
%{_datadir}/Frodo/Frodo.ui
%{_datadir}/Frodo/Frodo_Logo.png
%{_datadir}/applications/Frodo.desktop
%{_datadir}/applications/FrodoSC.desktop
%{_datadir}/icons/hicolor/128x128/apps/Frodo.png
