#include "MainWindow.h"
#include "MySettings.h"
#include "msdfmain.h"
#include "ui_MainWindow.h"

#include "ApplicationGlobal.h"
#include <QFileDialog>
#include <QMessageBox>

#include "msdfgen.h"
#include "core/ShapeDistanceFinder.h"
#include "ext/import-svg.h"
#include "ext/save-png.h"
#include "ApplicationSettings.h"
#include <cstdint>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	ui->radioButton_rendered->click();

	if (appsettings()->remember_and_restore_window_position) {
		Qt::WindowStates state = windowState();

		MySettings settings;
		settings.beginGroup("MainWindow");
		bool maximized = settings.value("Maximized").toBool();
		restoreGeometry(settings.value("Geometry").toByteArray());
		settings.endGroup();
		if (maximized) {
			state |= Qt::WindowMaximized;
			setWindowState(state);
		}
	}
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::show()
{
	{
		MySettings settings;
		settings.beginGroup("File");
		QString srcpath = settings.value("RecentInputFile", "").toString();
		settings.endGroup();
		ui->lineEdit_input->setText(srcpath);
		updateImage();
	}



	QMainWindow::show();
}

ApplicationSettings *MainWindow::appsettings()
{
	return &global->appsettings;
}

const ApplicationSettings *MainWindow::appsettings() const
{
	return &global->appsettings;
}


void MainWindow::on_pushButton_browse_input_clicked()
{
	QString path;
	path = QFileDialog::getOpenFileName(this, tr("Input file"), path, "SVG files (*.svg);;All files (*.*)");
	if (!path.isEmpty()) {
		ui->lineEdit_input->setText(path);
		{
			MySettings s;
			s.beginGroup("File");
			s.setValue("RecentInputFile", path);
			s.endGroup();
		}
		updateImage();
	}
}




QString MainWindow::pathInput() const
{
	return ui->lineEdit_input->text();
}

QString MainWindow::saveMSDF()
{
	if (msdf_image_.isNull()) {
		QMessageBox::warning(this, tr("Warning"), tr("No MSDF image to save!"));
		return QString();
	}

	QString path = pathInput();
	if (path.endsWith(".svg", Qt::CaseInsensitive)) {
		path.chop(4);
		path += ".png";
	}
	path = QFileDialog::getSaveFileName(this, tr("Output file"), path, "PNG files (*.png);;All files (*.*)");
	if (!path.isEmpty()) {
		msdf_image_.save(path, "PNG");
	}
}




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


void MainWindow::updateImage()
{
	QString srcpath = pathInput();
#if 1
	if (srcpath.isEmpty()) {
		QMessageBox::warning(this, tr("Warning"), tr("Input file path is empty!"));
		return;
	}
#else
	srcpath = "../example/twitter.svg";
	dstpath = "./twitter.png";
#endif

	std::string srcpath_std = srcpath.toStdString();

	std::vector<char const *> args;
	args.push_back("");
	Options opts;
	opts.input_file = srcpath_std;
	opts.width = 64;
	opts.height = 64;
	msdf_image_ = msdfmain(opts);

	ui->widget_view->setImage(msdf_image_);
}




void MainWindow::closeEvent(QCloseEvent *event)
{
	MySettings settings;

	if (appsettings()->remember_and_restore_window_position) {
		setWindowOpacity(0);
		Qt::WindowStates state = windowState();
		bool maximized = (state & Qt::WindowMaximized) != 0;
		if (maximized) {
			state &= ~Qt::WindowMaximized;
			setWindowState(state);
		}
		{
			settings.beginGroup("MainWindow");
			settings.setValue("Maximized", maximized);
			settings.setValue("Geometry", saveGeometry());
			settings.endGroup();
		}
	}

	QMainWindow::closeEvent(event);
}


void MainWindow::on_radioButton_msdf_clicked()
{
	ui->widget_view->setViewMode(ViewMode::MSDF);
}


void MainWindow::on_radioButton_rendered_clicked()
{
	ui->widget_view->setViewMode(ViewMode::Rendered);

}


void MainWindow::on_pushButton_save_clicked()
{
	QString path = saveMSDF();

}

