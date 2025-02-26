#!/bin/bash -e

# helpers
pushd () { command pushd "$@" > /dev/null; }
popd () { command popd "$@" > /dev/null; }
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
CPUS=$( grep -c processor < /proc/cpuinfo )
cd "$DIR"

source "$DIR/../util.sh"
DISTRO="$(operating_system)"

for_arm=false
if [[ "$#" -eq 1 ]]; then
    if [[ "$1" == "--for-arm" ]]; then
        for_arm=true
    else
        echo "Invalid argument received. Use '--for-arm' if you want to build the toolchain for ARM based CPU."
        exit 1
   fi
fi

os="$1"

# toolchain version
TOOLCHAIN_VERSION=4

# package versions used
GCC_VERSION=11.2.0
BINUTILS_VERSION=2.37
case "$DISTRO" in
    centos-7) # because GDB >= 9 does NOT compile with readline6.
        GDB_VERSION=8.3
    ;;
    *)
        GDB_VERSION=11.2
    ;;
esac
CMAKE_VERSION=3.22.1
CPPCHECK_VERSION=2.6
LLVM_VERSION=13.0.0
SWIG_VERSION=4.0.2 # used only for LLVM compilation

# Check for the dependencies.
echo "ALL BUILD PACKAGES: $($DIR/../os/$DISTRO.sh list TOOLCHAIN_BUILD_DEPS)"
$DIR/../os/$DISTRO.sh check TOOLCHAIN_BUILD_DEPS
echo "ALL RUN PACKAGES: $($DIR/../os/$DISTRO.sh list TOOLCHAIN_RUN_DEPS)"
$DIR/../os/$DISTRO.sh check TOOLCHAIN_RUN_DEPS

# check installation directory
NAME=toolchain-v$TOOLCHAIN_VERSION
PREFIX=/opt/$NAME
mkdir -p $PREFIX >/dev/null 2>/dev/null || true
if [ ! -d $PREFIX ] || [ ! -w $PREFIX ]; then
    echo "Please make sure that the directory '$PREFIX' exists and is writable by the current user!"
    echo
    echo "If unsure, execute these commands as root:"
    echo "    mkdir $PREFIX && chown $USER:$USER $PREFIX"
    echo
    echo "Press <return> when you have created the directory and granted permissions."
    # wait for the directory to be created
    while true; do
        read
        if [ ! -d $PREFIX ] || [ ! -w $PREFIX ]; then
            echo
            echo "You can't continue before you have created the directory and granted permissions!"
            echo
            echo "Press <return> when you have created the directory and granted permissions."
        else
            break
        fi
    done
fi

# create archives directory
mkdir -p archives

# download all archives
pushd archives
if [ ! -f gcc-$GCC_VERSION.tar.gz ]; then
    wget https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz
fi
if [ ! -f binutils-$BINUTILS_VERSION.tar.gz ]; then
    wget https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.gz
fi
if [ ! -f gdb-$GDB_VERSION.tar.gz ]; then
    wget https://ftp.gnu.org/gnu/gdb/gdb-$GDB_VERSION.tar.gz
fi
if [ ! -f cmake-$CMAKE_VERSION.tar.gz ]; then
    wget https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION.tar.gz
fi
if [ ! -f cppcheck-$CPPCHECK_VERSION.tar.gz ]; then
    wget https://github.com/danmar/cppcheck/archive/$CPPCHECK_VERSION.tar.gz -O cppcheck-$CPPCHECK_VERSION.tar.gz
fi
if [ ! -f llvm-$LLVM_VERSION.src.tar.xz ]; then
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/llvm-$LLVM_VERSION.src.tar.xz
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/clang-$LLVM_VERSION.src.tar.xz
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/lld-$LLVM_VERSION.src.tar.xz
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/clang-tools-extra-$LLVM_VERSION.src.tar.xz
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/compiler-rt-$LLVM_VERSION.src.tar.xz
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/libunwind-$LLVM_VERSION.src.tar.xz
fi
if [ ! -f pahole-gdb-master.zip ]; then
    wget https://github.com/PhilArmstrong/pahole-gdb/archive/master.zip -O pahole-gdb-master.zip
fi
if [ ! -f swig-$SWIG_VERSION.tar.gz ]; then
    wget https://github.com/swig/swig/archive/rel-$SWIG_VERSION.tar.gz -O swig-$SWIG_VERSION.tar.gz
fi


# verify all archives
# NOTE: Verification can fail if the archive is signed by another developer. I
# haven't added commands to download all developer GnuPG keys because the
# download is very slow. If the verification fails for you, figure out who has
# signed the archive and download their public key instead.
GPG="gpg --homedir .gnupg"
KEYSERVER="hkp://keyserver.ubuntu.com"

mkdir -p .gnupg
chmod 700 .gnupg
# verify gcc
if [ ! -f gcc-$GCC_VERSION.tar.gz.sig ]; then
    wget https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz.sig
fi
# list of valid gcc gnupg keys: https://gcc.gnu.org/mirrors.html
$GPG --keyserver $KEYSERVER --recv-keys FC26A641
$GPG --verify gcc-$GCC_VERSION.tar.gz.sig gcc-$GCC_VERSION.tar.gz
# verify binutils
if [ ! -f binutils-$BINUTILS_VERSION.tar.gz.sig ]; then
    wget https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.gz.sig
fi
$GPG --keyserver $KEYSERVER --recv-keys 0xDD9E3C4F
$GPG --verify binutils-$BINUTILS_VERSION.tar.gz.sig binutils-$BINUTILS_VERSION.tar.gz
# verify gdb
if [ ! -f gdb-$GDB_VERSION.tar.gz.sig ]; then
    wget https://ftp.gnu.org/gnu/gdb/gdb-$GDB_VERSION.tar.gz.sig
fi
$GPG --keyserver $KEYSERVER --recv-keys 0xFF325CF3
$GPG --verify gdb-$GDB_VERSION.tar.gz.sig gdb-$GDB_VERSION.tar.gz
# verify cmake
if [ ! -f cmake-$CMAKE_VERSION-SHA-256.txt ] || [ ! -f cmake-$CMAKE_VERSION-SHA-256.txt.asc ]; then
    wget https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-SHA-256.txt
    wget https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-SHA-256.txt.asc
    # Because CentOS 7 doesn't have the `--ignore-missing` flag for `sha256sum`
    # we filter out the missing files from the sums here manually.
    cat cmake-$CMAKE_VERSION-SHA-256.txt | grep "cmake-$CMAKE_VERSION.tar.gz" > cmake-$CMAKE_VERSION-SHA-256-filtered.txt
