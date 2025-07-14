#include "msdfmain.h"



#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>

#include "msdfgen.h"
#ifdef MSDFGEN_EXTENSIONS
#include "msdfgen-ext.h"
#endif

#include "core/ShapeDistanceFinder.h"

#define SDF_ERROR_ESTIMATE_PRECISION 19
#define DEFAULT_ANGLE_THRESHOLD 3.

#if defined(MSDFGEN_EXTENSIONS) && !defined(MSDFGEN_DISABLE_PNG)
#define DEFAULT_IMAGE_EXTENSION "png"
#define SAVE_DEFAULT_IMAGE_FORMAT savePng
#else
#define DEFAULT_IMAGE_EXTENSION "tiff"
#define SAVE_DEFAULT_IMAGE_FORMAT saveTiff
#endif

using namespace msdfgen;

enum class Format {
	AUTO,
	PNG,
	BMP,
	TIFF,
	RGBA,
	FL32,
	TEXT,
	TEXT_FLOAT,
	BINARY,
	BINARY_FLOAT,
	BINARY_FLOAT_BE
};

static bool is8bitFormat(Format format) {
	return format == Format::PNG || format == Format::BMP || format == Format::RGBA || format == Format::TEXT || format == Format::BINARY;
}

static char toupper(char c) {
	return c >= 'a' && c <= 'z' ? c-'a'+'A' : c;
}

static bool parseUnsigned(unsigned &value, const char *arg) {
	char *end = NULL;
	value = (unsigned) strtoul(arg, &end, 10);
	return end > arg && !*end;
}

static bool parseUnsignedDecOrHex(unsigned &value, const char *arg) {
	char *end = NULL;
	if (arg[0] == '0' && (arg[1] == 'x' || arg[1] == 'X')) {
		arg += 2;
		value = (unsigned) strtoul(arg, &end, 16);
	} else
		value = (unsigned) strtoul(arg, &end, 10);
	return end > arg && !*end;
}

static bool parseUnsignedLL(unsigned long long &value, const char *arg) {
	if (*arg >= '0' && *arg <= '9') {
		value = 0;
		do {
			value = 10*value+(*arg++-'0');
		} while (*arg >= '0' && *arg <= '9');
		return !*arg;
	}
	return false;
}

static bool parseDouble(double &value, const char *arg) {
	char *end = NULL;
	value = strtod(arg, &end);
	return end > arg && !*end;
}

static bool parseAngle(double &value, const char *arg) {
	char *end = NULL;
	value = strtod(arg, &end);
	if (end > arg) {
		arg = end;
		if (*arg == 'd' || *arg == 'D') {
			++arg;
			value *= M_PI/180;
		}
		return !*arg;
	}
	return false;
}

static void parseColoring(Shape &shape, const char *edgeAssignment) {
	unsigned c = 0, e = 0;
	if (shape.contours.size() < c) return;
	Contour *contour = &shape.contours[c];
	bool change = false;
	bool clear = true;
	for (const char *in = edgeAssignment; *in; ++in) {
		switch (*in) {
		case ',':
			if (change)
				++e;
			if (clear)
				while (e < contour->edges.size()) {
					contour->edges[e]->color = WHITE;
					++e;
				}
			++c, e = 0;
			if (shape.contours.size() <= c) return;
			contour = &shape.contours[c];
			change = false;
			clear = true;
			break;
		case '?':
			clear = false;
			break;
		case 'C': case 'M': case 'W': case 'Y': case 'c': case 'm': case 'w': case 'y':
			if (change) {
				++e;
				change = false;
			}
			if (e < contour->edges.size()) {
				contour->edges[e]->color = EdgeColor(
							(*in == 'C' || *in == 'c')*CYAN|
							(*in == 'M' || *in == 'm')*MAGENTA|
							(*in == 'Y' || *in == 'y')*YELLOW|
							(*in == 'W' || *in == 'w')*WHITE);
				change = true;
			}
			break;
		}
	}
}

#ifdef MSDFGEN_EXTENSIONS
static bool parseUnicode(unicode_t &unicode, const char *arg) {
	unsigned uuc;
	if (parseUnsignedDecOrHex(uuc, arg)) {
		unicode = uuc;
		return true;
	}
	if (arg[0] == '\'' && arg[1] && arg[2] == '\'' && !arg[3]) {
		unicode = (unicode_t) (unsigned char) arg[1];
		return true;
	}
	return false;
}

#ifndef MSDFGEN_DISABLE_VARIABLE_FONTS
static FontHandle *loadVarFont(FreetypeHandle *library, const char *filename) {
	std::string buffer;
	while (*filename && *filename != '?')
		buffer.push_back(*filename++);
	FontHandle *font = loadFont(library, buffer.c_str());
	if (font && *filename++ == '?') {
		do {
			buffer.clear();
			while (*filename && *filename != '=')
				buffer.push_back(*filename++);
			if (*filename == '=') {
				char *end = NULL;
				double value = strtod(++filename, &end);
				if (end > filename) {
					filename = end;
					setFontVariationAxis(library, font, buffer.c_str(), value);
				}
			}
		} while (*filename++ == '&');
	}
	return font;
}
#endif
#endif

template <int N>
static void invertColor(const BitmapRef<float, N> &bitmap) {
	const float *end = bitmap.pixels+N*bitmap.width*bitmap.height;
	for (float *p = bitmap.pixels; p < end; ++p)
		*p = 1.f-*p;
}

static bool writeTextBitmap(FILE *file, const float *values, int cols, int rows) {
	for (int row = 0; row < rows; ++row) {
		for (int col = 0; col < cols; ++col) {
			int v = clamp(int((*values++)*0x100), 0xff);
			fprintf(file, col ? " %02X" : "%02X", v);
		}
		fprintf(file, "\n");
	}
	return true;
}

static bool writeTextBitmapFloat(FILE *file, const float *values, int cols, int rows) {
	for (int row = 0; row < rows; ++row) {
		for (int col = 0; col < cols; ++col) {
			fprintf(file, col ? " %.9g" : "%.9g", *values++);
		}
		fprintf(file, "\n");
	}
	return true;
}

static bool writeBinBitmap(FILE *file, const float *values, int count) {
	for (int pos = 0; pos < count; ++pos) {
		unsigned char v = clamp(int((*values++)*0x100), 0xff);
		fwrite(&v, 1, 1, file);
	}
	return true;
}

#ifdef __BIG_ENDIAN__
static bool writeBinBitmapFloatBE(FILE *file, const float *values, int count)
#else
static bool writeBinBitmapFloat(FILE *file, const float *values, int count)
#endif
{
	return (int) fwrite(values, sizeof(float), count, file) == count;
}

#ifdef __BIG_ENDIAN__
static bool writeBinBitmapFloat(FILE *file, const float *values, int count)
#else
static bool writeBinBitmapFloatBE(FILE *file, const float *values, int count)
#endif
{
	for (int pos = 0; pos < count; ++pos) {
		const unsigned char *b = reinterpret_cast<const unsigned char *>(values++);
		for (int i = sizeof(float)-1; i >= 0; --i)
			fwrite(b+i, 1, 1, file);
	}
	return true;
}

static bool cmpExtension(const char *path, const char *ext) {
	for (const char *a = path+strlen(path)-1, *b = ext+strlen(ext)-1; b >= ext; --a, --b)
		if (a < path || toupper(*a) != toupper(*b))
			return false;
	return true;
}

