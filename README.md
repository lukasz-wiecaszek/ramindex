# ramindex - kernel module to provide access to processor's internal memory (caches)

Some ARM processors provide a mechanism to read the internal memory that the L1 caches,
L2/L3 cache(s), and Translation Lookaside Buffer (TLB) structures use
through IMPLEMENTATION DEFINED System registers. When the coherency between the cache data
and the system memory is broken, you may use this mechanism to investigate any issues.
The internal memory to be investigated is selected by programing the IMPLEMENTATION DEFINED
RAMINDEX registers.
On some cores access to that functionality is only available in EL3,
so some Secure Monitor is required to handle SMC calls which will access
System and RAMINDEX registers responsible for that functionality.

## BUILD
To build this module, just run:

    $ make

This should give you a file named "ramindex.ko", which is the requested kernel module.
Such generated kernel module can be loaded and run only on the same kernel version
as the one used for compilation. So if you've updated your kernel, either explicitely
or implicilely via some "update manager", you would have to rebuild your module one more time,
this time against the updated version of your kernel (and kernel headers).

### BUILD AGAIN
To rebuild ramindex.ko module, you have to first clean the previous artefacts of your build process.
To do so, just type

    $ make clean

## INSTALL
To install the module, run "make install" (you might have to be 'root' to have
all necessary permissions to install the module).

If your system has "sudo", do:

    $ make
    $ sudo make install
    $ sudo depmod -a

If your system lacks "sudo", do:

    $ make
    $ su
    (enter root password)
    $ make install
    $ depmod -a
    $ exit


