#
# To build an RPM, install rpm-utils, hiredis-devel, php-devel, etc.
# Copy spec file to ~/rpmbuild/SPECS
# cd ~/rpmbuild/SOURCES
# wget -O phpiredis-master.zip https://github.com/nrk/phpiredis/archive/4404c86f4b556da26d4d15e3aa724b5b1c46b41a.zip
# cd ~/rpmbuild/SPECS
# rpmbuild -ba phpiredis.spec
#
# Currently, the version string is set at 0.0.1 since there's not an obvious version string in the README.  This
# shouldn't be taken to mean that the version is set at 0.0.1; only that RPM requires a version, and this is lower
# than anything that would come later if/when versioning is introduced.
#
# SPEC originally from the Fedora php-pecl-APC package; adapted by Israel Shirk <israelshirk@gmail.com>.

%{!?php_inidir:  %{expand: %%global php_inidir  %{_sysconfdir}/php.d}}
%{!?php_incldir: %{expand: %%global php_incldir %{_includedir}/php}}
%{!?__php:       %{expand: %%global __php       %{_bindir}/php}}
%{!?__pecl:      %{expand: %%global __pecl      %{_bindir}/pecl}}

%global pecl_name phpiredis
%global gitrev    4404c8
%global fullgitrev  4404c86f4b556da26d4d15e3aa724b5b1c46b41a

Summary:       Phpiredis wraps the hiredis library in a PHP extension to provide: a very simple but efficient client library for Redis; and a fast incremental parser for the Redis protocol
Name:          php-phpiredis
Version:       0.0.1
Release:       001_git_%{gitrev}_%{?dist}
License:       PHP
Group:         Development/Languages
URL:           https://github.com/nrk/phpiredis
# wget -O phpiredis-master.zip https://github.com/nrk/phpiredis/archive/4404c86f4b556da26d4d15e3aa724b5b1c46b41a.zip
# unzip phpiredis-master.zip
Source0:       https://github.com/nrk/phpiredis/archive/phpiredis-master.zip
Source1:       php-phpiredis.ini

BuildRequires: php-devel
BuildRequires: php-pear
BuildRequires: pcre-devel
BuildRequires: hiredis-devel
# Only for tests (used by some unit tests)
BuildRequires: php-dom

Requires(post): %{__pecl}
Requires(postun): %{__pecl}
Requires:      php(zend-abi) = %{php_zend_api}
Requires:      php(api) = %{php_core_api}
Requires:      hiredis

Provides:      php-phpiredis = %{version}
Provides:      php-phpiredis%{?_isa} = %{version}
Provides:      php-pecl(%{pecl_name}) = %{version}
Provides:      php-pecl(%{pecl_name})%{?_isa} = %{version}

# Filter private shared
%{?filter_provides_in: %filter_provides_in %{_libdir}/.*\.so$}
%{?filter_setup}


%description
Phpiredis wraps the hiredis library in a PHP extension to provide: a very simple but efficient client library for Redis; and a fast incremental parser for the Redis protocol.

# %package devel
# Summary:       APC developer files (header)
# Group:         Development/Libraries
# Requires:      php-pecl-phpiredis%{?_isa} = %{version}-%{release}
# Requires:      php-devel%{?_isa}
# 
# %description devel
# Phpiredis wraps the hiredis library in a PHP extension to provide: a very simple but efficient client library for Redis; and a fast incremental parser for the Redis protocol.


%prep
%setup -q -c


%build
cd phpiredis-%{fullgitrev}
%{_bindir}/phpize
%configure --with-php-config=%{_bindir}/php-config
make %{?_smp_mflags}

%install
cd phpiredis-%{fullgitrev}
pwd
make install INSTALL_ROOT=%{buildroot}

install -D -m 644 %{_sourcedir}/php-phpiredis.ini %{buildroot}%{php_inidir}/php-phpiredis.ini



%check
cd phpiredis-%{fullgitrev}
make tests


%files
# %doc APC-%{version}/{TECHNOTES.txt,CHANGELOG,LICENSE,INSTALL,TODO,phpiredis.php}
# %doc %{_sourcedir}/README.md
%config(noreplace) %{php_inidir}/php-phpiredis.ini
%{php_extdir}/phpiredis.so

# %files devel
# %{php_incldir}/ext/phpiredis.so

%changelog
* Tue Nov 26 2013 Israel Shirk <israelshirk@gmail.com> - 0.0.1-001.4404c86f4b556da26d4d15e3aa724b5b1c46b41a
- First build