template <int N>
static const char *writeOutput(const BitmapConstRef<float, N> &bitmap, const char *filename, Format &format) {
	if (filename) {
		if (format == Format::AUTO) {
#if defined(MSDFGEN_EXTENSIONS) && !defined(MSDFGEN_DISABLE_PNG)
			if (cmpExtension(filename, ".png")) format = Format::PNG;
#else
			if (cmpExtension(filename, ".png"))
				return "PNG format is not available in core-only version.";
#endif
			else if (cmpExtension(filename, ".bmp")) format = Format::BMP;
			else if (cmpExtension(filename, ".tiff") || cmpExtension(filename, ".tif")) format = Format::TIFF;
			else if (cmpExtension(filename, ".rgba")) format = Format::RGBA;
			else if (cmpExtension(filename, ".fl32")) format = Format::FL32;
			else if (cmpExtension(filename, ".txt")) format = Format::TEXT;
			else if (cmpExtension(filename, ".bin")) format = Format::BINARY;
			else
				return "Could not deduce format from output file name.";
		}
		switch (format) {
#if defined(MSDFGEN_EXTENSIONS) && !defined(MSDFGEN_DISABLE_PNG)
		case Format::PNG: return savePng(bitmap, filename) ? NULL : "Failed to write output PNG image.";
#endif
		case Format::BMP: return saveBmp(bitmap, filename) ? NULL : "Failed to write output BMP image.";
		case Format::TIFF: return saveTiff(bitmap, filename) ? NULL : "Failed to write output TIFF image.";
		case Format::RGBA: return saveRgba(bitmap, filename) ? NULL : "Failed to write output RGBA image.";
		case Format::FL32: return saveFl32(bitmap, filename) ? NULL : "Failed to write output FL32 image.";
		case Format::TEXT:
		case Format::TEXT_FLOAT:
			{
				FILE *file = fopen(filename, "w");
				if (!file) return "Failed to write output text file.";
				if (format == Format::TEXT)
					writeTextBitmap(file, bitmap.pixels, N*bitmap.width, bitmap.height);
				else if (format == Format::TEXT_FLOAT)
					writeTextBitmapFloat(file, bitmap.pixels, N*bitmap.width, bitmap.height);
				fclose(file);
				return NULL;
			}
		case Format::BINARY:
		case Format::BINARY_FLOAT:
		case Format::BINARY_FLOAT_BE:
			{
				FILE *file = fopen(filename, "wb");
				if (!file) return "Failed to write output binary file.";
				if (format == Format::BINARY)
					writeBinBitmap(file, bitmap.pixels, N*bitmap.width*bitmap.height);
				else if (format == Format::BINARY_FLOAT)
					writeBinBitmapFloat(file, bitmap.pixels, N*bitmap.width*bitmap.height);
				else if (format == Format::BINARY_FLOAT_BE)
					writeBinBitmapFloatBE(file, bitmap.pixels, N*bitmap.width*bitmap.height);
				fclose(file);
				return NULL;
			}
		default:;
		}
	} else {
		if (format == Format::AUTO || format == Format::TEXT)
			writeTextBitmap(stdout, bitmap.pixels, N*bitmap.width, bitmap.height);
		else if (format == Format::TEXT_FLOAT)
			writeTextBitmapFloat(stdout, bitmap.pixels, N*bitmap.width, bitmap.height);
		else
			return "Unsupported format for standard output.";
	}
	return NULL;
}

#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)
#define MSDFGEN_VERSION_STRING STRINGIZE(MSDFGEN_VERSION)
#ifdef MSDFGEN_VERSION_UNDERLINE
#define VERSION_UNDERLINE STRINGIZE(MSDFGEN_VERSION_UNDERLINE)
#else
#define VERSION_UNDERLINE "--------"
#endif

#if defined(MSDFGEN_EXTENSIONS) && (defined(MSDFGEN_DISABLE_SVG) || defined(MSDFGEN_DISABLE_PNG) || defined(MSDFGEN_DISABLE_VARIABLE_FONTS))
#define TITLE_SUFFIX     " - custom config"
#define SUFFIX_UNDERLINE "----------------"
#elif !defined(MSDFGEN_EXTENSIONS) && defined(MSDFGEN_USE_OPENMP)
#define TITLE_SUFFIX     " - core with OpenMP"
#define SUFFIX_UNDERLINE "-------------------"
#elif !defined(MSDFGEN_EXTENSIONS)
#define TITLE_SUFFIX     " - core only"
#define SUFFIX_UNDERLINE "------------"
#elif defined(MSDFGEN_USE_SKIA) && defined(MSDFGEN_USE_OPENMP)
#define TITLE_SUFFIX     " with Skia & OpenMP"
#define SUFFIX_UNDERLINE "-------------------"
#elif defined(MSDFGEN_USE_SKIA)
#define TITLE_SUFFIX     " with Skia"
#define SUFFIX_UNDERLINE "----------"
#elif defined(MSDFGEN_USE_OPENMP)
#define TITLE_SUFFIX     " with OpenMP"
#define SUFFIX_UNDERLINE "------------"
#else
#define TITLE_SUFFIX
#define SUFFIX_UNDERLINE
#endif

static const char *const versionText =
		"MSDFgen v" MSDFGEN_VERSION_STRING TITLE_SUFFIX "\n"
														"(c) 2016 - " STRINGIZE(MSDFGEN_COPYRIGHT_YEAR) " Viktor Chlumsky";