fi
$GPG --keyserver $KEYSERVER --recv-keys 0xC6C265324BBEBDC350B513D02D2CEF1034921684
sha256sum -c cmake-$CMAKE_VERSION-SHA-256-filtered.txt
$GPG --verify cmake-$CMAKE_VERSION-SHA-256.txt.asc cmake-$CMAKE_VERSION-SHA-256.txt
# verify llvm, cfe, lld, clang-tools-extra
if [ ! -f llvm-$LLVM_VERSION.src.tar.xz.sig ]; then
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/llvm-$LLVM_VERSION.src.tar.xz.sig
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/clang-$LLVM_VERSION.src.tar.xz.sig
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/lld-$LLVM_VERSION.src.tar.xz.sig
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/clang-tools-extra-$LLVM_VERSION.src.tar.xz.sig
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/compiler-rt-$LLVM_VERSION.src.tar.xz.sig
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/libunwind-$LLVM_VERSION.src.tar.xz.sig
fi
# list of valid llvm gnupg keys: https://releases.llvm.org/download.html
$GPG --keyserver $KEYSERVER --recv-keys 0x474E22316ABF4785A88C6E8EA2C794A986419D8A
$GPG --verify llvm-$LLVM_VERSION.src.tar.xz.sig llvm-$LLVM_VERSION.src.tar.xz
$GPG --verify clang-$LLVM_VERSION.src.tar.xz.sig clang-$LLVM_VERSION.src.tar.xz
$GPG --verify lld-$LLVM_VERSION.src.tar.xz.sig lld-$LLVM_VERSION.src.tar.xz
$GPG --verify clang-tools-extra-$LLVM_VERSION.src.tar.xz.sig clang-tools-extra-$LLVM_VERSION.src.tar.xz
$GPG --verify compiler-rt-$LLVM_VERSION.src.tar.xz.sig compiler-rt-$LLVM_VERSION.src.tar.xz
$GPG --verify libunwind-$LLVM_VERSION.src.tar.xz.sig libunwind-$LLVM_VERSION.src.tar.xz

popd

# create build directory
mkdir -p build
pushd build

# compile gcc
if [ ! -f $PREFIX/bin/gcc ]; then
    if [ -d gcc-$GCC_VERSION ]; then
        rm -rf gcc-$GCC_VERSION
    fi
    tar -xvf ../archives/gcc-$GCC_VERSION.tar.gz
    pushd gcc-$GCC_VERSION
    ./contrib/download_prerequisites
    mkdir build && pushd build
    # influenced by: https://buildd.debian.org/status/fetch.php?pkg=gcc-11&arch=arm64&ver=11.2.0-14&stamp=1642052446&raw=0
    if [[ "$for_arm" = true ]]; then
        ../configure -v \
            --prefix=$PREFIX \
            --disable-multilib \
            --with-system-zlib \
            --enable-languages=c,c++,fortran \
            --enable-gold=yes \
            --enable-ld=yes \
            --disable-vtable-verify \
            --enable-libmpx \
            --without-cuda-driver \
            --enable-shared \
            --enable-linker-build-id \
            --without-included-gettext \
            --enable-threads=posix \
            --enable-nls \
            --enable-bootstrap \
            --enable-clocale=gnu \
            --enable-libstdcxx-debug \
            --enable-libstdcxx-time=yes \
            --with-default-libstdcxx-abi=new \
            --enable-gnu-unique-object \
            --disable-libquadmath \
            --disable-libquadmath-support \
            --enable-plugin \
            --enable-default-pie \
            --with-system-zlib \
            --enable-libphobos-checking=release \
            --with-target-system-zlib=auto \
            --enable-objc-gc=auto \
            --enable-multiarch \
            --enable-fix-cortex-a53-843419 \
            --disable-werror \
            --enable-checking=release \
            --build=aarch64-linux-gnu \
            --host=aarch64-linux-gnu \
            --target=aarch64-linux-gnu \
            --with-build-config=bootstrap-lto-lean \
            --enable-link-serialization=4
    else
        # influenced by: https://buildd.debian.org/status/fetch.php?pkg=gcc-8&arch=amd64&ver=8.3.0-6&stamp=1554588545
        ../configure -v \
            --build=x86_64-linux-gnu \
            --host=x86_64-linux-gnu \
            --target=x86_64-linux-gnu \
            --prefix=$PREFIX \
            --disable-multilib \
            --with-system-zlib \
            --enable-checking=release \
            --enable-languages=c,c++,fortran \
            --enable-gold=yes \
            --enable-ld=yes \
            --enable-lto \
            --enable-bootstrap \
            --disable-vtable-verify \
            --disable-werror \
            --without-included-gettext \
            --enable-threads=posix \
            --enable-nls \
            --enable-clocale=gnu \
            --enable-libstdcxx-debug \
            --enable-libstdcxx-time=yes \
            --enable-gnu-unique-object \
            --enable-libmpx \
            --enable-plugin \
            --enable-default-pie \
            --with-target-system-zlib \
            --with-tune=generic \
            --without-cuda-driver
            #--program-suffix=$( printf "$GCC_VERSION" | cut -d '.' -f 1,2 ) \
    fi
    make -j$CPUS
    # make -k check # run test suite
    make install
    popd && popd
fi

# activate toolchain
export PATH=$PREFIX/bin:$PATH
export LD_LIBRARY_PATH=$PREFIX/lib64

# compile binutils
if [ ! -f $PREFIX/bin/ld.gold ]; then
    if [ -d binutils-$BINUTILS_VERSION ]; then
        rm -rf binutils-$BINUTILS_VERSION
    fi
    tar -xvf ../archives/binutils-$BINUTILS_VERSION.tar.gz
    pushd binutils-$BINUTILS_VERSION
    mkdir build && pushd build
    if [[ "$for_arm" = true ]]; then
        # influenced by: https://buildd.debian.org/status/fetch.php?pkg=binutils&arch=arm64&ver=2.37.90.20220130-2&stamp=1643576183&raw=0
        env \
            CC=gcc \
            CXX=g++ \
            CFLAGS="-g -O2" \
            CXXFLAGS="-g -O2" \
            LDFLAGS="" \
            ../configure \
                --build=aarch64-linux-gnu \
                --host=aarch64-linux-gnu \
                --prefix=$PREFIX \
                --enable-ld=default \
                --enable-gold \
                --enable-lto \
                --enable-pgo-build=lto \
                --enable-plugins \
                --enable-shared \
                --enable-threads \
                --with-system-zlib \
                --enable-deterministic-archives \
                --disable-compressed-debug-sections \
                --disable-x86-used-note \
                --enable-obsolete \
                --enable-new-dtags \
                --disable-werror
    else
        # influenced by: https://buildd.debian.org/status/fetch.php?pkg=binutils&arch=amd64&ver=2.32-7&stamp=1553247092
        env \
            CC=gcc \
            CXX=g++ \
            CFLAGS="-g -O2" \
            CXXFLAGS="-g -O2" \
            LDFLAGS="" \
            ../configure \
                --build=x86_64-linux-gnu \
                --host=x86_64-linux-gnu \
                --prefix=$PREFIX \
                --enable-ld=default \
                --enable-gold \
                --enable-lto \
                --enable-plugins \
                --enable-shared \
                --enable-threads \
                --with-system-zlib \
                --enable-deterministic-archives \
                --disable-compressed-debug-sections \
                --enable-new-dtags \
                --disable-werror
    fi
    make -j$CPUS
    # make -k check # run test suite
    make install
    popd && popd
fi

