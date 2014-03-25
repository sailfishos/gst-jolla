Name:           gstreamer1.0-jolla
Summary:        Collection of Jolla specific GStreamer plugins
Version:        1.0.0
Release:        1
Group:          Applications/Multimedia
License:        Proprietary
URL:            http://jollamobile.com
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  pkgconfig(gstreamer-video-1.0)
BuildRequires:  pkgconfig(nemo-gstreamer-interfaces-1.0)
BuildRequires:  pkgconfig(android-headers)
BuildRequires:  pkgconfig(egl)
BuildRequires:  pkgconfig(libhardware)
BuildRequires:  pkgconfig(glesv2)

%description
Collection of Jolla specific GStreamer plugins

%package -n libgstreamer-1.0-anativewindowbuffer
Summary: gstreamer native buffer library
Group: Applications/Multimedia
%description -n libgstreamer-1.0-anativewindowbuffer
%{summary}
%post -n libgstreamer-1.0-anativewindowbuffer -p /sbin/ldconfig
%postun -n libgstreamer-1.0-anativewindowbuffer -p /sbin/ldconfig

%package -n libgstreamer-1.0-anativewindowbuffer-devel
Summary: gstreamer native buffer library devel package
Group: Applications/Multimedia
Requires: libgstreamer-1.0-anativewindowbuffer = %{version}-%{release}
%description -n libgstreamer-1.0-anativewindowbuffer-devel
%{summary}
%post -n libgstreamer-1.0-anativewindowbuffer-devel -p /sbin/ldconfig
%postun -n libgstreamer-1.0-anativewindowbuffer-devel -p /sbin/ldconfig

%package -n gstreamer-1.0-droideglsink
Summary: droideglsink HW accelerated sink
Group: Applications/Multimedia
%description -n gstreamer-1.0-droideglsink
%{summary}

%prep
%setup -q

%build
%autogen
%configure
make %{?jobs:-j%jobs}

%install 
%make_install

%files -n libgstreamer-1.0-anativewindowbuffer
%defattr(-,root,root,-)
%{_libdir}/libgstanativewindowbuffer-1.0.so.*

%files -n libgstreamer-1.0-anativewindowbuffer-devel
%defattr(-,root,root,-)
%{_libdir}/libgstanativewindowbuffer-1.0.so
%{_libdir}/pkgconfig/libgstanativewindowbuffer-1.0.pc
%{_includedir}/gstreamer-1.0/gst/anativewindowbuffer/gstanativewindowbufferpool.h


%files -n gstreamer-1.0-droideglsink
%defattr(-,root,root,-)
%{_libdir}/gstreamer-1.0/libgstdroideglsink.so
