DESTDIR = $$PWD/_bin

TEMPLATE = app
TARGET = qmsdfgen
CONFIG += -std=c++20
QT += core gui widgets

DEFINES += MSDFGEN_STANDALONE

INCLUDEPATH += $$PWD/src
INCLUDEPATH += $$PWD/msdfgen
INCLUDEPATH += $$PWD/config

!msvc:INCLUDEPATH += /usr/include/freetype2
!msvc:LIBS += -lpng -lfreetype -ltinyxml2

msvc:INCLUDEPATH += C:/vcpkg/packages/libpng_x64-windows/include
msvc:INCLUDEPATH += C:/vcpkg/packages/tinyxml2_x64-windows/include
msvc:INCLUDEPATH += C:/vcpkg/packages/freetype_x64-windows/include
msvc:LIBS += -LC:/vcpkg/packages/libpng_x64-windows/lib -llibpng16
msvc:LIBS += -LC:/vcpkg/packages/freetype_x64-windows/lib -lfreetype
msvc:LIBS += -LC:/vcpkg/packages/tinyxml2_x64-windows/lib -ltinyxml2

# Input
HEADERS += msdfgen/msdfgen-ext.h \
           config/msdfgen/msdfgen-config.h \
           msdfgen/msdfgen.h \
           msdfgen/resource.h \
           msdfgen/core/arithmetics.hpp \
           msdfgen/core/base.h \
           msdfgen/core/bitmap-interpolation.hpp \
           msdfgen/core/Bitmap.h \
           msdfgen/core/Bitmap.hpp \
           msdfgen/core/BitmapRef.hpp \
           msdfgen/core/contour-combiners.h \
           msdfgen/core/Contour.h \
           msdfgen/core/convergent-curve-ordering.h \
           msdfgen/core/DistanceMapping.h \
           msdfgen/core/edge-coloring.h \
           msdfgen/core/edge-segments.h \
           msdfgen/core/edge-selectors.h \
           msdfgen/core/EdgeColor.h \
           msdfgen/core/EdgeHolder.h \
           msdfgen/core/equation-solver.h \
           msdfgen/core/export-svg.h \
           msdfgen/core/generator-config.h \
           msdfgen/core/msdf-error-correction.h \
           msdfgen/core/MSDFErrorCorrection.h \
           msdfgen/core/pixel-conversion.hpp \
           msdfgen/core/Projection.h \
           msdfgen/core/Range.hpp \
           msdfgen/core/rasterization.h \
           msdfgen/core/render-sdf.h \
           msdfgen/core/save-bmp.h \
           msdfgen/core/save-fl32.h \
           msdfgen/core/save-rgba.h \
           msdfgen/core/save-tiff.h \
           msdfgen/core/Scanline.h \
           msdfgen/core/sdf-error-estimation.h \
           msdfgen/core/SDFTransformation.h \
           msdfgen/core/shape-description.h \
           msdfgen/core/Shape.h \
           msdfgen/core/ShapeDistanceFinder.h \
           msdfgen/core/ShapeDistanceFinder.hpp \
           msdfgen/core/SignedDistance.hpp \
           msdfgen/core/Vector2.hpp \
           msdfgen/ext/import-font.h \
           msdfgen/ext/import-svg.h \
           msdfgen/ext/resolve-shape-geometry.h \
           msdfgen/ext/save-png.h \
           src/MainWindow.h \
           src/MyImageView.h \
           src/msdfmain.h
SOURCES += \
           msdfgen/core/contour-combiners.cpp \
           msdfgen/core/Contour.cpp \
           msdfgen/core/convergent-curve-ordering.cpp \
           msdfgen/core/DistanceMapping.cpp \
           msdfgen/core/edge-coloring.cpp \
           msdfgen/core/edge-segments.cpp \
           msdfgen/core/edge-selectors.cpp \
           msdfgen/core/EdgeHolder.cpp \
           msdfgen/core/equation-solver.cpp \
           msdfgen/core/export-svg.cpp \
           msdfgen/core/msdf-error-correction.cpp \
           msdfgen/core/MSDFErrorCorrection.cpp \
           msdfgen/core/msdfgen.cpp \
           msdfgen/core/Projection.cpp \
           msdfgen/core/rasterization.cpp \
           msdfgen/core/render-sdf.cpp \
           msdfgen/core/save-bmp.cpp \
           msdfgen/core/save-fl32.cpp \
           msdfgen/core/save-rgba.cpp \
           msdfgen/core/save-tiff.cpp \
           msdfgen/core/Scanline.cpp \
           msdfgen/core/sdf-error-estimation.cpp \
           msdfgen/core/shape-description.cpp \
           msdfgen/core/Shape.cpp \
           msdfgen/ext/import-font.cpp \
           msdfgen/ext/import-svg.cpp \
           msdfgen/ext/resolve-shape-geometry.cpp \
           msdfgen/ext/save-png.cpp \
           src/MainWindow.cpp \
           src/MyImageView.cpp \
           src/main.cpp \
           src/msdfmain.cpp

DISTFILES += msdfgen/main.cpp

FORMS += \
	src/MainWindow.ui

