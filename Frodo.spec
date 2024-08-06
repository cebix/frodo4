%define name Frodo
%define version 4.2
%define release 1

Summary: Commodore 64 emulator
Name: %{name}
Version: %{version}
Release: %{release}
Copyright: GPL
Group: Applications/Emulators
Source: %{name}-%{version}.tar.gz
URL: http://www.uni-mainz.de/~bauec002/FRMain.html
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
cd Src
CFLAGS=${RPM_OPT_FLAGS} CXXFLAGS=${RPM_OPT_FLAGS} ./configure --prefix=%{_prefix}
make

%install
rm -rf ${RPM_BUILD_ROOT}
cd Src
make DESTDIR=${RPM_BUILD_ROOT} install

%clean
rm -rf ${RPM_BUILD_ROOT}

%files
%defattr(-,root,root)
%doc COPYING CHANGES
%doc Docs/*.html
%{_bindir}/Frodo
%{_bindir}/FrodoSC
%{_bindir}/Frodo_GUI.tcl
"%{_datadir}/frodo/Kernal ROM"
%{_datadir}/frodo/Frodo.glade
