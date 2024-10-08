# Generated by devtools/yamaker from nixpkgs 22.11.

LIBRARY()

LICENSE(
    "(FTL  OR GPL-2.0-or-later)" AND
    "(FTL OR GPL-2.0-or-later)" AND
    BSL-1.0 AND
    FTL AND
    MIT AND
    MIT-Modern-Variant AND
    MIT-open-group AND
    Public-Domain AND
    Zlib
)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

VERSION(2.13.3)

ORIGINAL_SOURCE(https://gitlab.freedesktop.org/freetype/freetype/)

PEERDIR(
    contrib/libs/zlib
)

ADDINCL(
    GLOBAL contrib/libs/freetype/include
    contrib/libs/freetype/include/freetype/config
    contrib/libs/freetype/src/autofit
    contrib/libs/freetype/src/base
    contrib/libs/freetype/src/bdf
    contrib/libs/freetype/src/cache
    contrib/libs/freetype/src/cff
    contrib/libs/freetype/src/cid
    contrib/libs/freetype/src/dlg
    contrib/libs/freetype/src/lzw
    contrib/libs/freetype/src/pcf
    contrib/libs/freetype/src/pfr
    contrib/libs/freetype/src/psaux
    contrib/libs/freetype/src/pshinter
    contrib/libs/freetype/src/psnames
    contrib/libs/freetype/src/raster
    contrib/libs/freetype/src/sdf
    contrib/libs/freetype/src/sfnt
    contrib/libs/freetype/src/smooth
    contrib/libs/freetype/src/svg
    contrib/libs/freetype/src/truetype
    contrib/libs/freetype/src/type1
    contrib/libs/freetype/src/type42
    contrib/libs/freetype/src/winfonts
)

NO_COMPILER_WARNINGS()

NO_RUNTIME()

CFLAGS(
    -DFT2_BUILD_LIBRARY
    -DFT_CONFIG_OPTION_SYSTEM_ZLIB
)

SRCS(
    src/autofit/autofit.c
    src/base/ftbase.c
    src/base/ftbbox.c
    src/base/ftbdf.c
    src/base/ftbitmap.c
    src/base/ftcid.c
    src/base/ftdebug.c
    src/base/ftfstype.c
    src/base/ftgasp.c
    src/base/ftglyph.c
    src/base/ftgxval.c
    src/base/ftinit.c
    src/base/ftmm.c
    src/base/ftotval.c
    src/base/ftpatent.c
    src/base/ftpfr.c
    src/base/ftstroke.c
    src/base/ftsynth.c
    src/base/ftsystem.c
    src/base/fttype1.c
    src/base/ftwinfnt.c
    src/bdf/bdf.c
    src/bzip2/ftbzip2.c
    src/cache/ftcache.c
    src/cff/cff.c
    src/cid/type1cid.c
    src/dlg/dlgwrap.c
    src/gzip/ftgzip.c
    src/lzw/ftlzw.c
    src/pcf/pcf.c
    src/pfr/pfr.c
    src/psaux/psaux.c
    src/pshinter/pshinter.c
    src/psnames/psnames.c
    src/raster/raster.c
    src/sdf/sdf.c
    src/sfnt/sfnt.c
    src/smooth/smooth.c
    src/svg/svg.c
    src/truetype/truetype.c
    src/type1/type1.c
    src/type42/type42.c
    src/winfonts/winfnt.c
)

IF (OS_DARWIN)
    SRCS(
        src/base/ftmac.c
    )
ENDIF()

END()
