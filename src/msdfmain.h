#ifndef MSDFMAIN_H
#define MSDFMAIN_H

#include <QImage>
#include <string>

struct Options {
	std::string input_file;
	int width = 64;
	int height = 64;
};

QImage msdfmain(Options const &opts);

#endif // MSDFMAIN_H