static const char *const helpText =
		"\n"
		"Multi-channel signed distance field generator by Viktor Chlumsky v" MSDFGEN_VERSION_STRING TITLE_SUFFIX "\n"
																												 "------------------------------------------------------------------" VERSION_UNDERLINE SUFFIX_UNDERLINE "\n"
																																																						 "  Usage: msdfgen"
		#ifdef _WIN32
																																																						 ".exe"
		#endif
																																																						 " <mode> <input specification> <options>\n"
																																																						 "\n"
																																																						 "MODES\n"
																																																						 "  sdf - Generate conventional monochrome (true) signed distance field.\n"
																																																						 "  psdf - Generate monochrome signed perpendicular distance field.\n"
																																																						 "  msdf - Generate multi-channel signed distance field. This is used by default if no mode is specified.\n"
																																																						 "  mtsdf - Generate combined multi-channel and true signed distance field in the alpha channel.\n"
																																																						 "  metrics - Report shape metrics only.\n"
																																																						 "\n"
																																																						 "INPUT SPECIFICATION\n"
																																																						 "  -defineshape <definition>\n"
																																																						 "\tDefines input shape using the ad-hoc text definition.\n"
		#ifdef MSDFGEN_EXTENSIONS
																																																						 "  -font <filename.ttf> <character code>\n"
																																																						 "\tLoads a single glyph from the specified font file.\n"
																																																						 "\tFormat of character code is '?', 63, 0x3F (Unicode value), or g34 (glyph index).\n"
		#endif
																																																						 "  -shapedesc <filename.txt>\n"
																																																						 "\tLoads text shape description from a file.\n"
																																																						 "  -stdin\n"
																																																						 "\tReads text shape description from the standard input.\n"
		#if defined(MSDFGEN_EXTENSIONS) && !defined(MSDFGEN_DISABLE_SVG)
																																																						 "  -svg <filename.svg>\n"
																																																						 "\tLoads the last vector path found in the specified SVG file.\n"
		#endif
		#if defined(MSDFGEN_EXTENSIONS) && !defined(MSDFGEN_DISABLE_VARIABLE_FONTS)
																																																						 "  -varfont <filename and variables> <character code>\n"
																																																						 "\tLoads a single glyph from a variable font. Specify variable values as x.ttf?var1=0.5&var2=1\n"
		#endif
																																																						 "\n"
		// Keep alphabetical order!
		"OPTIONS\n"
		"  -angle <angle>\n"
		"\tSpecifies the minimum angle between adjacent edges to be considered a corner. Append D for degrees.\n"
		"  -apxrange <outermost distance> <innermost distance>\n"
		"\tSpecifies the outermost (negative) and innermost representable distance in pixels.\n"
		"  -arange <outermost distance> <innermost distance>\n"
		"\tSpecifies the outermost (negative) and innermost representable distance in shape units.\n"
		"  -ascale <x scale> <y scale>\n"
		"\tSets the scale used to convert shape units to pixels asymmetrically.\n"
		"  -autoframe\n"
		"\tAutomatically scales (unless specified) and translates the shape to fit.\n"
		"  -coloringstrategy <simple / inktrap / distance>\n"
		"\tSelects the strategy of the edge coloring heuristic.\n"
		"  -dimensions <width> <height>\n"
		"\tSets the dimensions of the output image.\n"
		"  -edgecolors <sequence>\n"
		"\tOverrides automatic edge coloring with the specified color sequence.\n"
		#ifdef MSDFGEN_EXTENSIONS
		"  -emnormalize\n"
		"\tBefore applying scale, normalizes font glyph coordinates so that 1 = 1 em.\n"
		#endif
		"  -errorcorrection <mode>\n"
		"\tChanges the MSDF/MTSDF error correction mode. Use -errorcorrection help for a list of valid modes.\n"
		"  -errordeviationratio <ratio>\n"
		"\tSets the minimum ratio between the actual and maximum expected distance delta to be considered an error.\n"
		"  -errorimproveratio <ratio>\n"
		"\tSets the minimum ratio between the pre-correction distance error and the post-correction distance error.\n"
		"  -estimateerror\n"
		"\tComputes and prints the distance field's estimated fill error to the standard output.\n"
		"  -exportshape <filename.txt>\n"
		"\tSaves the shape description into a text file that can be edited and loaded using -shapedesc.\n"
		"  -exportsvg <filename.svg>\n"
		"\tSaves the shape geometry into a simple SVG file.\n"
		"  -fillrule <nonzero / evenodd / positive / negative>\n"
		"\tSets the fill rule for the scanline pass. Default is nonzero.\n"
		#if defined(MSDFGEN_EXTENSIONS) && !defined(MSDFGEN_DISABLE_PNG)
		"  -format <png / bmp / tiff / rgba / fl32 / text / textfloat / bin / binfloat / binfloatbe>\n"
		#else
		"  -format <bmp / tiff / rgba / fl32 / text / textfloat / bin / binfloat / binfloatbe>\n"
		#endif
		"\tSpecifies the output format of the distance field. Otherwise it is chosen based on output file extension.\n"
		"  -guessorder\n"
		"\tAttempts to detect if shape contours have the wrong winding and generates the SDF with the right one.\n"
		"  -help\n"
		"\tDisplays this help.\n"
		"  -legacy\n"
		"\tUses the original (legacy) distance field algorithms.\n"
		#ifdef MSDFGEN_EXTENSIONS
		"  -noemnormalize\n"
		"\tRaw integer font glyph coordinates will be used. Without this option, legacy scaling will be applied.\n"
		#endif
		#ifdef MSDFGEN_USE_SKIA
		"  -nopreprocess\n"
		"\tDisables path preprocessing which resolves self-intersections and overlapping contours.\n"
		#else
		"  -nooverlap\n"
		"\tDisables resolution of overlapping contours.\n"
		"  -noscanline\n"
		"\tDisables the scanline pass, which corrects the distance field's signs according to the selected fill rule.\n"
		#endif
		"  -o <filename>\n"
		"\tSets the output file name. The default value is \"output." DEFAULT_IMAGE_EXTENSION "\".\n"
		#ifdef MSDFGEN_USE_SKIA
																							  "  -overlap\n"
																							  "\tSwitches to distance field generator with support for overlapping contours.\n"
		#endif
																							  "  -printmetrics\n"
																							  "\tPrints relevant metrics of the shape to the standard output.\n"
																							  "  -pxrange <range>\n"
																							  "\tSets the width of the range between the lowest and highest signed distance in pixels.\n"
																							  "  -range <range>\n"
																							  "\tSets the width of the range between the lowest and highest signed distance in shape units.\n"
																							  "  -reverseorder\n"
																							  "\tGenerates the distance field as if the shape's vertices were in reverse order.\n"
																							  "  -scale <scale>\n"
																							  "\tSets the scale used to convert shape units to pixels.\n"
		#ifdef MSDFGEN_USE_SKIA
																							  "  -scanline\n"
																							  "\tPerforms an additional scanline pass to fix the signs of the distances.\n"
		#endif
																							  "  -seed <n>\n"
																							  "\tSets the random seed for edge coloring heuristic.\n"
																							  "  -stdout\n"
																							  "\tPrints the output instead of storing it in a file. Only text formats are supported.\n"
																							  "  -testrender <filename." DEFAULT_IMAGE_EXTENSION "> <width> <height>\n"
		#if defined(MSDFGEN_EXTENSIONS) && !defined(MSDFGEN_DISABLE_PNG)
																																				 "\tRenders an image preview using the generated distance field and saves it as a PNG file.\n"
		#else
																																				 "\tRenders an image preview using the generated distance field and saves it as a TIFF file.\n"
		#endif
																																				 "  -testrendermulti <filename." DEFAULT_IMAGE_EXTENSION "> <width> <height>\n"
																																																		 "\tRenders an image preview without flattening the color channels.\n"
																																																		 "  -translate <x> <y>\n"
																																																		 "\tSets the translation of the shape in shape units.\n"
																																																		 "  -version\n"
																																																		 "\tPrints the version of the program.\n"
																																																		 "  -windingpreprocess\n"
																																																		 "\tAttempts to fix only the contour windings assuming no self-intersections and even-odd fill rule.\n"
																																																		 "  -yflip\n"
																																																		 "\tInverts the Y axis in the output distance field. The default order is bottom to top.\n"
																																																		 "\n";

static const char *errorCorrectionHelpText =
		"\n"
		"ERROR CORRECTION MODES\n"
		"  auto-fast\n"
		"\tDetects inversion artifacts and distance errors that do not affect edges by range testing.\n"
		"  auto-full\n"
		"\tDetects inversion artifacts and distance errors that do not affect edges by exact distance evaluation.\n"
		"  auto-mixed (default)\n"
		"\tDetects inversions by distance evaluation and distance errors that do not affect edges by range testing.\n"
		"  disabled\n"
		"\tDisables error correction.\n"
		"  distance-fast\n"
		"\tDetects distance errors by range testing. Does not care if edges and corners are affected.\n"
		"  distance-full\n"
		"\tDetects distance errors by exact distance evaluation. Does not care if edges and corners are affected, slow.\n"
		"  edge-fast\n"
		"\tDetects inversion artifacts only by range testing.\n"
		"  edge-full\n"
		"\tDetects inversion artifacts only by exact distance evaluation.\n"
		"  help\n"
		"\tDisplays this help.\n"
		"\n";

