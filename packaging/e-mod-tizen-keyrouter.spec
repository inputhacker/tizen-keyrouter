%bcond_with wayland

Name: e-mod-tizen-keyrouter
Version: 0.1.21
Release: 1
Summary: The Enlightenment Keyrouter Module for Tizen
URL: http://www.enlightenment.org
Group: Graphics & UI Framework/Other
Source0: %{name}-%{version}.tar.gz
License: BSD-2-Clause
BuildRequires: pkgconfig(enlightenment)
BuildRequires:  gettext
BuildRequires:  pkgconfig(ttrace)
%if %{with wayland}
BuildRequires:  pkgconfig(wayland-server)
BuildRequires:  pkgconfig(tizen-extension-server)
BuildRequires:  pkgconfig(cynara-client)
BuildRequires:  pkgconfig(cynara-creds-socket)
BuildRequires:  pkgconfig(capi-system-device)
%endif
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(libtzplatform-config)
%if "%{?profile}" == "common"
%else
BuildRequires:  xkb-tizen-data
%endif

%global TZ_SYS_RO_SHARE  %{?TZ_SYS_RO_SHARE:%TZ_SYS_RO_SHARE}%{!?TZ_SYS_RO_SHARE:/usr/share}

%description
This package is a the Enlightenment Keyrouter Module for Tizen.

%prep
%setup -q

%build

export GC_SECTIONS_FLAGS="-fdata-sections -ffunction-sections -Wl,--gc-sections"
export CFLAGS+=" -Wall -g -fPIC -rdynamic ${GC_SECTIONS_FLAGS} -DE_LOGGING=1 "
export LDFLAGS+=" -Wl,--hash-style=both -Wl,--as-needed -Wl,--rpath=/usr/lib"

%autogen
%if %{with wayland}
%configure --prefix=/usr \
           --enable-wayland-only \
           --enable-cynara \
           TZ_SYS_RO_APP=%{TZ_SYS_RO_APP}
%endif

make

%install
rm -rf %{buildroot}

# for license notification
mkdir -p %{buildroot}/%{TZ_SYS_RO_SHARE}/license
cp -a %{_builddir}/%{buildsubdir}/COPYING %{buildroot}/%{TZ_SYS_RO_SHARE}/license/%{name}

# for install combination key's configuration
mkdir -p %{buildroot}%{TZ_SYS_RO_APP}/keyrouter
cp -af config/key_combinations.ini %{buildroot}%{TZ_SYS_RO_APP}/keyrouter/

# install
make install DESTDIR=%{buildroot}

# clear useless textual files
find  %{buildroot}%{_libdir}/enlightenment/modules/%{name} -name *.la | xargs rm

%files
%defattr(-,root,root,-)
%{_libdir}/enlightenment/modules/e-mod-tizen-keyrouter
%{TZ_SYS_RO_SHARE}/license/%{name}
%attr(754, app, root) %{TZ_SYS_RO_APP}/keyrouter
