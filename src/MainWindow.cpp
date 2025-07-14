#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QFileDialog>
#include <QMessageBox>

#include "msdfgen.h"
#include "core/ShapeDistanceFinder.h"
#include "ext/import-svg.h"
#include "ext/save-png.h"
#include <cstdint>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
{
	ui->setupUi(this);
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::on_pushButton_browse_input_clicked()
{
	QString path;
	path = QFileDialog::getOpenFileName(this, tr("Input file"), path, "SVG files (*.svg);;All files (*.*)");
	if (!path.isEmpty()) {
		ui->lineEdit_input->setText(path);
	}
}


void MainWindow::on_pushButton_browse_output_clicked()
{
	QString path;
	path = QFileDialog::getSaveFileName(this, tr("Output file"), path, "PNG files (*.png);;All files (*.*)");
	if (!path.isEmpty()) {
		ui->lineEdit_output->setText(path);
	}
}

QString MainWindow::pathInput() const
{
	return ui->lineEdit_input->text();
}

QString MainWindow::pathOutput() const
{
	return ui->lineEdit_output->text();
}


template <int N>
static const char *writeOutput(const msdfgen::BitmapConstRef<float, N> &bitmap, const char *filename) {
	if (filename) {
		return msdfgen::savePng(bitmap, filename) ? NULL : "Failed to write output PNG image.";
#if 0
		if (format == AUTO) {
		#if defined(MSDFGEN_EXTENSIONS) && !defined(MSDFGEN_DISABLE_PNG)
			if (cmpExtension(filename, ".png")) format = PNG;
		#else
			if (cmpExtension(filename, ".png"))
				return "PNG format is not available in core-only version.";
		#endif
			else if (cmpExtension(filename, ".bmp")) format = BMP;
			else if (cmpExtension(filename, ".tiff") || cmpExtension(filename, ".tif")) format = TIFF;
			else if (cmpExtension(filename, ".rgba")) format = RGBA;
			else if (cmpExtension(filename, ".fl32")) format = FL32;
			else if (cmpExtension(filename, ".txt")) format = TEXT;
			else if (cmpExtension(filename, ".bin")) format = BINARY;
			else
				return "Could not deduce format from output file name.";
		}
		switch (format) {
		#if defined(MSDFGEN_EXTENSIONS) && !defined(MSDFGEN_DISABLE_PNG)
			case PNG: return savePng(bitmap, filename) ? NULL : "Failed to write output PNG image.";
		#endif
			case BMP: return saveBmp(bitmap, filename) ? NULL : "Failed to write output BMP image.";
			case TIFF: return saveTiff(bitmap, filename) ? NULL : "Failed to write output TIFF image.";
			case RGBA: return saveRgba(bitmap, filename) ? NULL : "Failed to write output RGBA image.";
			case FL32: return saveFl32(bitmap, filename) ? NULL : "Failed to write output FL32 image.";
			case TEXT: case TEXT_FLOAT: {
				FILE *file = fopen(filename, "w");
				if (!file) return "Failed to write output text file.";
				if (format == TEXT)
					writeTextBitmap(file, bitmap.pixels, N*bitmap.width, bitmap.height);
				else if (format == TEXT_FLOAT)
					writeTextBitmapFloat(file, bitmap.pixels, N*bitmap.width, bitmap.height);
				fclose(file);
				return NULL;
			}
			case BINARY: case BINARY_FLOAT: case BINARY_FLOAT_BE: {
				FILE *file = fopen(filename, "wb");
				if (!file) return "Failed to write output binary file.";
				if (format == BINARY)
					writeBinBitmap(file, bitmap.pixels, N*bitmap.width*bitmap.height);
				else if (format == BINARY_FLOAT)
					writeBinBitmapFloat(file, bitmap.pixels, N*bitmap.width*bitmap.height);
				else if (format == BINARY_FLOAT_BE)
					writeBinBitmapFloatBE(file, bitmap.pixels, N*bitmap.width*bitmap.height);
				fclose(file);
				return NULL;
			}
			default:;
		}
	} else {
		if (format == AUTO || format == TEXT)
			writeTextBitmap(stdout, bitmap.pixels, N*bitmap.width, bitmap.height);
		else if (format == TEXT_FLOAT)
			writeTextBitmapFloat(stdout, bitmap.pixels, N*bitmap.width, bitmap.height);
		else
			return "Unsupported format for standard output.";
#endif
	}
	return NULL;
}

int msdfmain(int argc, const char *const *argv);

QImage render_msdf_image(QImage const &msdfimage, QSize size)
{
	if (msdfimage.isNull()) return {};
	int w = size.width();
	int h = size.height();
	QImage src = msdfimage.convertToFormat(QImage::Format_RGBA8888);
	src = src.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	QImage dst(w, h, QImage::Format_Grayscale8);
	for (int y = 0; y < h; ++y) {
		uint8_t const *s = (uint8_t const *)src.constScanLine(y);
		uint8_t *d = (uint8_t *)dst.scanLine(y);
		for (int x = 0; x < w; ++x) {
			auto Median = [](uint8_t r, uint8_t g, uint8_t b) {
				if (r < g) {
					if (g < b) return g; // r < g < b
					else if (r < b) return b; // r < b <= g
					else return r; // b <= r <= g
				} else {
					if (r < b) return r; // g < r < b
					else if (g < b) return b; // g < b <= r
					else return g; // b <= g <= r
				}
			};
			uint8_t r = s[4 * x + 0];
			uint8_t g = s[4 * x + 1];
			uint8_t b = s[4 * x + 2];
			d[x] = Median(r, g, b) < 128 ? 0 : 255;
		}
	}
	return dst;
}


void MainWindow::on_pushButton_save_clicked()
{
	QString srcpath = pathInput();
	QString dstpath = pathOutput();
#if 1
	if (srcpath.isEmpty()) {
		QMessageBox::warning(this, tr("Warning"), tr("Input file path is empty!"));
		return;
	}
	if (dstpath.isEmpty()) {
		QMessageBox::warning(this, tr("Warning"), tr("Output file path is empty!"));
		return;
	}
#else
	srcpath = "../example/twitter.svg";
	dstpath = "./twitter.png";
#endif

	std::string srcpath_std = srcpath.toStdString();
	std::string dstpath_std = dstpath.toStdString();
	std::string size = "64";

	std::vector<char const *> args;
	args.push_back("");
	args.push_back("-svg");
	args.push_back(srcpath_std.c_str());
	args.push_back("-o");
	args.push_back(dstpath_std.c_str());
	args.push_back("-size");
	args.push_back(size.c_str());
	args.push_back(size.c_str());
	args.push_back("-autoframe");
	msdfmain(args.size(), args.data());

	QSize sz = ui->widget_view->size();

	QImage image(dstpath);
	// image = render_msdf_image(image, sz);
	ui->widget_view->setImage(image);
}

