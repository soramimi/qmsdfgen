#include "ApplicationGlobal.h"
#include "MainWindow.h"
#include <memory>
#include <QFileIconProvider>
#include <QBuffer>
// #include "IncrementalSearch.h"
// #include "MemoryReader.h"
// #include "gunzip.h"


struct ApplicationGlobal::Private {
};

ApplicationGlobal::ApplicationGlobal()
	: m(new Private)
{
}

ApplicationGlobal::~ApplicationGlobal()
{
	delete m;
}