# compile gdb
if [ ! -f $PREFIX/bin/gdb ]; then
    if [ -d gdb-$GDB_VERSION ]; then
        rm -rf gdb-$GDB_VERSION
    fi
    tar -xvf ../archives/gdb-$GDB_VERSION.tar.gz
    pushd gdb-$GDB_VERSION
    mkdir build && pushd build
    if [[ "$for_arm" = true ]]; then
        # https://buildd.debian.org/status/fetch.php?pkg=gdb&arch=arm64&ver=10.1-2&stamp=1614889767&raw=0
        env \
            CC=gcc \
            CXX=g++ \
            CFLAGS="-g -O2 -fstack-protector-strong -Wformat -Werror=format-security" \
            CXXFLAGS="-g -O2 -fstack-protector-strong -Wformat -Werror=format-security" \
            CPPFLAGS="-Wdate-time -D_FORTIFY_SOURCE=2 -fPIC" \
            LDFLAGS="-Wl,-z,relro" \
            PYTHON="" \
            ../configure \
                --build=aarch64-linux-gnu \
                --host=aarch64-linux-gnu \
                --prefix=$PREFIX \
                --disable-maintainer-mode \
                --disable-dependency-tracking \
                --disable-silent-rules \
                --disable-gdbtk \
                --disable-shared \
                --without-guile \
                --with-system-gdbinit=$PREFIX/etc/gdb/gdbinit \
                --with-system-readline \
                --with-expat \
                --with-system-zlib \
                --with-lzma \
                --without-babeltrace \
                --enable-tui \
                --with-python=python3
    else
        # https://buildd.debian.org/status/fetch.php?pkg=gdb&arch=amd64&ver=8.2.1-2&stamp=1550831554&raw=0
        env \
            CC=gcc \
            CXX=g++ \
            CFLAGS="-g -O2 -fstack-protector-strong -Wformat -Werror=format-security" \
            CXXFLAGS="-g -O2 -fstack-protector-strong -Wformat -Werror=format-security" \
            CPPFLAGS="-Wdate-time -D_FORTIFY_SOURCE=2 -fPIC" \
            LDFLAGS="-Wl,-z,relro" \
            PYTHON="" \
            ../configure \
                --build=x86_64-linux-gnu \
                --host=x86_64-linux-gnu \
                --prefix=$PREFIX \
                --disable-maintainer-mode \
                --disable-dependency-tracking \
                --disable-silent-rules \
                --disable-gdbtk \
                --disable-shared \
                --without-guile \
                --with-system-gdbinit=$PREFIX/etc/gdb/gdbinit \
                --with-system-readline \
                --with-expat \
                --with-system-zlib \
                --with-lzma \
                --with-babeltrace \
                --with-intel-pt \
                --enable-tui \
                --with-python=python3
    fi
    make -j$CPUS
    make install
    popd && popd
fi

# install pahole
if [ ! -d $PREFIX/share/pahole-gdb ]; then
    unzip ../archives/pahole-gdb-master.zip
    mv pahole-gdb-master $PREFIX/share/pahole-gdb
fi

# setup system gdbinit
if [ ! -f $PREFIX/etc/gdb/gdbinit ]; then
    mkdir -p $PREFIX/etc/gdb
    cat >$PREFIX/etc/gdb/gdbinit <<EOF
# improve formatting
set print pretty on
set print object on
set print static-members on
set print vtbl on
set print demangle on
set demangle-style gnu-v3
set print sevenbit-strings off

# load libstdc++ pretty printers
add-auto-load-scripts-directory $PREFIX/lib64
add-auto-load-safe-path $PREFIX

# load pahole
python
sys.path.insert(0, "$PREFIX/share/pahole-gdb")
import offsets
import pahole
end
EOF
fi

# compile cmake
if [ ! -f $PREFIX/bin/cmake ]; then
    if [ -d cmake-$CMAKE_VERSION ]; then
        rm -rf cmake-$CMAKE_VERSION
    fi
    tar -xvf ../archives/cmake-$CMAKE_VERSION.tar.gz
    pushd cmake-$CMAKE_VERSION
    # influenced by: https://buildd.debian.org/status/fetch.php?pkg=cmake&arch=amd64&ver=3.13.4-1&stamp=1549799837
    echo 'set(CMAKE_SKIP_RPATH ON CACHE BOOL "Skip rpath" FORCE)' >> build-flags.cmake
    echo 'set(CMAKE_USE_RELATIVE_PATHS ON CACHE BOOL "Use relative paths" FORCE)' >> build-flags.cmake
    echo 'set(CMAKE_C_FLAGS "-g -O2 -fstack-protector-strong -Wformat -Werror=format-security -Wdate-time -D_FORTIFY_SOURCE=2" CACHE STRING "C flags" FORCE)' >> build-flags.cmake
    echo 'set(CMAKE_CXX_FLAGS "-g -O2 -fstack-protector-strong -Wformat -Werror=format-security -Wdate-time -D_FORTIFY_SOURCE=2" CACHE STRING "C++ flags" FORCE)' >> build-flags.cmake
    echo 'set(CMAKE_SKIP_BOOTSTRAP_TEST ON CACHE BOOL "Skip BootstrapTest" FORCE)' >> build-flags.cmake
    echo 'set(BUILD_CursesDialog ON CACHE BOOL "Build curses GUI" FORCE)' >> build-flags.cmake
    mkdir build && pushd build
    ../bootstrap \
        --prefix=$PREFIX \
        --init=../build-flags.cmake \
        --parallel=$CPUS \
        --system-curl
    make -j$CPUS
    # make test # run test suite
    make install
    popd && popd
fi

# compile cppcheck
if [ ! -f $PREFIX/bin/cppcheck ]; then
    if [ -d cppcheck-$CPPCHECK_VERSION ]; then
        rm -rf cppcheck-$CPPCHECK_VERSION
    fi
    tar -xvf ../archives/cppcheck-$CPPCHECK_VERSION.tar.gz
    pushd cppcheck-$CPPCHECK_VERSION
    env \
        CC=gcc \
        CXX=g++ \
        PREFIX=$PREFIX \
        FILESDIR=$PREFIX/share/cppcheck \
        CFGDIR=$PREFIX/share/cppcheck/cfg \
            make -j$CPUS
    env \
        CC=gcc \
        CXX=g++ \
        PREFIX=$PREFIX \
        FILESDIR=$PREFIX/share/cppcheck \
        CFGDIR=$PREFIX/share/cppcheck/cfg \
            make install
    popd
fi

# compile swig
if [ ! -d swig-$SWIG_VERSION/install ]; then
    if [ -d swig-$SWIG_VERSION ]; then
        rm -rf swig-$SWIG_VERSION
    fi
    tar -xvf ../archives/swig-$SWIG_VERSION.tar.gz
    mv swig-rel-$SWIG_VERSION swig-$SWIG_VERSION
    pushd swig-$SWIG_VERSION
    ./autogen.sh
    mkdir build && pushd build
    ../configure --prefix=$DIR/build/swig-$SWIG_VERSION/install
    make -j$CPUS
    make install
    popd && popd
fi