(The `depmod -a` call will re-calculate module dependencies, in order to
automatically load additional kernel modules required by ramindex.ko.

## RUN
Load the ramindex.ko module as root. Yet again.

If your system has "sudo", type:

    $ sudo modprobe ramindex

If your system lacks "sudo", do:

    $ make
    $ su
    (enter root password)
    $ modprobe ramindex
    $ exit

## OPTIONS

### debug
You may specify verbosity of debug messages emmited by ramindex module.
Default value is 0, which means that no debug messages will be printed.
Max value is 4, enabling highest verbosity level. Thus typing

    $ sudo modprobe ramindex debug=4

will turn on the highest verbosity, whereas typing

    $ sudo modprobe ramindex

will use the default level (none of the debug messages will be emited).

## TESTS
Cortex A72 is present on Raspberry Pi 4 boards.
Thus we may perform some tests using that popular platform.
You may for example grab some system memory content using `\dev\mem` device file.
In my configuration kernel code occupies addresses from 0x00210000 till 0x0136ffff
and kernel data from 0x01840000 till 0x01b7ffff.

```
$ sudo cat /proc/iomem

00000000-3b3fffff : System RAM
  00000000-00000fff : reserved
  00210000-0136ffff : Kernel code
  01370000-0183ffff : reserved
  01840000-01b7ffff : Kernel data
```

Therefore I dump such amount so that those two areas (kernel code and kernel data) are covered.
I do so by using `dd`.

    $ sudo dd if=/dev/mem of=mem.dump bs=64K count=440

Then I use `ramindex` utility, to grab the whole content of the L1 instruction cache.

    $ sudo ramindex -l1 -t1

Now I have to compare output from `dd` and from `ramindex -l1 -t1`.
The listing below presents several L1 instruction cache lines as output by `ramindex` utility.

```
SET:0000 WAY:00 V:1 D:0 NS:1 TAG:000000f00000 DATA[0:63] 61feff34 49010094 f1ffff17 1f2003d5 08277412 e5ffffff e9031eaa 1f2003d5 3f2303d5 fd7bbea9 fd030091 f30b00f9 33423bd5 df4303d5 024138d5 410840b9
SET:0000 WAY:01 V:1 D:0 NS:1 TAG:000041fdb000 DATA[0:63] c00700d0 e03700f9 00403c91 000440f9 1f7c00f1 680e0054 43008052 011080d2 130480d2 8102018b 214000d1 350c40f9 3f0015eb 60190054 a00e40f9 020840f9
SET:0000 WAY:02 V:1 D:0 NS:1 TAG:000040afb000 DATA[0:63] 034138d5 652c40b9 0000012a 0100132a 4516a836 86dc4093 8600068a e0937eb2 df0000eb 89040054 f96b44a9 b3018012 3300132a 1f2003d5 004138d5 e13f40f9
SET:0001 WAY:00 V:1 D:0 NS:1 TAG:000000ecc040 DATA[0:63] 5f2403d5 e80300aa 271c0012 e720072a e740072a e78007aa 5f3c00f1 48010054 42001836 078500f8 42001036 074500b8 42000836 07250078 42000036 07010039
SET:0001 WAY:01 V:1 D:0 NS:1 TAG:000040afb040 DATA[0:63] 02c042f9 210002eb 020080d2 a11a0054 e003132a f35341a9 f76343a9 fd7bc8a8 bf2303d5 c0035fd6 a0018012 7302002a f2ffff17 074138d5 e82c40b9 74800091
SET:0001 WAY:02 V:1 D:0 NS:1 TAG:000041fdb040 DATA[0:63] 5f0015eb 61500054 a302138b c5070090 a4602a91 620440f9 420040b2 620400f9 200c00f9 010800f9 9f0204eb 80000054 a00640f9 00007eb2 a00600f9 a00700f0
SET:0002 WAY:00 V:1 D:0 NS:1 TAG:000040afb080 DATA[0:63] e50314aa b3018012 18008052 88edaf37 e70040f9 ff000672 60edff54 68ffff17 a70040f9 e50304aa ff000672 20ebff54 56ffff17 1f03026b 1893821a 80f84892
SET:0002 WAY:01 V:1 D:0 NS:1 TAG:00000032c080 DATA[0:63] fd030091 1fffff97 fd7bc1a8 bf2303d5 c0035fd6 1f2003d5 08277412 e5ffffff e9031eaa 1f2003d5 3f2303d5 fd7bbfa9 fd030091 13ffff97 fd7bc1a8 bf2303d5
SET:0002 WAY:02 V:1 D:0 NS:1 TAG:000000ecc080 DATA[0:63] c0035fd6 e40308cb 840c40f2 80000054 071d00a9 420004cb 0801048b a70400b4 5f0001f1 ea020054 43047cf2 e0000054 7f800071 60000054 6b000054 071d81a8
SET:0003 WAY:00 V:1 D:0 NS:1 TAG:00000032c0c0 DATA[0:63] c0035fd6 1f2003d5 08277412 e5ffffff e9031eaa 1f2003d5 3f2303d5 fd7bbda9 fd030091 f35301a9 f40300aa 531c0012 f51300f9 f50301aa a44f2f94 832240f9
SET:0003 WAY:01 V:1 D:0 NS:1 TAG:0000428190c0 DATA[0:63] ef6340f9 c4710054 7f02156b 83710054 00bfff35 93abff36 8afbff17 0a048052 ff6b00f9 f5fcff17 83180035 1c43f837 5a2f0091 5af37d92 0d0040b9 0b008052
SET:0003 WAY:02 V:1 D:0 NS:1 TAG:000040afb0c0 DATA[0:63] 13008052 180800b8 3300132a 73150035 58150034 97120091 f55b02a9 96de78d3 fb7305a9 fb03182a 15008052 0310c0d2 1c4138d5 1f2003d5 2003152a 00001a2a
SET:0004 WAY:00 V:1 D:0 NS:1 TAG:000042819100 DATA[0:63] 0c008052 06008052 e6fcff17 bf160071 a4da413a 21180054 690600f0 c2008052 29012c91 d50080d2 62fcff17 854a0094 46fbff17 81230011 3f000071 ed1c0054
SET:0004 WAY:01 V:1 D:0 NS:1 TAG:000041fdb100 DATA[0:63] f55b42a9 fb7345a9 fd7bcca8 c0035fd6 73ee7c92 e02200b4 c00700d0 e03700f9 00403c91 637e0453 000440f9 7f0200eb 09100054 7ffe0ff1 69490054 60fe52d3
SET:0004 WAY:02 V:1 D:0 NS:1 TAG:000040afb100 DATA[0:63] 00f408d5 9f3f03d5 df3f03d5 00f038d5 e07300b9 21f038d5 e003152a e17700b9 01200091 3f001beb 63040054 00100091 0403154b 0203154b 1f001beb e3070054
```

Let's start from examining cacheline with tag TAG:000000ecc040.

Here is the actual content at address 0x00ecc040

```
00ecc040  5f 24 03 d5 e8 03 00 aa  27 1c 00 12 e7 20 07 2a  |_$......'.... .*|
00ecc050  e7 40 07 2a e7 80 07 aa  5f 3c 00 f1 48 01 00 54  |.@.*...._<..H..T|
00ecc060  42 00 18 36 07 85 00 f8  42 00 10 36 07 45 00 b8  |B..6....B..6.E..|
00ecc070  42 00 08 36 07 25 00 78  42 00 00 36 07 01 00 39  |B..6.%.xB..6...9|
```
It looks, that the cacheline and memory content at 0x00ecc040 are the same.
So let's check address 0x0032c0c0.

```
0032c0c0  c0 03 5f d6 1f 20 03 d5  08 27 f4 c2 eb ff ff ff  |.._.. ...'......|
0032c0d0  e9 03 1e aa 1f 20 03 d5  3f 23 03 d5 fd 7b bd a9  |..... ..?#...{..|
0032c0e0  fd 03 00 91 f3 53 01 a9  f4 03 00 aa 53 1c 00 12  |.....S......S...|
0032c0f0  f5 13 00 f9 f5 03 01 aa  a4 4f 2f 94 83 22 40 f9  |.........O/.."@.|
```

They match again. That is how I was verifying/testing that the
`ramindex` kernel module and `ramindex` userspace utility work as desired.
Other level and types of caches was tested using exactly the same method.

### TODO
