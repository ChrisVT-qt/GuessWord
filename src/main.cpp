// main.cpp

// Project includes
#include "AllWords.h"
#include "Application.h"
#include "Deploy.h"
#include "MainWindow.h"

// Qt includes
#include <QDebug>
#include <QFile>



///////////////////////////////////////////////////////////////////////////////
// Main
int main(int mNumParameters, char * mpParameter[])
{
    // Handle command line parameters for GUI
    Application * app = Application::Instance(mNumParameters, mpParameter);

    // Make sure main window is the active one
    MainWindow * main_window = MainWindow::Instance();
    main_window -> raise();
    main_window -> activateWindow();

    // Hand over control to the GUI
    const int result = app -> exec();

    // Dump new words so we can put them in the dictionary
    if (!DEPLOY)
    {
        AllWords::Instance() -> DumpNewWords();
    }

    // Done here
    return result;
}