# compile llvm
if [ ! -f $PREFIX/bin/clang ]; then
    if [ -d llvm-$LLVM_VERSION ]; then
        rm -rf llvm-$LLVM_VERSION
    fi
    tar -xvf ../archives/llvm-$LLVM_VERSION.src.tar.xz
    mv llvm-$LLVM_VERSION.src llvm-$LLVM_VERSION
    tar -xvf ../archives/clang-$LLVM_VERSION.src.tar.xz
    mv clang-$LLVM_VERSION.src llvm-$LLVM_VERSION/tools/clang
    tar -xvf ../archives/lld-$LLVM_VERSION.src.tar.xz
    mv lld-$LLVM_VERSION.src/ llvm-$LLVM_VERSION/tools/lld
    tar -xvf ../archives/clang-tools-extra-$LLVM_VERSION.src.tar.xz
    mv clang-tools-extra-$LLVM_VERSION.src/ llvm-$LLVM_VERSION/tools/clang/tools/extra
    tar -xvf ../archives/compiler-rt-$LLVM_VERSION.src.tar.xz
    mv compiler-rt-$LLVM_VERSION.src/ llvm-$LLVM_VERSION/projects/compiler-rt
    tar -xvf ../archives/libunwind-$LLVM_VERSION.src.tar.xz
    mv libunwind-$LLVM_VERSION.src/include/mach-o llvm-$LLVM_VERSION/tools/lld/include
    pushd llvm-$LLVM_VERSION
    mkdir build && pushd build
    # activate swig
    export PATH=$DIR/build/swig-$SWIG_VERSION/install/bin:$PATH
    # influenced by: https://buildd.debian.org/status/fetch.php?pkg=llvm-toolchain-7&arch=amd64&ver=1%3A7.0.1%7E%2Brc2-1%7Eexp1&stamp=1541506173&raw=0
    cmake .. \
        -DCMAKE_C_COMPILER=$PREFIX/bin/gcc \
        -DCMAKE_CXX_COMPILER=$PREFIX/bin/g++ \
        -DCMAKE_CXX_LINK_FLAGS="-L$PREFIX/lib64 -Wl,-rpath,$PREFIX/lib64" \
        -DCMAKE_INSTALL_PREFIX=$PREFIX \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O2 -DNDEBUG" \
        -DCMAKE_CXX_FLAGS=' -fuse-ld=gold -fPIC -Wno-unused-command-line-argument -Wno-unknown-warning-option' \
        -DCMAKE_C_FLAGS=' -fuse-ld=gold -fPIC -Wno-unused-command-line-argument -Wno-unknown-warning-option' \
        -DLLVM_LINK_LLVM_DYLIB=ON \
        -DLLVM_INSTALL_UTILS=ON \
        -DLLVM_VERSION_SUFFIX= \
        -DLLVM_BUILD_LLVM_DYLIB=ON \
        -DLLVM_ENABLE_RTTI=ON \
        -DLLVM_ENABLE_FFI=ON \
        -DLLVM_BINUTILS_INCDIR=$PREFIX/include/ \
        -DLLVM_USE_PERF=yes
    make -j$CPUS
    if [[ "$for_arm" = false ]]; then
        make -j$CPUS check-clang # run clang test suite
        make -j$CPUS check-lld # run lld test suite
    fi
    make install
    popd && popd
fi

popd

# create README
if [ ! -f $PREFIX/README.md ]; then
    cat >$PREFIX/README.md <<EOF
# Memgraph Toolchain v$TOOLCHAIN_VERSION

## Included tools

 - GCC $GCC_VERSION
 - Binutils $BINUTILS_VERSION
 - GDB $GDB_VERSION
 - CMake $CMAKE_VERSION
 - Cppcheck $CPPCHECK_VERSION
 - LLVM (Clang, LLD, compiler-rt, Clang tools extra) $LLVM_VERSION

## Required libraries

In order to be able to run all of these tools you should install the following
packages:

