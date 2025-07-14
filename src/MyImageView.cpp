#include "MyImageView.h"

#include <QPainter>

QImage render_msdf_image(QImage const &msdfimage, QSize size);


MyImageView::MyImageView(QWidget *parent)
	: QWidget{parent}
{

}

void MyImageView::setImage(const QImage &image)
{
	msdf_image_ = image;
	rendered_image_ = {};
	update();
}

void MyImageView::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	if (rendered_image_.isNull()) {
		rendered_image_ = render_msdf_image(msdf_image_, size());
	}
	QPainter painter(this);
	QImage img = rendered_image_;
	if (!img.isNull()) {
		painter.drawImage(rect(), img);
	} else {
		painter.fillRect(rect(), Qt::white);
		painter.drawText(rect(), Qt::AlignCenter, tr("No image loaded"));
	}

}

void MyImageView::resizeEvent(QResizeEvent *event)
{
	rendered_image_ = {};
	QWidget::resizeEvent(event);
}
