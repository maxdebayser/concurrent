Name:		concurrent
Version:	0.1
Release:	1%{?dist}
Summary:	An implementation of the disruptor pattern for C++

Group:		System Environment/Libraries
License:	MIT
URL:		http://github.com/maxdebayser/concurrent
Source0:	concurrent.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
#BuildRequires:	clang
BuildRequires:	gcc-c++

%global debug_package %{nil}

%description
This library contains a C++11 implementation of the 
disruptor pattern togheter with a few concurrency
tools necessary to implement it, but useful in other
contexts. It also contains higher level classes
based on the disruptor


%prep
%setup -q -n concurrent


%build
mkdir build
cd build
cmake ../ -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
cd build
make install DESTDIR=$RPM_BUILD_ROOT

%check
cd build/src/unit_test
./unit_test


%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc LICENSE README.md
%{_libdir}/*.a
%{_includedir}/concurrent/*.h
%{_includedir}/concurrent/disruptor/*.h

%changelog
* Tue May 17 2016 Maximilien de Bayser <maxdebayser@gmail.com> 0.1
- First version of this spec