\`\`\`
$($DIR/../os/$DISTRO.sh list TOOLCHAIN_RUN_DEPS)
\`\`\`

## Usage

In order to use the toolchain you just have to source the activation script:

\`\`\`
source $PREFIX/activate
\`\`\`
EOF
fi

# create activation script
if [ ! -f $PREFIX/activate ]; then
    cat >$PREFIX/activate <<EOF
# This file must be used with "source $PREFIX/activate" *from bash*
# You can't run it directly!

env_error="You already have an active virtual environment!"

# zsh does not recognize the option -t of the command type
# therefore we use the alternative whence -w
if [[ "\$ZSH_NAME" == "zsh" ]]; then
    # check for active virtual environments
    if [ "\$( whence -w deactivate )" != "deactivate: none" ]; then
        echo \$env_error
        return 0;
    fi
# any other shell
else
    # check for active virtual environments
    if [ "\$( type -t deactivate )" != "" ]; then
        echo \$env_error
        return 0
    fi
fi

# check that we aren't root
if [[ "\$USER" == "root" ]]; then
    echo "You shouldn't use the toolchain as root!"
    return 0
fi

# save original environment
export ORIG_PATH=\$PATH
export ORIG_PS1=\$PS1
export ORIG_LD_LIBRARY_PATH=\$LD_LIBRARY_PATH
export ORIG_CXXFLAGS=\$CXXFLAGS
export ORIG_CFLAGS=\$CFLAGS

# activate new environment
export PATH=$PREFIX:$PREFIX/bin:\$PATH
export PS1="($NAME) \$PS1"
export LD_LIBRARY_PATH=$PREFIX/lib:$PREFIX/lib64
export CXXFLAGS=-isystem\ $PREFIX/include\ \$CXXFLAGS
export CFLAGS=-isystem\ $PREFIX/include\ \$CFLAGS

# disable root
function su () {
    echo "You don't want to use root functions while using the toolchain!"
    return 1
}
function sudo () {
    echo "You don't want to use root functions while using the toolchain!"
    return 1
}

# create deactivation function
function deactivate() {
    export PATH=\$ORIG_PATH
    export PS1=\$ORIG_PS1
    export LD_LIBRARY_PATH=\$ORIG_LD_LIBRARY_PATH
    export CXXFLAGS=\$ORIG_CXXFLAGS
    export CFLAGS=\$ORIG_CFLAGS
    unset ORIG_PATH ORIG_PS1 ORIG_LD_LIBRARY_PATH ORIG_CXXFLAGS ORIG_CFLAGS
    unset -f su sudo deactivate
}
EOF
fi

BOOST_SHA256=94ced8b72956591c4775ae2207a9763d3600b30d9d7446562c552f0a14a63be7
BOOST_VERSION=1.78.0
BOOST_VERSION_UNDERSCORES=`echo "${BOOST_VERSION//./_}"`
BZIP2_SHA256=a2848f34fcd5d6cf47def00461fcb528a0484d8edef8208d6d2e2909dc61d9cd
BZIP2_VERSION=1.0.6
DOUBLE_CONVERSION_SHA256=8a79e87d02ce1333c9d6c5e47f452596442a343d8c3e9b234e8a62fce1b1d49c
DOUBLE_CONVERSION_VERSION=3.1.6
FBLIBS_VERSION=2022.01.31.00
FIZZ_SHA256=32a60e78d41ea2682ce7e5d741b964f0ea83642656e42d4fea90c0936d6d0c7d
FLEX_VERSION=2.6.4
FMT_SHA256=b06ca3130158c625848f3fb7418f235155a4d389b2abc3a6245fb01cb0eb1e01
FMT_VERSION=8.0.1
FOLLY_SHA256=7b8d5dd2eb51757858247af0ad27af2e3e93823f84033a628722b01e06cd68a9
GFLAGS_COMMIT_HASH=b37ceb03a0e56c9f15ce80409438a555f8a67b7c
GLOG_SHA256=eede71f28371bf39aa69b45de23b329d37214016e2055269b3b5e7cfd40b59f5
GLOG_VERSION=0.5.0
JEMALLOC_COMMIT_HASH=ea6b3e973b477b8061e0076bb257dbd7f3faa756
LIBAIO_VERSION=0.3.112
LIBEVENT_VERSION=2.1.12-stable
LIBSODIUM_VERSION=1.0.18
LIBUNWIND_VERSION=1.6.2
LZ4_SHA256=33af5936ac06536805f9745e0b6d61da606a1f8b4cc5c04dd3cbaca3b9b4fc43
LZ4_VERSION=1.8.3
PROXYGEN_SHA256=5360a8ccdfb2f5a6c7b3eed331ec7ab0e2c792d579c6fff499c85c516c11fe14
SNAPPY_SHA256=75c1fbb3d618dd3a0483bff0e26d0a92b495bbe5059c8b4f1c962b478b6e06e7
SNAPPY_VERSION=1.1.9
XZ_VERSION=5.2.5 # for LZMA
ZLIB_VERSION=1.2.11
ZSTD_VERSION=1.5.0
WANGLE_SHA256=1002e9c32b6f4837f6a760016e3b3e22f3509880ef3eaad191c80dc92655f23f

pushd archives

if [ ! -f boost_$BOOST_VERSION_UNDERSCORES.tar.gz ]; then
    # do not redirect the download into a file, because it will download the file into a ".1" postfixed file
    # I am not sure why this is happening, but I think because of some redirects that happens during the download
    wget https://boostorg.jfrog.io/artifactory/main/release/$BOOST_VERSION/source/boost_$BOOST_VERSION_UNDERSCORES.tar.gz -O boost_$BOOST_VERSION_UNDERSCORES.tar.gz
fi
if [ ! -f bzip2-$BZIP2_VERSION.tar.gz ]; then
    wget https://sourceforge.net/projects/bzip2/files/bzip2-$BZIP2_VERSION.tar.gz -O bzip2-$BZIP2_VERSION.tar.gz
fi
if [ ! -f double-conversion-$DOUBLE_CONVERSION_VERSION.tar.gz ]; then
    wget https://github.com/google/double-conversion/archive/refs/tags/v$DOUBLE_CONVERSION_VERSION.tar.gz -O double-conversion-$DOUBLE_CONVERSION_VERSION.tar.gz
fi
if [ ! -f fizz-$FBLIBS_VERSION.tar.gz ]; then
    wget https://github.com/facebookincubator/fizz/releases/download/v$FBLIBS_VERSION/fizz-v$FBLIBS_VERSION.tar.gz -O fizz-$FBLIBS_VERSION.tar.gz
fi
if [ ! -f flex-$FLEX_VERSION.tar.gz ]; then
    wget https://github.com/westes/flex/releases/download/v$FLEX_VERSION/flex-$FLEX_VERSION.tar.gz -O flex-$FLEX_VERSION.tar.gz
fi
if [ ! -f fmt-$FMT_VERSION.tar.gz ]; then
    wget https://github.com/fmtlib/fmt/archive/refs/tags/$FMT_VERSION.tar.gz -O fmt-$FMT_VERSION.tar.gz
fi
if [ ! -f folly-$FBLIBS_VERSION.tar.gz ]; then
    wget https://github.com/facebook/folly/releases/download/v$FBLIBS_VERSION/folly-v$FBLIBS_VERSION.tar.gz -O folly-$FBLIBS_VERSION.tar.gz
fi
if [ ! -f glog-$GLOG_VERSION.tar.gz ]; then
    wget https://github.com/google/glog/archive/refs/tags/v$GLOG_VERSION.tar.gz -O glog-$GLOG_VERSION.tar.gz
fi
if [ ! -f libaio-$LIBAIO_VERSION.tar.gz ]; then
    wget https://releases.pagure.org/libaio/libaio-$LIBAIO_VERSION.tar.gz -O libaio-$LIBAIO_VERSION.tar.gz
fi
if [ ! -f libevent-$LIBEVENT_VERSION.tar.gz ]; then
    wget https://github.com/libevent/libevent/releases/download/release-$LIBEVENT_VERSION/libevent-$LIBEVENT_VERSION.tar.gz -O libevent-$LIBEVENT_VERSION.tar.gz
fi
if [ ! -f libsodium-$LIBSODIUM_VERSION.tar.gz ]; then
    curl https://download.libsodium.org/libsodium/releases/libsodium-$LIBSODIUM_VERSION.tar.gz -o libsodium-$LIBSODIUM_VERSION.tar.gz
fi
if [ ! -f libunwind-$LIBUNWIND_VERSION.tar.gz ]; then
    wget https://github.com/libunwind/libunwind/releases/download/v$LIBUNWIND_VERSION/libunwind-$LIBUNWIND_VERSION.tar.gz -O libunwind-$LIBUNWIND_VERSION.tar.gz
fi
if [ ! -f lz4-$LZ4_VERSION.tar.gz ]; then
    wget https://github.com/lz4/lz4/archive/v$LZ4_VERSION.tar.gz -O lz4-$LZ4_VERSION.tar.gz
fi
if [ ! -f proxygen-$FBLIBS_VERSION.tar.gz ]; then
    wget https://github.com/facebook/proxygen/releases/download/v$FBLIBS_VERSION/proxygen-v$FBLIBS_VERSION.tar.gz -O proxygen-$FBLIBS_VERSION.tar.gz
fi
if [ ! -f snappy-$SNAPPY_VERSION.tar.gz ]; then
    wget https://github.com/google/snappy/archive/refs/tags/$SNAPPY_VERSION.tar.gz -O snappy-$SNAPPY_VERSION.tar.gz
fi
if [ ! -f xz-$XZ_VERSION.tar.gz ]; then
    wget https://tukaani.org/xz/xz-$XZ_VERSION.tar.gz -O xz-$XZ_VERSION.tar.gz
fi
if [ ! -f zlib-$ZLIB_VERSION.tar.gz ]; then
    wget https://zlib.net/zlib-$ZLIB_VERSION.tar.gz -O zlib-$ZLIB_VERSION.tar.gz
fi
if [ ! -f zstd-$ZSTD_VERSION.tar.gz ]; then
    wget https://github.com/facebook/zstd/releases/download/v$ZSTD_VERSION/zstd-$ZSTD_VERSION.tar.gz -O zstd-$ZSTD_VERSION.tar.gz
fi
if [ ! -f wangle-$FBLIBS_VERSION.tar.gz ]; then
    wget https://github.com/facebook/wangle/releases/download/v$FBLIBS_VERSION/wangle-v$FBLIBS_VERSION.tar.gz -O wangle-$FBLIBS_VERSION.tar.gz
fi

# verify boost
echo "$BOOST_SHA256 boost_$BOOST_VERSION_UNDERSCORES.tar.gz" | sha256sum -c
# verify bzip2
echo "$BZIP2_SHA256 bzip2-$BZIP2_VERSION.tar.gz" | sha256sum -c
# verify double-conversion
echo "$DOUBLE_CONVERSION_SHA256 double-conversion-$DOUBLE_CONVERSION_VERSION.tar.gz" | sha256sum -c
# verify fizz
echo "$FIZZ_SHA256 fizz-$FBLIBS_VERSION.tar.gz" | sha256sum -c
# verify flex
if [ ! -f flex-$FLEX_VERSION.tar.gz.sig ]; then
    wget https://github.com/westes/flex/releases/download/v$FLEX_VERSION/flex-$FLEX_VERSION.tar.gz.sig
fi
$GPG --keyserver $KEYSERVER --recv-keys 0xE4B29C8D64885307
$GPG --verify flex-$FLEX_VERSION.tar.gz.sig flex-$FLEX_VERSION.tar.gz
# verify fmt
echo "$FMT_SHA256 fmt-$FMT_VERSION.tar.gz" | sha256sum -c
# verify folly
echo "$FOLLY_SHA256 folly-$FBLIBS_VERSION.tar.gz" | sha256sum -c
# verify glog
echo "$GLOG_SHA256  glog-$GLOG_VERSION.tar.gz" | sha256sum -c
# verify libaio
if [ ! -f libaio-CHECKSUMS ]; then
    wget https://releases.pagure.org/libaio/CHECKSUMS -O libaio-CHECKSUMS
fi
cat libaio-CHECKSUMS | grep "SHA256 (libaio-$LIBAIO_VERSION.tar.gz)" | sha256sum -c
# verify libevent
if [ ! -f libevent-$LIBEVENT_VERSION.tar.gz.asc ]; then
    wget https://github.com/libevent/libevent/releases/download/release-$LIBEVENT_VERSION/libevent-$LIBEVENT_VERSION.tar.gz.asc
fi
$GPG --keyserver $KEYSERVER --recv-keys 0x9E3AC83A27974B84D1B3401DB86086848EF8686D
$GPG --verify libevent-$LIBEVENT_VERSION.tar.gz.asc libevent-$LIBEVENT_VERSION.tar.gz
# verify libsodium
if [ ! -f libsodium-$LIBSODIUM_VERSION.tar.gz.sig ]; then
    curl https://download.libsodium.org/libsodium/releases/libsodium-$LIBSODIUM_VERSION.tar.gz.sig -o libsodium-$LIBSODIUM_VERSION.tar.gz.sig
fi
$GPG --keyserver $KEYSERVER --recv-keys 0x0C7983A8FD9A104C623172CB62F25B592B6F76DA
$GPG --verify libsodium-$LIBSODIUM_VERSION.tar.gz.sig libsodium-$LIBSODIUM_VERSION.tar.gz
# verify libunwind
if [ ! -f libunwind-$LIBUNWIND_VERSION.tar.gz.sig ]; then
    wget https://github.com/libunwind/libunwind/releases/download/v$LIBUNWIND_VERSION/libunwind-$LIBUNWIND_VERSION.tar.gz.sig
fi
$GPG --keyserver $KEYSERVER --recv-keys 0x75D2CFC56CC2E935A4143297015A268A17D55FA4
$GPG --verify libunwind-$LIBUNWIND_VERSION.tar.gz.sig libunwind-$LIBUNWIND_VERSION.tar.gz
# verify lz4
echo "$LZ4_SHA256  lz4-$LZ4_VERSION.tar.gz" | sha256sum -c
# verify proxygen
echo "$PROXYGEN_SHA256 proxygen-$FBLIBS_VERSION.tar.gz" | sha256sum -c
# verify snappy
echo "$SNAPPY_SHA256  snappy-$SNAPPY_VERSION.tar.gz" | sha256sum -c
# verify xz
if [ ! -f xz-$XZ_VERSION.tar.gz.sig ]; then
    wget https://tukaani.org/xz/xz-$XZ_VERSION.tar.gz.sig
fi
$GPG --import ../xz_pgp.txt
$GPG --verify xz-$XZ_VERSION.tar.gz.sig xz-$XZ_VERSION.tar.gz
# verify zlib
if [ ! -f zlib-$ZLIB_VERSION.tar.gz.asc ]; then
    wget https://zlib.net/zlib-$ZLIB_VERSION.tar.gz.asc
fi
$GPG --keyserver $KEYSERVER --recv-keys 0x783FCD8E58BCAFBA
$GPG --verify zlib-$ZLIB_VERSION.tar.gz.asc zlib-$ZLIB_VERSION.tar.gz
#verify zstd
if [ ! -f zstd-$ZSTD_VERSION.tar.gz.sig ]; then
    wget https://github.com/facebook/zstd/releases/download/v$ZSTD_VERSION/zstd-$ZSTD_VERSION.tar.gz.sig
fi
$GPG --keyserver $KEYSERVER --recv-keys 0xEF8FE99528B52FFD
$GPG --verify zstd-$ZSTD_VERSION.tar.gz.sig zstd-$ZSTD_VERSION.tar.gz
# verify wangle
echo "$WANGLE_SHA256 wangle-$FBLIBS_VERSION.tar.gz" | sha256sum -c

popd

pushd build

source $PREFIX/activate

export CC=$PREFIX/bin/clang
export CXX=$PREFIX/bin/clang++
export CFLAGS="$CFLAGS -fPIC"
export CXXFLAGS="$CXXFLAGS -fPIC"
COMMON_CMAKE_FLAGS="-DCMAKE_INSTALL_PREFIX=$PREFIX
                    -DCMAKE_PREFIX_PATH=$PREFIX
                    -DCMAKE_BUILD_TYPE=Release
                    -DCMAKE_C_COMPILER=$CC
                    -DCMAKE_CXX_COMPILER=$CXX
                    -DBUILD_SHARED_LIBS=OFF
                    -DCMAKE_CXX_STANDARD=20
                    -DBUILD_TESTING=OFF
                    -DCMAKE_REQUIRED_INCLUDES=$PREFIX/include
                    -DCMAKE_POSITION_INDEPENDENT_CODE=ON"
COMMON_CONFIGURE_FLAGS="--enable-shared=no --prefix=$PREFIX"
COMMON_MAKE_INSTALL_FLAGS="-j$CPUS BUILD_SHARED=no PREFIX=$PREFIX install"

# install bzip2
if [ ! -f $PREFIX/include/bzlib.h ]; then
    if [ -d bzip2-$BZIP2_VERSION ]; then
        rm -rf bzip2-$BZIP2_VERSION
    fi
    tar -xzf ../archives/bzip2-$BZIP2_VERSION.tar.gz
    pushd bzip2-$BZIP2_VERSION
    make $COMMON_MAKE_INSTALL_FLAGS
    popd
fi

# install fmt
if [ ! -d $PREFIX/include/fmt ]; then
    if [ -d fmt-$FMT_VERSION ]; then
        rm -rf fmt-$FMT_VERSION
    fi
    tar -xzf ../archives/fmt-$FMT_VERSION.tar.gz
    pushd fmt-$FMT_VERSION
    mkdir build && pushd build
    cmake .. $COMMON_CMAKE_FLAGS -DFMT_TEST=OFF
    make -j$CPUS install
    popd && popd
fi

# install lz4
if [ ! -f $PREFIX/include/lz4.h ]; then
    if [ -d lz4-$LZ4_VERSION ]; then
        rm -rf lz4-$LZ4_VERSION
    fi
    tar -xzf ../archives/lz4-$LZ4_VERSION.tar.gz
    pushd lz4-$LZ4_VERSION
    make $COMMON_MAKE_INSTALL_FLAGS
    popd
fi

# install xz
if [ ! -f $PREFIX/include/lzma.h ]; then
    if [ -d xz-$XZ_VERSION ]; then
        rm -rf xz-$XZ_VERSION
    fi
    tar -xzf ../archives/xz-$XZ_VERSION.tar.gz
    pushd xz-$XZ_VERSION
    ./configure $COMMON_CONFIGURE_FLAGS
    make -j$CPUS install
    popd
fi

# install zlib
if [ ! -f $PREFIX/include/zlib.h ]; then
    if [ -d zlib-$ZLIB_VERSION ]; then
        rm -rf zlib-$ZLIB_VERSION
    fi
    tar -xzf ../archives/zlib-$ZLIB_VERSION.tar.gz
    pushd zlib-$ZLIB_VERSION
    mkdir build && pushd build
    cmake .. $COMMON_CMAKE_FLAGS
    make -j$CPUS install
    rm $PREFIX/lib/libz.so*
    popd && popd
fi

# install zstd
if [ ! -f $PREFIX/include/zstd.h ]; then
    if [ -d zstd-$ZSTD_VERSION ]; then
        rm -rf zstd-$ZSTD_VERSION
    fi
    tar -xzf ../archives/zstd-$ZSTD_VERSION.tar.gz
    pushd zstd-$ZSTD_VERSION
    # build is used by facebook builder
    mkdir _build
    pushd _build
    cmake ../build/cmake $COMMON_CMAKE_FLAGS -DZSTD_BUILD_SHARED=OFF
    make -j$CPUS install
    popd && popd
fi

#install jemalloc
if [ ! -d $PREFIX/include/jemalloc ]; then
    if [ -d jemalloc ]; then
        rm -rf jemalloc
    fi

    git clone https://github.com/jemalloc/jemalloc.git jemalloc
    pushd jemalloc
    git checkout $JEMALLOC_COMMIT_HASH
    ./autogen.sh --with-malloc-conf="percpu_arena:percpu,oversize_threshold:0,muzzy_decay_ms:5000,dirty_decay_ms:5000"
    env \
        EXTRA_FLAGS="-DJEMALLOC_NO_PRIVATE_NAMESPACE -D_GNU_SOURCE -Wno-redundant-decls" \
        ./configure $COMMON_CONFIGURE_FLAGS --disable-cxx
    make -j$CPUS install
    popd
fi

# install boost
if [ ! -d $PREFIX/include/boost ]; then
    if [ -d boost_$BOOST_VERSION_UNDERSCORES ]; then
        rm -rf boost_$BOOST_VERSION_UNDERSCORES
    fi
    tar -xzf ../archives/boost_$BOOST_VERSION_UNDERSCORES.tar.gz
    pushd boost_$BOOST_VERSION_UNDERSCORES
    ./bootstrap.sh --prefix=$PREFIX --with-toolset=clang --with-python=python3  --without-icu
    ./b2 toolset=clang -j$CPUS install variant=release link=static cxxstd=20 --disable-icu \
        -sZLIB_SOURCE="$PREFIX" -sZLIB_INCLUDE="$PREFIX/include" -sZLIB_LIBPATH="$PREFIX/lib" \
        -sBZIP2_SOURCE="$PREFIX" -sBZIP2_INCLUDE="$PREFIX/include" -sBZIP2_LIBPATH="$PREFIX/lib" \
        -sLZMA_SOURCE="$PREFIX" -sLZMA_INCLUDE="$PREFIX/include" -sLZMA_LIBPATH="$PREFIX/lib" \
        -sZSTD_SOURCE="$PREFIX" -sZSTD_INCLUDE="$PREFIX/include" -sZSTD_LIBPATH="$PREFIX/lib"
    popd
fi

# install double-conversion
if [ ! -d $PREFIX/include/double-conversion ]; then
    if [ -d double-conversion-$DOUBLE_CONVERSION_VERSION ]; then
        rm -rf double-conversion-$DOUBLE_CONVERSION_VERSION
    fi
    tar -xzf ../archives/double-conversion-$DOUBLE_CONVERSION_VERSION.tar.gz
    pushd double-conversion-$DOUBLE_CONVERSION_VERSION
    # build is used by facebook builder
    mkdir build
    pushd build
    cmake .. $COMMON_CMAKE_FLAGS
    make -j$CPUS install
    popd && popd
fi

# install gflags
if [ ! -d $PREFIX/include/gflags ]; then
    if [ -d gflags ]; then
        rm -rf gflags
    fi

    git clone https://github.com/memgraph/gflags.git gflags
    pushd gflags
    git checkout $GFLAGS_COMMIT_HASH
    mkdir build
    pushd build
    cmake .. $COMMON_CMAKE_FLAGS \
        -DREGISTER_INSTALL_PREFIX=OFF \
        -DBUILD_gflags_nothreads_LIB=OFF \
        -DGFLAGS_NO_FILENAMES=0
    make -j$CPUS install
    popd && popd
fi

# install libunwind
if [ ! -f $PREFIX/include/libunwind.h ]; then
    if [ -d libunwind-$LIBUNWIND_VERSION ]; then
        rm -rf libunwind-$LIBUNWIND_VERSION
    fi
    tar -xzf ../archives/libunwind-$LIBUNWIND_VERSION.tar.gz
    pushd libunwind-$LIBUNWIND_VERSION
    ./configure $COMMON_CONFIGURE_FLAGS \
        --disable-minidebuginfo # disable LZMA usage to not depend on libLZMA
    make -j$CPUS install
    popd
fi

# install glog
if [ ! -d $PREFIX/include/glog ]; then
    if [ -d glog-$GLOG_VERSION ]; then
        rm -rf glog-$GLOG_VERSION
    fi
    tar -xzf ../archives/glog-$GLOG_VERSION.tar.gz
    pushd glog-$GLOG_VERSION
    mkdir build
    pushd build
    cmake .. $COMMON_CMAKE_FLAGS -DGFLAGS_NOTHREADS=OFF
    make -j$CPUS install
    popd && popd
fi

# install libevent
if [ ! -d $PREFIX/include/event2 ]; then
    if [ -d libevent-$LIBEVENT_VERSION ]; then
        rm -rf libevent-$LIBEVENT_VERSION
    fi
    tar -xzf ../archives/libevent-$LIBEVENT_VERSION.tar.gz
    pushd libevent-$LIBEVENT_VERSION
    mkdir build
    pushd build
    cmake .. $COMMON_CMAKE_FLAGS \
        -DEVENT__DISABLE_BENCHMARK=ON \
        -DEVENT__DISABLE_REGRESS=ON \
        -DEVENT__DISABLE_SAMPLES=ON \
        -DEVENT__DISABLE_TESTS=ON \
        -DEVENT__LIBRARY_TYPE="STATIC"
    make -j$CPUS install
    popd && popd
fi

# install snappy
if [ ! -f $PREFIX/include/snappy.h ]; then
    if [ -d snappy-$SNAPPY_VERSION ]; then
        rm -rf snappy-$SNAPPY_VERSION
    fi
    tar -xzf ../archives/snappy-$SNAPPY_VERSION.tar.gz
    pushd snappy-$SNAPPY_VERSION
    patch CMakeLists.txt ../../snappy.diff
    mkdir build
    pushd build
    cmake .. $COMMON_CMAKE_FLAGS \
        -DSNAPPY_BUILD_TESTS=OFF \
        -DSNAPPY_BUILD_BENCHMARKS=OFF \
        -DSNAPPY_FUZZING_BUILD=OFF
    make -j$CPUS install
    popd && popd
fi

# install libsodium
if [ ! -f $PREFIX/include/sodium.h ]; then
    if [ -d libsodium-$LIBSODIUM_VERSION ]; then
        rm -rf libsodium-$LIBSODIUM_VERSION
    fi
    tar -xzf ../archives/libsodium-$LIBSODIUM_VERSION.tar.gz
    pushd libsodium-$LIBSODIUM_VERSION
    ./configure $COMMON_CONFIGURE_FLAGS
    make -j$CPUS install
    popd
fi

# install libaio
if [ ! -f $PREFIX/include/libaio.h ]; then
    if [ -d libaio-$LIBAIO_VERSION ]; then
        rm -rf libaio-$LIBAIO_VERSION
    fi
    tar -xzf ../archives/libaio-$LIBAIO_VERSION.tar.gz
    pushd libaio-$LIBAIO_VERSION
    make prefix=$PREFIX ENABLE_SHARED=0 -j$CPUS install
    popd
fi

# install folly
if [ ! -d $PREFIX/include/folly ]; then
    if [ -d folly-$FBLIBS_VERSION ]; then
        rm -rf folly-$FBLIBS_VERSION
    fi
    mkdir folly-$FBLIBS_VERSION
    tar -xzf ../archives/folly-$FBLIBS_VERSION.tar.gz -C folly-$FBLIBS_VERSION
    pushd folly-$FBLIBS_VERSION
    patch folly/CMakeLists.txt ../../folly.diff
    # build is used by facebook builder
    mkdir _build
    pushd _build
    cmake .. $COMMON_CMAKE_FLAGS \
        -DBOOST_LINK_STATIC=ON \
        -DBUILD_TESTS=OFF \
        -DGFLAGS_NOTHREADS=OFF \
        -DCXX_STD="c++20"
    make -j$CPUS install
    popd && popd
fi

# install fizz
if [ ! -d $PREFIX/include/fizz ]; then
    if [ -d fizz-$FBLIBS_VERSION ]; then
        rm -rf fizz-$FBLIBS_VERSION
    fi
    mkdir fizz-$FBLIBS_VERSION
    tar -xzf ../archives/fizz-$FBLIBS_VERSION.tar.gz -C fizz-$FBLIBS_VERSION
    pushd fizz-$FBLIBS_VERSION
    # build is used by facebook builder
    mkdir _build
    pushd _build
    cmake ../fizz $COMMON_CMAKE_FLAGS \
        -DBUILD_TESTS=OFF \
        -DBUILD_EXAMPLES=OFF \
        -DGFLAGS_NOTHREADS=OFF
    make -j$CPUS install
    popd && popd
fi

# install wangle
if [ ! -d $PREFIX/include/wangle ]; then
    if [ -d wangle-$FBLIBS_VERSION ]; then
        rm -rf wangle-$FBLIBS_VERSION
    fi
    mkdir wangle-$FBLIBS_VERSION
    tar -xzf ../archives/wangle-$FBLIBS_VERSION.tar.gz -C wangle-$FBLIBS_VERSION
    pushd wangle-$FBLIBS_VERSION
    # build is used by facebook builder
    mkdir _build
    pushd _build
    cmake ../wangle $COMMON_CMAKE_FLAGS \
        -DBUILD_TESTS=OFF \
        -DBUILD_EXAMPLES=OFF \
        -DGFLAGS_NOTHREADS=OFF
    make -j$CPUS install
    popd && popd
fi

# install proxygen
if [ ! -d $PREFIX/include/proxygen ]; then
    if [ -d proxygen-$FBLIBS_VERSION ]; then
        rm -rf proxygen-$FBLIBS_VERSION
    fi
    mkdir proxygen-$FBLIBS_VERSION
    tar -xzf ../archives/proxygen-$FBLIBS_VERSION.tar.gz -C proxygen-$FBLIBS_VERSION
    pushd proxygen-$FBLIBS_VERSION
    patch cmake/proxygen-config.cmake.in ../../proxygen.diff
    # build is used by facebook builder
    mkdir _build
    pushd _build
    cmake .. $COMMON_CMAKE_FLAGS \
        -DBUILD_TESTS=OFF \
        -DBUILD_SAMPLES=OFF \
        -DGFLAGS_NOTHREADS=OFF \
        -DBUILD_QUIC=OFF
    make -j$CPUS install
    popd && popd
fi

# install flex
if [ ! -f $PREFIX/include/FlexLexer.h ]; then
    if [ -d flex-$FLEX_VERSION ]; then
        rm -rf flex-$FLEX_VERSION
    fi
    tar -xzf ../archives/flex-$FLEX_VERSION.tar.gz
    pushd flex-$FLEX_VERSION
    ./configure $COMMON_CONFIGURE_FLAGS
    make -j$CPUS install
    popd
fi

# install fbthrift
if [ ! -d $PREFIX/include/thrift ]; then
    if [ -d fbthrift-$FBLIBS_VERSION ]; then
        rm -rf fbthrift-$FBLIBS_VERSION
    fi
    git clone --depth 1 --branch v$FBLIBS_VERSION https://github.com/facebook/fbthrift.git fbthrift-$FBLIBS_VERSION
    pushd fbthrift-$FBLIBS_VERSION
    # build is used by facebook builder
    mkdir _build
    pushd _build
    cmake .. $COMMON_CMAKE_FLAGS \
        -Denable_tests=OFF \
        -DGFLAGS_NOTHREADS=OFF \
        -DCMAKE_CXX_FLAGS=-fsized-deallocation
    make -j$CPUS install
    popd
fi

popd

# create toolchain archive
if [ ! -f $NAME-binaries-$DISTRO.tar.gz ]; then
    tar --owner=root --group=root -cpvzf $NAME-binaries-$DISTRO.tar.gz -C /opt $NAME
fi

# output final instructions
echo -e "\n\n"
echo "All tools have been built. They are installed in '$PREFIX'."
echo "In order to distribute the tools to someone else, an archive with the toolchain was created in the 'build' directory."
echo "If you want to install the packed tools you should execute the following command:"
echo
echo "    tar -xvzf build/$NAME-binaries.tar.gz -C /opt"
echo
echo "Because the tools were built on this machine, you should probably change the permissions of the installation directory using:"
echo
echo "    OPTIONAL: chown -R root:root $PREFIX"
echo
echo "In order to use all of the newly compiled tools you should use the prepared activation script:"
echo
echo "    source $PREFIX/activate"
echo
echo "Or, for more advanced uses, you can add the following lines to your script:"
echo
echo "    export PATH=$PREFIX/bin:\$PATH"
echo "    export LD_LIBRARY_PATH=$PREFIX/lib:$PREFIX/lib64"
echo
echo "Enjoy!"
