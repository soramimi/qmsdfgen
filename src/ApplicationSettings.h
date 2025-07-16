#ifndef APPLICATIONSETTINGS_H
#define APPLICATIONSETTINGS_H

// #include "GenerativeAI.h"

#include <QColor>
#include <QString>

#define ORGANIZATION_NAME "soramimi.jp"
#define APPLICATION_NAME "qmsdfgen"

class ApplicationBasicData {
public:
	QString organization_name = ORGANIZATION_NAME;
	QString application_name = APPLICATION_NAME;
	QString this_executive_program;
	QString generic_config_dir;
	QString app_config_dir;
	QString config_file_path;
};

class ApplicationSettings {
public:
	bool remember_and_restore_window_position = true;
};

#endif // APPLICATIONSETTINGS_H
