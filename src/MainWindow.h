// MainWindow.h
// Class definition

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// Qt includes
#include <QHash>
#include <QList>
#include <QWidget>



// Class definition
class MainWindow
    : public QWidget
{
    Q_OBJECT
    
    
    
    // ============================================================== Lifecycle
private:
    // Constructor
    MainWindow();
    
public:
    // Destructor
    virtual ~MainWindow();
    
    // Instanciator
    static MainWindow * Instance();

private:
    // Instance
    static MainWindow * m_Instance;
    


    // ==================================================================== GUI
private:
    // Initialize Actions
    void InitActions();

private slots:
    // Action handlers
    void About() const;
    void NewGame();
    void Quit();

private:
    QString m_Word;
    QList < QString > m_Tries;
    bool m_LastTryFinished;

    enum Status {
        Status_NotTried,
        Status_NotInWord,
        Status_WrongPosition,
        Status_CorrectPosition
    };
    QHash < QString, Status > m_LetterStatus;
    QHash < Status, QColor > m_StatusToColor;

protected:
    // Get key
    virtual void keyPressEvent(QKeyEvent * mpEvent);

    // Redraw
    void paintEvent(QPaintEvent * mpEvent);

    // Check if finished
    void CheckNewTry();
};

#endif