QImage msdfmain(Options const &opts)
{
#define ABORT(msg) do { fputs(msg "\n", stderr); return {}; } while (false)

	struct InternalOptions {
		enum {
			NONE,
			SVG,
			FONT,
			VAR_FONT,
			DESCRIPTION_ARG,
			DESCRIPTION_STDIN,
			DESCRIPTION_FILE
		} inputType = InternalOptions::SVG;

		enum {
			SINGLE,
			PERPENDICULAR,
			MULTI,
			MULTI_AND_TRUE,
			METRICS
		} mode = MULTI;

		enum {
			NO_PREPROCESS,
			WINDING_PREPROCESS,
			FULL_PREPROCESS
		} geometryPreproc =
#ifdef MSDFGEN_USE_SKIA
		FULL_PREPROCESS;
#else
		NO_PREPROCESS;
#endif

		bool legacyMode = false;
		MSDFGeneratorConfig generatorConfig;
		bool scanlinePass = geometryPreproc == InternalOptions::NO_PREPROCESS;
		FillRule fillRule = FILL_NONZERO;
		Format format = Format::AUTO;
		const char *input = NULL;
		const char *output = "output." DEFAULT_IMAGE_EXTENSION;
		const char *shapeExport = NULL;
		const char *svgExport = NULL;
		const char *testRender = NULL;
		const char *testRenderMulti = NULL;
		bool outputSpecified = false;
#ifdef MSDFGEN_EXTENSIONS
		bool glyphIndexSpecified = false;
		GlyphIndex glyphIndex;
		unicode_t unicode = 0;
		FontCoordinateScaling fontCoordinateScaling = FONT_SCALING_LEGACY;
		bool fontCoordinateScalingSpecified = false;
#endif

		int width = 64;
		int height = 64;
		int testWidth = 0;
		int testHeight = 0;
		int testWidthM = 0;
		int testHeightM = 0;

		bool autoFrame = false;

		enum {
			RANGE_UNIT,
			RANGE_PX
		} rangeMode = RANGE_PX;

		Range range = 1;
		Range pxRange = 2;
		Vector2 translate;
		Vector2 scale = 1;
		bool scaleSpecified = false;
		double angleThreshold = DEFAULT_ANGLE_THRESHOLD;
		float outputDistanceShift = 0.f;
		const char *edgeAssignment = NULL;
		bool yFlip = false;
		bool printMetrics = false;
		bool estimateError = false;
		bool skipColoring = false;
		enum {
			KEEP,
			REVERSE,
			GUESS
		} orientation = KEEP;

		unsigned long long coloringSeed = 0;
		void (*edgeColoring)(Shape &, double, unsigned long long) = &edgeColoringSimple;
		bool explicitErrorCorrectionMode = false;
	} opts2;

	opts2.input = opts.input_file.c_str();
	opts2.format = Format::PNG;
	opts2.outputSpecified = true;

	opts2.width = opts.width;
	opts2.height = opts.height;
	opts2.autoFrame = true;

	opts2.generatorConfig.overlapSupport = (opts2.geometryPreproc == InternalOptions::NO_PREPROCESS);

#if 0
	// parse command line options
	int argPos = 1;
	bool suggestHelp = false;
	while (argPos < argc) {
		const char *arg = argv[argPos];
#define ARG_CASE(s, p) if ((!strcmp(arg, s)) && argPos+(p) < argc && (++argPos, true))
#define ARG_CASE_OR ) || !strcmp(arg,
#define ARG_MODE(s, m) if (!strcmp(arg, s)) { opts.mode = m; ++argPos; continue; }
#define ARG_IS(s) (!strcmp(argv[argPos], s))
#define SET_FORMAT(fmt, ext) do { opts.format = fmt; if (!opts.outputSpecified) opts.output = "output." ext; } while (false)

		// Accept arguments prefixed with -- instead of -
		if (arg[0] == '-' && arg[1] == '-')
			++arg;

		ARG_MODE("sdf", InternalOptions::SINGLE)
				ARG_MODE("psdf", InternalOptions::PERPENDICULAR)
				ARG_MODE("msdf", InternalOptions::MULTI)
				ARG_MODE("mtsdf", InternalOptions::MULTI_AND_TRUE)
				ARG_MODE("metrics", InternalOptions::METRICS)

		#if defined(MSDFGEN_EXTENSIONS) && !defined(MSDFGEN_DISABLE_SVG)
				ARG_CASE("-svg", 1) {
			opts.inputType = InternalOptions::SVG;
			opts.input = argv[argPos++];
			continue;
		}
#endif
#ifdef MSDFGEN_EXTENSIONS
		//ARG_CASE -font, -varfont
		if (argPos+2 < argc && (
					(!strcmp(arg, "-font") && (opts.inputType = InternalOptions::FONT, true))
			#ifndef MSDFGEN_DISABLE_VARIABLE_FONTS
					|| (!strcmp(arg, "-varfont") && (opts.inputType = InternalOptions::VAR_FONT, true))
			#endif
					) && (++argPos, true)) {
			opts.input = argv[argPos++];
			const char *charArg = argv[argPos++];
			unsigned gi;
			switch (charArg[0]) {
			case 'G': case 'g':
				if (parseUnsignedDecOrHex(gi, charArg+1)) {
					opts.glyphIndex = GlyphIndex(gi);
					opts.glyphIndexSpecified = true;
				}
				break;
			case 'U': case 'u':
				++charArg;
				// fallthrough
			default:
				parseUnicode(opts.unicode, charArg);
			}
			continue;
		}
		ARG_CASE("-noemnormalize", 0) {
			opts.fontCoordinateScaling = FONT_SCALING_NONE;
			opts.fontCoordinateScalingSpecified = true;
			continue;
		}
		ARG_CASE("-emnormalize", 0) {
			opts.fontCoordinateScaling = FONT_SCALING_EM_NORMALIZED;
			opts.fontCoordinateScalingSpecified = true;
			continue;
		}
		ARG_CASE("-legacyfontscaling", 0) {
			opts.fontCoordinateScaling = FONT_SCALING_LEGACY;
			opts.fontCoordinateScalingSpecified = true;
			continue;
		}
#else
		ARG_CASE("-svg", 1) {
			ABORT("SVG input is not available in core-only version.");
		}
		ARG_CASE("-font", 2) {
			ABORT("Font input is not available in core-only version.");
		}
		ARG_CASE("-varfont", 2) {
			ABORT("Variable font input is not available in core-only version.");
		}
#endif
		ARG_CASE("-defineshape", 1) {
			opts.inputType = InternalOptions::DESCRIPTION_ARG;
			opts.input = argv[argPos++];
			continue;
		}
		ARG_CASE("-stdin", 0) {
			opts.inputType = InternalOptions::DESCRIPTION_STDIN;
			opts.input = "stdin";
			continue;
		}
		ARG_CASE("-shapedesc", 1) {
			opts.inputType = InternalOptions::DESCRIPTION_FILE;
			opts.input = argv[argPos++];
			continue;
		}
		ARG_CASE("-o" ARG_CASE_OR "-out" ARG_CASE_OR "-output" ARG_CASE_OR "-imageout", 1) {
			opts.output = argv[argPos++];
			opts.outputSpecified = true;
			continue;
		}
		ARG_CASE("-stdout", 0) {
			opts.output = NULL;
			continue;
		}
		ARG_CASE("-legacy", 0) {
			opts.legacyMode = true;
#ifdef MSDFGEN_EXTENSIONS
			opts.fontCoordinateScaling = FONT_SCALING_LEGACY;
			opts.fontCoordinateScalingSpecified = true;
#endif
			continue;
		}
		ARG_CASE("-nopreprocess", 0) {
			opts.geometryPreproc = InternalOptions::NO_PREPROCESS;
			continue;
		}
		ARG_CASE("-windingpreprocess", 0) {
			opts.geometryPreproc = InternalOptions::WINDING_PREPROCESS;
			continue;
		}
		ARG_CASE("-preprocess", 0) {
			opts.geometryPreproc = InternalOptions::FULL_PREPROCESS;
			continue;
		}
		ARG_CASE("-nooverlap", 0) {
			opts.generatorConfig.overlapSupport = false;
			continue;
		}
		ARG_CASE("-overlap", 0) {
			opts.generatorConfig.overlapSupport = true;
			continue;
		}
		ARG_CASE("-noscanline", 0) {
			opts.scanlinePass = false;
			continue;
		}
		ARG_CASE("-scanline", 0) {
			opts.scanlinePass = true;
			continue;
		}
		ARG_CASE("-fillrule", 1) {
			opts.scanlinePass = true;
			if (ARG_IS("nonzero")) opts.fillRule = FILL_NONZERO;
			else if (ARG_IS("evenodd") || ARG_IS("odd")) opts.fillRule = FILL_ODD;
			else if (ARG_IS("positive")) opts.fillRule = FILL_POSITIVE;
			else if (ARG_IS("negative")) opts.fillRule = FILL_NEGATIVE;
			else
				fputs("Unknown fill rule specified.\n", stderr);
			++argPos;
			continue;
		}
		ARG_CASE("-format", 1) {
			if (ARG_IS("auto")) opts.format = AUTO;
#if defined(MSDFGEN_EXTENSIONS) && !defined(MSDFGEN_DISABLE_PNG)
			else if (ARG_IS("png")) SET_FORMAT(PNG, "png");
#else
			else if (ARG_IS("png"))
				fputs("PNG format is not available in core-only version.\n", stderr);
#endif
			else if (ARG_IS("bmp")) SET_FORMAT(BMP, "bmp");
			else if (ARG_IS("tiff") || ARG_IS("tif")) SET_FORMAT(TIFF, "tiff");
			else if (ARG_IS("rgba")) SET_FORMAT(RGBA, "rgba");
			else if (ARG_IS("fl32")) SET_FORMAT(FL32, "fl32");
			else if (ARG_IS("text") || ARG_IS("txt")) SET_FORMAT(TEXT, "txt");
			else if (ARG_IS("textfloat") || ARG_IS("txtfloat")) SET_FORMAT(TEXT_FLOAT, "txt");
			else if (ARG_IS("bin") || ARG_IS("binary")) SET_FORMAT(BINARY, "bin");
			else if (ARG_IS("binfloat") || ARG_IS("binfloatle")) SET_FORMAT(BINARY_FLOAT, "bin");
			else if (ARG_IS("binfloatbe")) SET_FORMAT(BINARY_FLOAT_BE, "bin");
			else
				fputs("Unknown format specified.\n", stderr);
			++argPos;
			continue;
		}
		ARG_CASE("-dimensions" ARG_CASE_OR "-size", 2) {
			unsigned w, h;
			if (!(parseUnsigned(w, argv[argPos++]) && parseUnsigned(h, argv[argPos++]) && w && h))
				ABORT("Invalid dimensions. Use -dimensions <width> <height> with two positive integers.");
			opts.width = w, opts.height = h;
			continue;
		}
		ARG_CASE("-autoframe", 0) {
			opts.autoFrame = true;
			continue;
		}
		ARG_CASE("-range" ARG_CASE_OR "-unitrange", 1) {
			double r;
			if (!parseDouble(r, argv[argPos++]))
				ABORT("Invalid range argument. Use -range <range> with a real number.");
			if (r == 0)
				ABORT("Range must be non-zero.");
			opts.rangeMode = InternalOptions::RANGE_UNIT;
			opts.range = Range(r);
			continue;
		}
		ARG_CASE("-pxrange", 1) {
			double r;
			if (!parseDouble(r, argv[argPos++]))
				ABORT("Invalid range argument. Use -pxrange <range> with a real number.");
			if (r == 0)
				ABORT("Range must be non-zero.");
			opts.rangeMode = InternalOptions::RANGE_PX;
			opts.pxRange = Range(r);
			continue;
		}
		ARG_CASE("-arange" ARG_CASE_OR "-aunitrange", 2) {
			double r0, r1;
			if (!(parseDouble(r0, argv[argPos++]) && parseDouble(r1, argv[argPos++])))
				ABORT("Invalid range arguments. Use -arange <minimum> <maximum> with two real numbers.");
			if (r0 == r1)
				ABORT("Range must be non-empty.");
			opts.rangeMode = InternalOptions::RANGE_UNIT;
			opts.range = Range(r0, r1);
			continue;
		}
		ARG_CASE("-apxrange", 2) {
			double r0, r1;
			if (!(parseDouble(r0, argv[argPos++]) && parseDouble(r1, argv[argPos++])))
				ABORT("Invalid range arguments. Use -apxrange <minimum> <maximum> with two real numbers.");
			if (r0 == r1)
				ABORT("Range must be non-empty.");
			opts.rangeMode = InternalOptions::RANGE_PX;
			opts.pxRange = Range(r0, r1);
			continue;
		}
		ARG_CASE("-scale", 1) {
			double s;
			if (!(parseDouble(s, argv[argPos++]) && s > 0))
				ABORT("Invalid scale argument. Use -scale <scale> with a positive real number.");
			opts.scale = s;
			opts.scaleSpecified = true;
			continue;
		}
		ARG_CASE("-ascale", 2) {
			double sx, sy;
			if (!(parseDouble(sx, argv[argPos++]) && parseDouble(sy, argv[argPos++]) && sx > 0 && sy > 0))
				ABORT("Invalid scale arguments. Use -ascale <x> <y> with two positive real numbers.");
			opts.scale.set(sx, sy);
			opts.scaleSpecified = true;
			continue;
		}
		ARG_CASE("-translate", 2) {
			double tx, ty;
			if (!(parseDouble(tx, argv[argPos++]) && parseDouble(ty, argv[argPos++])))
				ABORT("Invalid translate arguments. Use -translate <x> <y> with two real numbers.");
			opts.translate.set(tx, ty);
			continue;
		}
		ARG_CASE("-angle", 1) {
			double at;
			if (!parseAngle(at, argv[argPos++]))
				ABORT("Invalid angle threshold. Use -angle <min angle> with a positive real number less than PI or a value in degrees followed by 'd' below 180d.");
			opts.angleThreshold = at;
			continue;
		}
		ARG_CASE("-errorcorrection", 1) {
			if (ARG_IS("disable") || ARG_IS("disabled") || ARG_IS("0") || ARG_IS("none") || ARG_IS("false")) {
				opts.generatorConfig.errorCorrection.mode = ErrorCorrectionConfig::DISABLED;
				opts.generatorConfig.errorCorrection.distanceCheckMode = ErrorCorrectionConfig::DO_NOT_CHECK_DISTANCE;
			} else if (ARG_IS("default") || ARG_IS("auto") || ARG_IS("auto-mixed") || ARG_IS("mixed")) {
				opts.generatorConfig.errorCorrection.mode = ErrorCorrectionConfig::EDGE_PRIORITY;
				opts.generatorConfig.errorCorrection.distanceCheckMode = ErrorCorrectionConfig::CHECK_DISTANCE_AT_EDGE;
			} else if (ARG_IS("auto-fast") || ARG_IS("fast")) {
				opts.generatorConfig.errorCorrection.mode = ErrorCorrectionConfig::EDGE_PRIORITY;
				opts.generatorConfig.errorCorrection.distanceCheckMode = ErrorCorrectionConfig::DO_NOT_CHECK_DISTANCE;
			} else if (ARG_IS("auto-full") || ARG_IS("full")) {
				opts.generatorConfig.errorCorrection.mode = ErrorCorrectionConfig::EDGE_PRIORITY;
				opts.generatorConfig.errorCorrection.distanceCheckMode = ErrorCorrectionConfig::ALWAYS_CHECK_DISTANCE;
			} else if (ARG_IS("distance") || ARG_IS("distance-fast") || ARG_IS("indiscriminate") || ARG_IS("indiscriminate-fast")) {
				opts.generatorConfig.errorCorrection.mode = ErrorCorrectionConfig::INDISCRIMINATE;
				opts.generatorConfig.errorCorrection.distanceCheckMode = ErrorCorrectionConfig::DO_NOT_CHECK_DISTANCE;
			} else if (ARG_IS("distance-full") || ARG_IS("indiscriminate-full")) {
				opts.generatorConfig.errorCorrection.mode = ErrorCorrectionConfig::INDISCRIMINATE;
				opts.generatorConfig.errorCorrection.distanceCheckMode = ErrorCorrectionConfig::ALWAYS_CHECK_DISTANCE;
			} else if (ARG_IS("edge-fast")) {
				opts.generatorConfig.errorCorrection.mode = ErrorCorrectionConfig::EDGE_ONLY;
				opts.generatorConfig.errorCorrection.distanceCheckMode = ErrorCorrectionConfig::DO_NOT_CHECK_DISTANCE;
			} else if (ARG_IS("edge") || ARG_IS("edge-full")) {
				opts.generatorConfig.errorCorrection.mode = ErrorCorrectionConfig::EDGE_ONLY;
				opts.generatorConfig.errorCorrection.distanceCheckMode = ErrorCorrectionConfig::ALWAYS_CHECK_DISTANCE;
			} else if (ARG_IS("help")) {
				puts(errorCorrectionHelpText);
				return 0;
			} else
				fputs("Unknown error correction mode. Use -errorcorrection help for more information.\n", stderr);
			++argPos;
			opts.explicitErrorCorrectionMode = true;
			continue;
		}
		ARG_CASE("-errordeviationratio", 1) {
			double edr;
			if (!(parseDouble(edr, argv[argPos++]) && edr > 0))
				ABORT("Invalid error deviation ratio. Use -errordeviationratio <ratio> with a positive real number.");
			opts.generatorConfig.errorCorrection.minDeviationRatio = edr;
			continue;
		}
		ARG_CASE("-errorimproveratio", 1) {
			double eir;
			if (!(parseDouble(eir, argv[argPos++]) && eir > 0))
				ABORT("Invalid error improvement ratio. Use -errorimproveratio <ratio> with a positive real number.");
			opts.generatorConfig.errorCorrection.minImproveRatio = eir;
			continue;
		}
		ARG_CASE("-coloringstrategy" ARG_CASE_OR "-edgecoloring", 1) {
			if (ARG_IS("simple")) opts.edgeColoring = &edgeColoringSimple;
			else if (ARG_IS("inktrap")) opts.edgeColoring = &edgeColoringInkTrap;
			else if (ARG_IS("distance")) opts.edgeColoring = &edgeColoringByDistance;
			else
				fputs("Unknown coloring strategy specified.\n", stderr);
			++argPos;
			continue;
		}
		ARG_CASE("-edgecolors", 1) {
			static const char *allowed = " ?,cmwyCMWY";
			for (int i = 0; argv[argPos][i]; ++i) {
				for (int j = 0; allowed[j]; ++j)
					if (argv[argPos][i] == allowed[j])
						goto EDGE_COLOR_VERIFIED;
				ABORT("Invalid edge coloring sequence. Use -edgecolors <color sequence> with only the colors C, M, Y, and W. Separate contours by commas and use ? to keep the default assigment for a contour.");
EDGE_COLOR_VERIFIED:;
			}
			opts.edgeAssignment = argv[argPos++];
			continue;
		}
		ARG_CASE("-distanceshift", 1) {
			double ds;
			if (!parseDouble(ds, argv[argPos++]))
				ABORT("Invalid distance shift. Use -distanceshift <shift> with a real value.");
			opts.outputDistanceShift = (float) ds;
			continue;
		}
		ARG_CASE("-exportshape", 1) {
			opts.shapeExport = argv[argPos++];
			continue;
		}
		ARG_CASE("-exportsvg", 1) {
			opts.svgExport = argv[argPos++];
			continue;
		}
		ARG_CASE("-testrender", 3) {
			unsigned w, h;
			opts.testRender = argv[argPos++];
			if (!(parseUnsigned(w, argv[argPos++]) && parseUnsigned(h, argv[argPos++]) && (int) w > 0 && (int) h > 0))
				ABORT("Invalid arguments for test render. Use -testrender <output." DEFAULT_IMAGE_EXTENSION "> <width> <height>.");
			opts.testWidth = w;
			opts.testHeight = h;
			continue;
		}
		ARG_CASE("-testrendermulti", 3) {
			unsigned w, h;
			opts.testRenderMulti = argv[argPos++];
			if (!(parseUnsigned(w, argv[argPos++]) && parseUnsigned(h, argv[argPos++]) && (int) w > 0 && (int) h > 0))
				ABORT("Invalid arguments for test render. Use -testrendermulti <output." DEFAULT_IMAGE_EXTENSION "> <width> <height>.");
			opts.testWidthM = w;
			opts.testHeightM = h;
			continue;
		}
		ARG_CASE("-yflip", 0) {
			opts.yFlip = true;
			continue;
		}
		ARG_CASE("-printmetrics", 0) {
			opts.printMetrics = true;
			continue;
		}
		ARG_CASE("-estimateerror", 0) {
			opts.estimateError = true;
			continue;
		}
		ARG_CASE("-keeporder", 0) {
			opts.orientation = InternalOptions::KEEP;
			continue;
		}
		ARG_CASE("-reverseorder", 0) {
			opts.orientation = InternalOptions::REVERSE;
			continue;
		}
		ARG_CASE("-guessorder", 0) {
			opts.orientation = InternalOptions::GUESS;
			continue;
		}
		ARG_CASE("-seed", 1) {
			if (!parseUnsignedLL(opts.coloringSeed, argv[argPos++]))
				ABORT("Invalid seed. Use -seed <N> with N being a non-negative integer.");
			continue;
		}
		ARG_CASE("-version", 0) {
			puts(versionText);
			return 0;
		}
		ARG_CASE("-help", 0) {
			puts(helpText);
			return 0;
		}
		fprintf(stderr, "Unknown setting or insufficient parameters: %s\n", argv[argPos++]);
		suggestHelp = true;
	}
	if (suggestHelp)
		fprintf(stderr, "Use -help for more information.\n");

#endif

	// Load input
	Shape::Bounds svgViewBox = { };
	double glyphAdvance = 0;
	if (!opts2.inputType || !opts2.input) {
#ifdef MSDFGEN_EXTENSIONS
#ifdef MSDFGEN_DISABLE_SVG
		ABORT("No input specified! Use -font <file.ttf/otf> <character code> or see -help.");
#else
		ABORT("No input specified! Use either -svg <file.svg> or -font <file.ttf/otf> <character code>, or see -help.");
#endif
#else
		ABORT("No input specified! See -help.");
#endif
	}
	if (opts2.mode == InternalOptions::MULTI_AND_TRUE && (opts2.format == Format::BMP || (opts2.format == Format::AUTO && opts2.output && cmpExtension(opts2.output, ".bmp"))))
		ABORT("Incompatible image format. A BMP file cannot contain alpha channel, which is required in mtsdf mode.");
	Shape shape;
	switch (opts2.inputType) {
#if defined(MSDFGEN_EXTENSIONS) && !defined(MSDFGEN_DISABLE_SVG)
	case InternalOptions::SVG:
		{
			int svgImportFlags = loadSvgShape(shape, svgViewBox, opts2.input);
			if (!(svgImportFlags&SVG_IMPORT_SUCCESS_FLAG))
				ABORT("Failed to load shape from SVG file.");
			if (svgImportFlags&SVG_IMPORT_PARTIAL_FAILURE_FLAG)
				fputs("Warning: Failed to load part of SVG file.\n", stderr);
			if (svgImportFlags&SVG_IMPORT_INCOMPLETE_FLAG)
				fputs("Warning: SVG file contains multiple paths or shapes but this version is only able to load one.\n", stderr);
			else if (svgImportFlags&SVG_IMPORT_UNSUPPORTED_FEATURE_FLAG)
				fputs("Warning: SVG file likely contains elements that are unsupported.\n", stderr);
			if (svgImportFlags&SVG_IMPORT_TRANSFORMATION_IGNORED_FLAG)
				fputs("Warning: SVG path transformation ignored.\n", stderr);
			break;
		}
#endif
#ifdef MSDFGEN_EXTENSIONS
	case InternalOptions::FONT: case InternalOptions::VAR_FONT:
		{
			if (!opts2.glyphIndexSpecified && !opts2.unicode)
				ABORT("No character specified! Use -font <file.ttf/otf> <character code>. Character code can be a Unicode index (65, 0x41), a character in apostrophes ('A'), or a glyph index prefixed by g (g36, g0x24).");
			struct FreetypeFontGuard {
				FreetypeHandle *ft;
				FontHandle *font;
				FreetypeFontGuard() : ft(), font() { }
				~FreetypeFontGuard() {
					if (ft) {
						if (font)
							destroyFont(font);
						deinitializeFreetype(ft);
					}
				}
			} guard;
			if (!(guard.ft = initializeFreetype()))
				ABORT("Failed to initialize FreeType library.");
			if (!(guard.font = (
		  #ifndef MSDFGEN_DISABLE_VARIABLE_FONTS
					  opts2.inputType == InternalOptions::VAR_FONT ? loadVarFont(guard.ft, opts2.input) :
		  #endif
					  loadFont(guard.ft, opts2.input)
					  )))
				ABORT("Failed to load font file.");
			if (opts2.unicode)
				getGlyphIndex(opts2.glyphIndex, guard.font, opts2.unicode);
			if (!loadGlyph(shape, guard.font, opts2.glyphIndex, opts2.fontCoordinateScaling, &glyphAdvance))
				ABORT("Failed to load glyph from font file.");
			if (!opts2.fontCoordinateScalingSpecified && (!opts2.autoFrame || opts2.scaleSpecified || opts2.rangeMode == InternalOptions::RANGE_UNIT || opts2.mode == InternalOptions::METRICS || opts2.printMetrics || opts2.shapeExport || opts2.svgExport)) {
				fputs(
							"Warning: Using legacy font coordinate conversion for compatibility reasons.\n"
							"         The implicit scaling behavior will likely change in a future version resulting in different output.\n"
							"         To silence this warning, use one of the following options:\n"
							"           -noemnormalize to switch to the correct native font coordinates,\n"
							"           -emnormalize to switch to coordinates normalized to 1 em, or\n"
							"           -legacyfontscaling to keep current behavior and make sure it will not change.\n", stderr);
			}
			break;
		}
#endif
	case InternalOptions::DESCRIPTION_ARG:
		{
			if (!readShapeDescription(opts2.input, shape, &opts2.skipColoring))
				ABORT("Parse error in shape description.");
			break;
		}
	case InternalOptions::DESCRIPTION_STDIN:
		{
			if (!readShapeDescription(stdin, shape, &opts2.skipColoring))
				ABORT("Parse error in shape description.");
			break;
		}
	case InternalOptions::DESCRIPTION_FILE:
		{
			FILE *file = fopen(opts2.input, "r");
			if (!file)
				ABORT("Failed to load shape description file.");
			bool readSuccessful = readShapeDescription(file, shape, &opts2.skipColoring);
			fclose(file);
			if (!readSuccessful)
				ABORT("Parse error in shape description.");
			break;
		}
	default:;
	}

	// Validate and normalize shape
	if (!shape.validate())
		ABORT("The geometry of the loaded shape is invalid.");
	switch (opts2.geometryPreproc) {
	case InternalOptions::NO_PREPROCESS:
		break;
	case InternalOptions::WINDING_PREPROCESS:
		shape.orientContours();
		break;
	case InternalOptions::FULL_PREPROCESS:
#ifdef MSDFGEN_USE_SKIA
		if (!resolveShapeGeometry(shape))
			fputs("Shape geometry preprocessing failed, skipping.\n", stderr);
		else if (skipColoring) {
			skipColoring = false;
			fputs("Note: Input shape coloring won't be preserved due to geometry preprocessing.\n", stderr);
		}
#else
		ABORT("Shape geometry preprocessing (-preprocess) is not available in this version because the Skia library is not present.");
#endif
		break;
	}
	shape.normalize();
	if (opts2.yFlip)
		shape.inverseYAxis = !shape.inverseYAxis;

	double avgScale = .5*(opts2.scale.x+opts2.scale.y);
	Shape::Bounds bounds = { };
	if (opts2.autoFrame || opts2.mode == InternalOptions::METRICS || opts2.printMetrics || opts2.orientation == InternalOptions::GUESS || opts2.svgExport)
		bounds = shape.getBounds();

	if (opts2.outputDistanceShift) {
		Range &rangeRef = opts2.rangeMode == InternalOptions::RANGE_PX ? opts2.pxRange : opts2.range;
		double rangeShift = -opts2.outputDistanceShift*(rangeRef.upper-rangeRef.lower);
		rangeRef.lower += rangeShift;
		rangeRef.upper += rangeShift;
	}

	// Auto-frame
	if (opts2.autoFrame) {
		double l = bounds.l, b = bounds.b, r = bounds.r, t = bounds.t;
		Vector2 frame(opts2.width, opts2.height);
		if (!opts2.scaleSpecified) {
			if (opts2.rangeMode == InternalOptions::RANGE_UNIT)
				l += opts2.range.lower, b += opts2.range.lower, r -= opts2.range.lower, t -= opts2.range.lower;
			else
				frame += 2*opts2.pxRange.lower;
		}
		if (l >= r || b >= t)
			l = 0, b = 0, r = 1, t = 1;
		if (frame.x <= 0 || frame.y <= 0)
			ABORT("Cannot fit the specified pixel range.");
		Vector2 dims(r-l, t-b);
		if (opts2.scaleSpecified)
			opts2.translate = .5*(frame/opts2.scale-dims)-Vector2(l, b);
		else {
			if (dims.x*frame.y < dims.y*frame.x) {
				opts2.translate.set(.5*(frame.x/frame.y*dims.y-dims.x)-l, -b);
				opts2.scale = avgScale = frame.y/dims.y;
			} else {
				opts2.translate.set(-l, .5*(frame.y/frame.x*dims.x-dims.y)-b);
				opts2.scale = avgScale = frame.x/dims.x;
			}
		}
		if (opts2.rangeMode == InternalOptions::RANGE_PX && !opts2.scaleSpecified)
			opts2.translate -= opts2.pxRange.lower/opts2.scale;
	}

	if (opts2.rangeMode == InternalOptions::RANGE_PX)
		opts2.range = opts2.pxRange/min(opts2.scale.x, opts2.scale.y);

	// Print metrics
	if (opts2.mode == InternalOptions::METRICS || opts2.printMetrics) {
		FILE *out = stdout;
		if (opts2.mode == InternalOptions::METRICS && opts2.outputSpecified)
			out = fopen(opts2.output, "w");
		if (!out)
			ABORT("Failed to write output file.");
		if (shape.inverseYAxis)
			fprintf(out, "inverseY = true\n");
		if (svgViewBox.l < svgViewBox.r && svgViewBox.b < svgViewBox.t)
			fprintf(out, "view box = %.17g, %.17g, %.17g, %.17g\n", svgViewBox.l, svgViewBox.b, svgViewBox.r, svgViewBox.t);
		if (bounds.l < bounds.r && bounds.b < bounds.t)
			fprintf(out, "bounds = %.17g, %.17g, %.17g, %.17g\n", bounds.l, bounds.b, bounds.r, bounds.t);
		if (glyphAdvance != 0)
			fprintf(out, "advance = %.17g\n", glyphAdvance);
		if (opts2.autoFrame) {
			if (!opts2.scaleSpecified)
				fprintf(out, "scale = %.17g\n", avgScale);
			fprintf(out, "translate = %.17g, %.17g\n", opts2.translate.x, opts2.translate.y);
		}
		if (opts2.rangeMode == InternalOptions::RANGE_PX)
			fprintf(out, "range %.17g to %.17g\n", opts2.range.lower, opts2.range.upper);
		if (opts2.mode == InternalOptions::METRICS && opts2.outputSpecified)
			fclose(out);
	}

	// Compute output
	SDFTransformation transformation(Projection(opts2.scale, opts2.translate), opts2.range);
	Bitmap<float, 1> sdf;
	Bitmap<float, 3> msdf;
	Bitmap<float, 4> mtsdf;
	MSDFGeneratorConfig postErrorCorrectionConfig(opts2.generatorConfig);
	if (opts2.scanlinePass) {
		if (opts2.explicitErrorCorrectionMode && opts2.generatorConfig.errorCorrection.distanceCheckMode != ErrorCorrectionConfig::DO_NOT_CHECK_DISTANCE) {
			const char *fallbackModeName = "unknown";
			switch (opts2.generatorConfig.errorCorrection.mode) {
			case ErrorCorrectionConfig::DISABLED: fallbackModeName = "disabled"; break;
			case ErrorCorrectionConfig::INDISCRIMINATE: fallbackModeName = "distance-fast"; break;
			case ErrorCorrectionConfig::EDGE_PRIORITY: fallbackModeName = "auto-fast"; break;
			case ErrorCorrectionConfig::EDGE_ONLY: fallbackModeName = "edge-fast"; break;
			}
			fprintf(stderr, "Selected error correction mode not compatible with scanline pass, falling back to %s.\n", fallbackModeName);
		}
		opts2.generatorConfig.errorCorrection.mode = ErrorCorrectionConfig::DISABLED;
		postErrorCorrectionConfig.errorCorrection.distanceCheckMode = ErrorCorrectionConfig::DO_NOT_CHECK_DISTANCE;
	}
	switch (opts2.mode) {
	case InternalOptions::SINGLE:
		{
			sdf = Bitmap<float, 1>(opts2.width, opts2.height);
			if (opts2.legacyMode)
				generateSDF_legacy(sdf, shape, opts2.range, opts2.scale, opts2.translate);
			else
				generateSDF(sdf, shape, transformation, opts2.generatorConfig);
			break;
		}
	case InternalOptions::PERPENDICULAR:
		{
			sdf = Bitmap<float, 1>(opts2.width, opts2.height);
			if (opts2.legacyMode)
				generatePSDF_legacy(sdf, shape, opts2.range, opts2.scale, opts2.translate);
			else
				generatePSDF(sdf, shape, transformation, opts2.generatorConfig);
			break;
		}
	case InternalOptions::MULTI:
		{
			if (!opts2.skipColoring)
				opts2.edgeColoring(shape, opts2.angleThreshold, opts2.coloringSeed);
			if (opts2.edgeAssignment)
				parseColoring(shape, opts2.edgeAssignment);
			msdf = Bitmap<float, 3>(opts2.width, opts2.height);
			if (opts2.legacyMode)
				generateMSDF_legacy(msdf, shape, opts2.range, opts2.scale, opts2.translate, opts2.generatorConfig.errorCorrection);
			else
				generateMSDF(msdf, shape, transformation, opts2.generatorConfig);
			break;
		}
	case InternalOptions::MULTI_AND_TRUE:
		{
			if (!opts2.skipColoring)
				opts2.edgeColoring(shape, opts2.angleThreshold, opts2.coloringSeed);
			if (opts2.edgeAssignment)
				parseColoring(shape, opts2.edgeAssignment);
			mtsdf = Bitmap<float, 4>(opts2.width, opts2.height);
			if (opts2.legacyMode)
				generateMTSDF_legacy(mtsdf, shape, opts2.range, opts2.scale, opts2.translate, opts2.generatorConfig.errorCorrection);
			else
				generateMTSDF(mtsdf, shape, transformation, opts2.generatorConfig);
			break;
		}
	default:;
	}

	if (opts2.orientation == InternalOptions::GUESS) {
		// Get sign of signed distance outside bounds
		Point2 p(bounds.l-(bounds.r-bounds.l)-1, bounds.b-(bounds.t-bounds.b)-1);
		double distance = SimpleTrueShapeDistanceFinder::oneShotDistance(shape, p);
		opts2.orientation = distance <= 0 ? InternalOptions::KEEP : InternalOptions::REVERSE;
	}
	if (opts2.orientation == InternalOptions::REVERSE) {
		switch (opts2.mode) {
		case InternalOptions::SINGLE:
		case InternalOptions::PERPENDICULAR:
			invertColor<1>(sdf);
			break;
		case InternalOptions::MULTI:
			invertColor<3>(msdf);
			break;
		case InternalOptions::MULTI_AND_TRUE:
			invertColor<4>(mtsdf);
			break;
		default:;
		}
	}
	if (opts2.scanlinePass) {
		switch (opts2.mode) {
		case InternalOptions::SINGLE:
		case InternalOptions::PERPENDICULAR:
			distanceSignCorrection(sdf, shape, transformation, opts2.fillRule);
			break;
		case InternalOptions::MULTI:
			distanceSignCorrection(msdf, shape, transformation, opts2.fillRule);
			msdfErrorCorrection(msdf, shape, transformation, postErrorCorrectionConfig);
			break;
		case InternalOptions::MULTI_AND_TRUE:
			distanceSignCorrection(mtsdf, shape, transformation, opts2.fillRule);
			msdfErrorCorrection(mtsdf, shape, transformation, postErrorCorrectionConfig);
			break;
		default:;
		}
	}

	// writeOutput<3>(msdf, opts2.output, opts2.format);
	QImage image(msdf.width(), msdf.height(), QImage::Format_RGBA8888);
	int w = msdf.width();
	int h = msdf.height();
	for (int y = 0; y < h; y++) {
		float const *s = msdf(0, h - y - 1);
		uint8_t *d = image.scanLine(y);
		for (int x = 0; x < w; x++) {
			int R = std::max(0, std::min(255, int{floorf(s[3 * x + 0] * 255 + 0.5f)}));
			int G = std::max(0, std::min(255, int{floorf(s[3 * x + 1] * 255 + 0.5f)}));
			int B = std::max(0, std::min(255, int{floorf(s[3 * x + 2] * 255 + 0.5f)}));
			d[4 * x + 0] = R;
			d[4 * x + 1] = G;
			d[4 * x + 2] = B;
			d[4 * x + 3] = 255;
		}
	}
	return image;

	// Save output
	if (opts2.shapeExport) {
		if (FILE *file = fopen(opts2.shapeExport, "w")) {
			writeShapeDescription(file, shape);
			fclose(file);
		} else
			fputs("Failed to write shape export file.\n", stderr);
	}
	if (opts2.svgExport) {
		if (!saveSvgShape(shape, bounds, opts2.svgExport))
			fputs("Failed to write shape SVG file.\n", stderr);
	}
	const char *error = NULL;
	switch (opts2.mode) {
	case InternalOptions::SINGLE:
	case InternalOptions::PERPENDICULAR:
		if ((error = writeOutput<1>(sdf, opts2.output, opts2.format))) {
			fprintf(stderr, "%s\n", error);
			return {};
		}
		if (is8bitFormat(opts2.format) && (opts2.testRenderMulti || opts2.testRender || opts2.estimateError))
			simulate8bit(sdf);
		if (opts2.estimateError) {
			double sdfError = estimateSDFError(sdf, shape, transformation, SDF_ERROR_ESTIMATE_PRECISION, opts2.fillRule);
			printf("SDF error ~ %e\n", sdfError);
		}
		if (opts2.testRenderMulti) {
			Bitmap<float, 3> render(opts2.testWidthM, opts2.testHeightM);
			renderSDF(render, sdf, avgScale*opts2.range);
			if (!SAVE_DEFAULT_IMAGE_FORMAT(render, opts2.testRenderMulti))
				fputs("Failed to write test render file.\n", stderr);
		}
		if (opts2.testRender) {
			Bitmap<float, 1> render(opts2.testWidth, opts2.testHeight);
			renderSDF(render, sdf, avgScale*opts2.range);
			if (!SAVE_DEFAULT_IMAGE_FORMAT(render, opts2.testRender))
				fputs("Failed to write test render file.\n", stderr);
		}
		break;
	case InternalOptions::MULTI:
		if ((error = writeOutput<3>(msdf, opts2.output, opts2.format))) {
			fprintf(stderr, "%s\n", error);
			return {};
		}
		if (is8bitFormat(opts2.format) && (opts2.testRenderMulti || opts2.testRender || opts2.estimateError))
			simulate8bit(msdf);
		if (opts2.estimateError) {
			double sdfError = estimateSDFError(msdf, shape, transformation, SDF_ERROR_ESTIMATE_PRECISION, opts2.fillRule);
			printf("SDF error ~ %e\n", sdfError);
		}
		if (opts2.testRenderMulti) {
			Bitmap<float, 3> render(opts2.testWidthM, opts2.testHeightM);
			renderSDF(render, msdf, avgScale*opts2.range);
			if (!SAVE_DEFAULT_IMAGE_FORMAT(render, opts2.testRenderMulti))
				fputs("Failed to write test render file.\n", stderr);
		}
		if (opts2.testRender) {
			Bitmap<float, 1> render(opts2.testWidth, opts2.testHeight);
			renderSDF(render, msdf, avgScale*opts2.range);
			if (!SAVE_DEFAULT_IMAGE_FORMAT(render, opts2.testRender))
				fputs("Failed to write test render file.\n", stderr);
		}
		break;
	case InternalOptions::MULTI_AND_TRUE:
		if ((error = writeOutput<4>(mtsdf, opts2.output, opts2.format))) {
			fprintf(stderr, "%s\n", error);
			return {};
		}
		if (is8bitFormat(opts2.format) && (opts2.testRenderMulti || opts2.testRender || opts2.estimateError))
			simulate8bit(mtsdf);
		if (opts2.estimateError) {
			double sdfError = estimateSDFError(mtsdf, shape, transformation, SDF_ERROR_ESTIMATE_PRECISION, opts2.fillRule);
			printf("SDF error ~ %e\n", sdfError);
		}
		if (opts2.testRenderMulti) {
			Bitmap<float, 4> render(opts2.testWidthM, opts2.testHeightM);
			renderSDF(render, mtsdf, avgScale*opts2.range);
			if (!SAVE_DEFAULT_IMAGE_FORMAT(render, opts2.testRenderMulti))
				fputs("Failed to write test render file.\n", stderr);
		}
		if (opts2.testRender) {
			Bitmap<float, 1> render(opts2.testWidth, opts2.testHeight);
			renderSDF(render, mtsdf, avgScale*opts2.range);
			if (!SAVE_DEFAULT_IMAGE_FORMAT(render, opts2.testRender))
				fputs("Failed to write test render file.\n", stderr);
		}
		break;
	default:;
	}

	return {};
}


