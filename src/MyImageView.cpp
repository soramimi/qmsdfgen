#include "MyImageView.h"

#include <QPainter>

QImage render_msdf_image(QImage const &msdfimage, QSize size);


MyImageView::MyImageView(QWidget *parent)
	: QWidget{parent}
{

}

void MyImageView::setImage(const QImage &image)
{
	image_ = image;
	rendered_image_ = {};
	update();
}

void MyImageView::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	if (rendered_image_.isNull()) {
		rendered_image_ = render_msdf_image(image_, size());
	}
	QPainter painter(this);
	if (!image_.isNull()) {
		painter.drawImage(rect(), rendered_image_);
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
