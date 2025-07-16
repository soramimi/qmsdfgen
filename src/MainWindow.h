#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class ApplicationSettings;

namespace Ui {
class MainWindow;
}

class ApplicationBasicData;


class MainWindow : public QMainWindow {
	Q_OBJECT
private:
	Ui::MainWindow *ui;
	QImage msdf_image_;
	QString pathInput() const;
	QString saveMSDF();
	void updateImage();
public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();
	void show();
	ApplicationSettings *appsettings();
	const ApplicationSettings *appsettings() const;
private slots:
	void on_pushButton_browse_input_clicked();
	void on_radioButton_msdf_clicked();
	void on_radioButton_rendered_clicked();
	void on_pushButton_save_clicked();

protected:
	void closeEvent(QCloseEvent *event);
};

#endif // MAINWINDOW_H
