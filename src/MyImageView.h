#ifndef MYIMAGEVIEW_H
#define MYIMAGEVIEW_H

#include <QWidget>

enum class ViewMode {
	MSDF,
	Rendered,
};

class MyImageView : public QWidget {
	Q_OBJECT
private:
	ViewMode view_mode_ = ViewMode::Rendered;
	QImage original_msdf_image_;
	QImage scaled_msdf_image_;
	QImage rendered_image_;
protected:
	void paintEvent(QPaintEvent *event);
	void resizeEvent(QResizeEvent *event);
public:
	explicit MyImageView(QWidget *parent = nullptr);

	void setImage(const QImage &image);

	void setViewMode(ViewMode mode)
	{
		view_mode_ = mode;
		scaled_msdf_image_ = {};
		rendered_image_ = {};
		update();
	}
};

#endif // MYIMAGEVIEW_H
