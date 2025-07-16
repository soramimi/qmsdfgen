#include "MyImageView.h"

#include <QPainter>

QImage render_msdf_image(QImage const &msdfimage, QSize size);


MyImageView::MyImageView(QWidget *parent)
	: QWidget{parent}
{


}

void MyImageView::setImage(const QImage &image)
{
	original_msdf_image_ = image;
	scaled_msdf_image_ = {};
	rendered_image_ = {};
	update();
}

void MyImageView::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	if (scaled_msdf_image_.isNull()) {
		int w = width();
		int h = height();
		scaled_msdf_image_ = original_msdf_image_.convertToFormat(QImage::Format_RGBA8888);
		scaled_msdf_image_ = scaled_msdf_image_.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		if (view_mode_ == ViewMode::Rendered) {
			rendered_image_ = render_msdf_image(original_msdf_image_, size());
		}
	}
	QImage img;
	if (view_mode_ == ViewMode::MSDF) {
		img = scaled_msdf_image_;
	} else if (view_mode_ == ViewMode::Rendered) {
		img = rendered_image_;
	}
	QPainter painter(this);
	if (!img.isNull()) {
		painter.drawImage(rect(), img);
	} else {
		painter.fillRect(rect(), Qt::white);
		painter.drawText(rect(), Qt::AlignCenter, tr("No image loaded"));
	}

}

void MyImageView::resizeEvent(QResizeEvent *event)
{
	scaled_msdf_image_ = {};
	rendered_image_ = {};
	QWidget::resizeEvent(event);
}
