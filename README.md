# clang-extract

A tool to extract code content from source files using the clang and LLVM infrastructure.

## Getting started

### Compiling clang-extract

clang-extract requires clang, LLVM, libelf, zlib, meson and ninja in order to build.
On openSUSE, you can install them by running:
```
$ sudo zypper install clang18 clang18-devel libclang-cpp18 \
       clang-tools libLLVM18 llvm18 llvm18-devel libelf-devel meson ninja \
       zlib-devel libzstd-devel
```
It's advised to use LLVM 18 and higher, since it's well tested. But there
support for LLVM 16 and 17 as well, but you might find issues with it.

Once you have all those packages installed, you must setup the meson build system in order
to compile. You can run either `build-debug.sh` for a debug build with no optimization
and debug flags enabled for development, or a full optimized build with
`build-release.sh`.  Those scripts will create a `build` folder where you can `cd` into
and invoke `ninja` for it to build.  Example:
```
$ ./build-release.sh
$ cd build
$ ninja
```

Then the `clang-extract` binary will be available for you in the `build` folder.

### Testing clang-extract

clang-extract has automated testing. Running the testsuite is as easy as running:
```
$ ninja test
```
inside the `build` directory.  Test results are written into `*.log` files in the
build folder.

## Using clang-extract
Clang-extract currently only support C projects. Assuming clang-extract is compiled, it can be used to extract code content from projects using the following steps.

1. Find, in the project, the function you want to extract, and which file it is in.
2. Compile the project and grab the command line passed to the compiler.
3.  Replace `gcc` with `clang-extract`
4. Pass `-DCE_NO_EXTERNALIZATION -DCE_EXTRACT_FUNCTIONS=function -DCE_OUTPUT_FILE=/tmp/output.c`  to clang-extract.
5. Done. In `/tmp/output.c` will have everything necessary for  `function` to compile without any external dependencies.

### Trivial example

Lets show how clang-extract works with a trivial example. Save the following code as a.c:
```
#include <stdlib.h>
#include <stdio.h>

void *unused_function(void)
{
  return malloc(1024);
}

int main(int argc, char *argv[])
{
  puts("Hello, world!");
  return 0;
}
```
compiling this code with clang would be:
```
$ clang a.c -O2 -o a
```

Note that the source code of `a.c` contain unused functions. In this case, clang-extract can be
used to extract only the functions actually needed. In this case, extract the `main` function:
```
$ clang-extract a.c -O2 -o a -DCE_EXTRACT_FUNCTIONS=main -DCE_OUTPUT_FILE=out.c
```
on the output file `out.c`, you will see the following code:
```
/** clang-extract: from /usr/include/stdio.h:719:1  */
extern int puts (const char *__s);

/** clang-extract: from /tmp/a.c:9:1  */
int main(int argc, char *argv[])
{
  puts("Hello, world!");
  return 0;
}
```
Notice how *any reference to unused_function is removed* and *all headers has been removed* and replaced by a declaration of
`puts`. The output code can be compiled with the same flags used to compile the original code:
```
$ clang out.c -O2 -o a
```
If you desire to keep the includes, see `-DCE_KEEP_INCLUDES` options and the _Supported options_ chapter.

### Symbol Externalization

Code transformation is very often needed when generating livepatches. For example,
in livepatching if we need to call functions that are not exported
in the program (i.e. private), we need to do a process called _externalization_.

Externalization works by redeclaring the original symbol as a pointer to its original
symbol. By doing that we avoid linking issues that may come from using an private
symbol.

Externalization is automatically enabled by default and can be disabled by providing the
`-DCE_NO_EXTERNALIZATION` option.

### Manual externalization

For example, with the following input:
```
#include <stdio.h>

int function(void)
{
  return 0;
}

int main(int argc, char *argv[])
{
  puts("Hello, world!");
  return function();
}
```
calling clang-extract with:
```
$ clang-extract a.c -DCE_EXTRACT_FUNCTIONS=main -DCE_OUTPUT_FILE=out.c -DCE_EXPORT_SYMBOLS=function
```
will externalize the function `function`, as the following output shows:
```
/** clang-extract: from /usr/include/stdio.h:719:1  */
extern int puts (const char *__s);

/** clang-extract: from /tmp/a.c:3:1  */
static int (*klpe_function)(void);

/** clang-extract: from /tmp/a.c:8:1  */
int main(int argc, char *argv[])
{
  puts("Hello, world!");
  return (*klpe_function)();
}
```
as one can see, the `function` was replaced by a pointer to a function `klpe_function`. On livepatching,
this pointer to function is filled with the address of the original function, bypassing any kind of
linking issues generated by symbol visibility.

