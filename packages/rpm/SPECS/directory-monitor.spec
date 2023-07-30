Name:           directory-monitor
Version:        0.0.1
Release:        1%{?dist}
Summary:        Package for process and daemon program from https://github.com/TCA166/reimagined-adventure
BuildArch:      x86_64

License:        none
Source0:        %{name}.tar.gz

Requires:       systemd

BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-buildroot

%description
Package for process and daemon program from https://github.com/TCA166/reimagined-adventure

%prep
%setup -q

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_bindir}
mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/systemd/system
cp %{name}-daemon $RPM_BUILD_ROOT/%{_bindir}
cp %{name}-process $RPM_BUILD_ROOT/%{_bindir}
cp %{name}.service $RPM_BUILD_ROOT/%{_sysconfdir}/systemd/system

%clean
rm -rf $RPM_BUILD_ROOT

%files
%{_bindir}/%{name}-daemon
%{_bindir}/%{name}-process
%{_sysconfdir}/systemd/system/%{name}.service

%changelog
