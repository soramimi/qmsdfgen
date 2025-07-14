#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT
private:
	Ui::MainWindow *ui;
	QString pathInput() const;
	QString pathOutput() const;
public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();

private slots:
	void on_pushButton_browse_input_clicked();

	void on_pushButton_browse_output_clicked();

	void on_pushButton_save_clicked();
};

#endif // MAINWINDOW_H