### Automatic externalization

clang-extract is able to automatically detect which symbols should be externalized if correct
information is given to it. For that, three switches are available for the user to provide
such information:

- `-DCE_DEBUGINFO_PATH=<path>`: Path to the debuginfo of the binary that will
  receive the livepatching. For compiled binaries with `-g`, this is embedded into the binary itself.
  With this clang-extract can discover which symbols are available and automatically mark the functions
  to be externalized.
- `-DCE_IPACLONES_PATH=<path>`: Path containing a single `ipa-clones` or a folder with multiple `ipa-clones`
  file. This is used to verify the symbols that got inlined and may need to have its entire body copied to
  the output file.
- `-DCE_SYMVERS_PATH=<path>`: Path containing the kernel `Modules.symvers` file, used by kernel livepatching
  to also externalize symbols that comes from modules that the livepatch do not want to depend upon.

The precision of the automatic analysis depends of the amount of information the user provides.  Clang-extract
will in any case try to do its best to figure out what is the best option when certain information is not
available.

### Running on glibc project
Let's extract the function `__libc_malloc` from the glibc project. The steps are:
1. Compile the glibc project until `malloc.c` is compiled: `make -j8 | grep malloc.c`
2. Grab the command line:
```
gcc malloc.c -c -std=gnu11 -fgnu89-inline  -g -O2 -Wall -Wwrite-strings -Wundef -Werror -fmerge-all-constants -frounding-math -fno-stack-protector -fno-common -Wp,-U_FORTIFY_SOURCE -Wstrict-prototypes -Wold-style-definition -fmath-errno    -fPIE   -DMORECORE_CLEARS=2  -ftls-model=initial-exec     -I../include -I/home/giulianob/projects/glibc/build_glibc/malloc  -I/home/giulianob/projects/glibc/build_glibc  -I../sysdeps/unix/sysv/linux/x86_64/64  -I../sysdeps/unix/sysv/linux/x86_64  -I../sysdeps/unix/sysv/linux/x86/include -I../sysdeps/unix/sysv/linux/x86  -I../sysdeps/x86/nptl  -I../sysdeps/unix/sysv/linux/wordsize-64  -I../sysdeps/x86_64/nptl  -I../sysdeps/unix/sysv/linux/include -I../sysdeps/unix/sysv/linux  -I../sysdeps/nptl  -I../sysdeps/pthread  -I../sysdeps/gnu  -I../sysdeps/unix/inet  -I../sysdeps/unix/sysv  -I../sysdeps/unix/x86_64  -I../sysdeps/unix  -I../sysdeps/posix  -I../sysdeps/x86_64/64  -I../sysdeps/x86_64/fpu/multiarch  -I../sysdeps/x86_64/fpu  -I../sysdeps/x86/fpu  -I../sysdeps/x86_64/multiarch  -I../sysdeps/x86_64  -I../sysdeps/x86/include -I../sysdeps/x86  -I../sysdeps/ieee754/float128  -I../sysdeps/ieee754/ldbl-96/include -I../sysdeps/ieee754/ldbl-96  -I../sysdeps/ieee754/dbl-64  -I../sysdeps/ieee754/flt-32  -I../sysdeps/wordsize-64  -I../sysdeps/ieee754  -I../sysdeps/generic  -I.. -I../libio -I.  -D_LIBC_REENTRANT -include /home/giulianob/projects/glibc/build_glibc/libc-modules.h -DMODULE_NAME=libc -include ../include/libc-symbols.h  -DPIC  -DUSE_TCACHE=1   -DTOP_NAMESPACE=
glibc -o /home/giulianob/projects/glibc/build_glibc/malloc/malloc.o -MD -MP -MF /home/giulianob/projects/glibc/build_glibc/malloc/malloc.o.dt -MT /home/giulianob/projects/glibc/build_glibc/malloc/malloc.o
```
3. Replace `gcc` with `clang-extract` and add the extra parameters (removed `-Werror` since clang treats some things as errors where gcc doesn't:
```
clang-extract malloc.c -c -std=gnu11 -fgnu89-inline  -g -O2 -Wall -Wwrite-strings -fmerge-all-constants -frounding-math -fno-stack-protector -fno-common -Wp,-U_FORTIFY_SOURCE -Wstrict-prototypes -Wold-style-definition -fmath-errno    -fPIE   -DMORECORE_CLEARS=2  -ftls-model=initial-exec     -I../include -I/home/giulianob/projects/glibc/build_glibc/malloc  -I/home/giulianob/projects/glibc/build_glibc  -I../sysdeps/unix/sysv/linux/x86_64/64  -I../sysdeps/unix/sysv/linux/x86_64  -I../sysdeps/unix/sysv/linux/x86/include -I../sysdeps/unix/sysv/linux/x86  -I../sysdeps/x86/nptl  -I../sysdeps/unix/sysv/linux/wordsize-64  -I../sysdeps/x86_64/nptl  -I../sysdeps/unix/sysv/linux/include -I../sysdeps/unix/sysv/linux  -I../sysdeps/nptl  -I../sysdeps/pthread  -I../sysdeps/gnu  -I../sysdeps/unix/inet  -I../sysdeps/unix/sysv  -I../sysdeps/unix/x86_64  -I../sysdeps/unix  -I../sysdeps/posix  -I../sysdeps/x86_64/64  -I../sysdeps/x86_64/fpu/multiarch  -I../sysdeps/x86_64/fpu  -I../sysdeps/x86/fpu  -I../sysdeps/x86_64/multiarch  -I../sysdeps/x86_64  -I../sysdeps/x86/include -I../sysdeps/x86  -I../sysdeps/ieee754/float128  -I../sysdeps/ieee754/ldbl-96/include -I../sysdeps/ieee754/ldbl-96  -I../sysdeps/ieee754/dbl-64  -I../sysdeps/ieee754/flt-32  -I../sysdeps/wordsize-64  -I../sysdeps/ieee754  -I../sysdeps/generic  -I.. -I../libio -I.  -D_LIBC_REENTRANT -include /home/giulianob/projects/glibc/build_glibc/libc-modules.h -DMODULE_NAME=libc -include ../include/libc-symbols.h  -DPIC  -DUSE_TCACHE=1   -DTOP_NAMESPACE=glibc -o /home/giulianob/projects/glibc/build_glibc/malloc/malloc.o -MD -MP -MF /home/giulianob/projects/glibc/build_glibc/malloc/malloc.o.dt -MT /home/giulianob/projects/glibc/build_glibc/malloc/malloc.o -DCE_NO_EXTERNALIZATION -DCE_OUTPUT_FILE=/tmp/out.c -DCE_EXTRACT_FUNCTIONS=__libc_malloc
```
4. The output should be in `/tmp/out.c` and should be self-compilable. Check it by calling `$ gcc -c /tmp/out.c`. Here is the output for malloc: https://godbolt.org/z/6vrrTPoP9

##  Supported options

Clang-extract support many options which controls the output code:

- `-D__KERNEL__`                  Indicate that we are processing a Linux sourcefile, which triggers some special logics for kernel livepatching.
- `-DCE_EXTRACT_FUNCTIONS=<args>` Extract the functions specified in the <args> list, separated by commas.
- `-DCE_EXPORT_SYMBOLS=<args>`    Force externalization of symbols specified in the <args> list, separated by commas.
- `-DCE_OUTPUT_FILE=<arg>`        Output code to <arg> file.  Default is `<input>.CE.c`.
- `-DCE_NO_EXTERNALIZATION`       Disable symbol externalization.
- `-DCE_DUMP_PASSES`              Dump the results of each transformation pass into files. Files will be dumped at the same path of the input files. Additional files are also generated on `/tmp/` folder.
- `-DCE_KEEP_INCLUDES`            Keep all possible `#include<file>` directives.
- `-DCE_KEEP_INCLUDES=<policy>`   Keep all possible `#include<file>` directives, but using the specified include expansion <policy>.  Valid values are nothing, everything and kernel.
- `-DCE_EXPAND_INCLUDES=<args>`   Force expansion of the headers provided in <args>.
- `-DCE_RENAME_SYMBOLS`           Allow renaming of extracted symbols.
- `-DCE_DEBUGINFO_PATH=<arg>`     Path to the compiled (ELF) object of the desired program to extract.  This is used to decide if externalization is necessary or not for given symbol.
- `-DCE_IPACLONES_PATH=<arg>`     Path to gcc .ipa-clones files generated by gcc.  Used to decide if desired function to extract was inlined into other functions.
- `-DCE_SYMVERS_PATH=<arg>`       Path to kernel Modules.symvers file.  Only used when `-D__KERNEL__` is specified.
- `-DCE_DSC_OUTPUT=<arg>`         Libpulp .dsc file output, used for userspace livepatching.
- `-DCE_LATE_EXTERNALIZE`         Enable late externalization (declare externalized variables later than the original).  May reduce code output when `-DCE_KEEP_INCLUDES` is enabled.

For more switches, see
```
$ clang-extract --help
```
for more options.

## Supported features

Currently we only support projects written in C. Clang-extract is extensively tested with the Linux kernel, glibc and openSSL sourcecode.
C++ support is planned and clang-extract has some tests for it, but it can not handle libstdc++ headers yet.
