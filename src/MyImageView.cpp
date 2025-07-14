#include "MyImageView.h"

#include <QPainter>


MyImageView::MyImageView(QWidget *parent)
	: QWidget{parent}
{

}

void MyImageView::setImage(const QImage &image)
{
	image_ = image;
	update();
}

void MyImageView::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	QPainter painter(this);
	if (!image_.isNull()) {
		painter.drawImage(rect(), image_);
	} else {
		painter.fillRect(rect(), Qt::white);
		painter.drawText(rect(), Qt::AlignCenter, tr("No image loaded"));
	}

}
