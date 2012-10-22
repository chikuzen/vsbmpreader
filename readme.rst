==========================================================
vsbmpreader - Windows Bitmap reader for VapourSynth
==========================================================
Windows Bitmap(.bmp/.dib) source filter for VapourSynth.
1/2/4/8/24/32bit color RGB are supported except RLE compressed.

Usage:
------
    >>> import vapoursynth as vs
    >>> core = vs.Core()
    >>> core.std.LoadPlugin('/path/to/vsbmpreader.dll')

    - read single file:
    >>> clip = core.bmpr.Read('/path/to/file.bmp')

    - read two or more files:
    >>> srcs = ['/path/to/file1.bmp', '/path/to/file2.bmp', ... ,'/path/to/fileX.bmp']
    >>> clip = core.bmpr.Read(srcs)

    - read image sequence:
    >>> import os
    >>> dir = '/path/to/the/directory/'
    >>> srcs = [dir + src for src in os.listdir(dir) if src.endswith('.bmp')]
    >>> clip = core.bmpr.Read(srcs)

Note:
-----
    - When reading two or more images, all those width and height need to be the same.

How to compile:
---------------
    on unix system(include mingw/cygwin), type as follows::

    $ git clone git://github.com/chikuzen/vsbmpreader.git
    $ cd ./vsbmpreader
    $ ./configure
    $ make

    if you want to compile with msvc++, then

    1) rename bmpreader.c to bmpreader.cpp
    2) create vcxproj yourself

sourcecode:
-----------
    https://github.com/chikuzen/vsbmpreader


Author: Oka Motofumi (chikuzen.mo at gmail dot com)
