// Application.cpp
// Class definition

// Project includes
#include "Application.h"
#include "CallTracer.h"
#include "MessageLogger.h"

// Qt includes
#include <QDebug>



// ================================================================== Lifecycle



///////////////////////////////////////////////////////////////////////////////
// Constructor
Application::Application(int & argc, char ** argv)
    : QApplication(argc, argv)
{
    CALL_IN("");

    // Nothing to initialize just now. We'd like the instance to exist before
    // we initialize the GUI as the GUI will try and connect to signals in
    // Application - and it can't do that if the Application has not been fully
    // instanciated!
    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Destructor
Application::~Application()
{
    CALL_IN("");

    // Nothing to do.
    CALL_OUT("");
}

   

///////////////////////////////////////////////////////////////////////////////
// Instanciator
Application * Application::Instance(int & argc, char ** argv)
{
    QStringList arg_values;
    for (int index = 0; index < argc; index++)
    {
        arg_values << argv[index];
    }
    CALL_IN(QString("argc=%1, argv={\"%2\"}")
        .arg(argc).arg(arg_values.join("\", \"")));

    if (!m_Instance)
    {
        m_Instance = new Application(argc, argv);
    }

    CALL_OUT("");
    return m_Instance;
}
    


///////////////////////////////////////////////////////////////////////////////
// Instance
Application * Application::Instance()
{
    CALL_IN("");

    if (!m_Instance)
    {
        // Error.
        const QString reason =
            tr("Trying to access uninitialized instance. Should not happen.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return nullptr;
    }

    CALL_OUT("");
    return m_Instance;
}
    


///////////////////////////////////////////////////////////////////////////////
// Instance
Application * Application::m_Instance = nullptr;
