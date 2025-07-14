#ifndef MYIMAGEVIEW_H
#define MYIMAGEVIEW_H

#include <QWidget>

class MyImageView : public QWidget {
	Q_OBJECT
private:
	QImage image_;
	QImage rendered_image_;

	// QWidget interface
public:
	explicit MyImageView(QWidget *parent = nullptr);

	void setImage(const QImage &image);

signals:


	// QWidget interface
protected:
	void paintEvent(QPaintEvent *event);

	// QWidget interface
protected:
	void resizeEvent(QResizeEvent *event);
};

#endif // MYIMAGEVIEW_H
