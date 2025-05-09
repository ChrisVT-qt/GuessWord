// MainWindow.cpp
// Class definition

// Project includes
#include "AllWords.h"
#include "Application.h"
#include "CallTracer.h"
#include "MainWindow.h"

// Qt includes
#include <QAction>
#include <QGridLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>



// ================================================================== Lifecycle



///////////////////////////////////////////////////////////////////////////////
// Constructor
MainWindow::MainWindow()
{
    CALL_IN("");

    // Initialize
    InitActions();

    m_StatusToColor[Status_NotTried] = QColor(255,255,255);
    m_StatusToColor[Status_CorrectPosition] = QColor(180,255,180);
    m_StatusToColor[Status_NotInWord] = QColor(255,180,180);
    m_StatusToColor[Status_WrongPosition] = QColor(255,255,180);

    // Set width
    setMinimumSize(600, 400);
    resize(1500, 600);

    NewGame();

    show();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Destructor
MainWindow::~MainWindow()
{
    CALL_IN("");

    // Nothing to do.

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Instanciator
MainWindow * MainWindow::Instance()
{
    CALL_IN("");

    if (!m_Instance)
    {
        m_Instance = new MainWindow();
    }

    CALL_OUT("");
    return m_Instance;
}
    


///////////////////////////////////////////////////////////////////////////////
// Instance
MainWindow * MainWindow::m_Instance = nullptr;



// ======================================================================== GUI



///////////////////////////////////////////////////////////////////////////////
// Initialize actions
void MainWindow::InitActions()
{
    CALL_IN("");

    // We'll need this
    QAction * action;

    // File menu
    QMenuBar * menubar = new QMenuBar(nullptr);
    QMenu * file_menu = menubar -> addMenu(tr("&File"));

    // == Apple menu
    // About
    action = new QAction(tr("About"), this);
    action -> setMenuRole(QAction::AboutRole);
    connect(action, SIGNAL(triggered()),
        this, SLOT(About()));
    file_menu -> addAction(action);

    // New game
    action = new QAction(tr("New Game"), this);
    action -> setShortcut(tr("Ctrl+N"));
    connect(action, SIGNAL(triggered()),
        this, SLOT(NewGame()));
    file_menu -> addAction(action);

    // Quit
    action = new QAction(tr("Quit Home"), this);
    action -> setShortcut(tr("Ctrl+Q"));
    action -> setMenuRole(QAction::QuitRole);
    connect(action, SIGNAL(triggered()),
        this, SLOT(Quit()));
    file_menu -> addAction(action);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Action handler: About
void MainWindow::About() const
{
    CALL_IN("");

    // !!! Not implemented

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Action handler: New Game
void MainWindow::NewGame()
{
    CALL_IN("");

    // Reset it all
    m_Tries.clear();
    m_Tries << QString();

    m_LetterStatus.clear();
    for (char letter = 'a';
         letter <= 'z';
         letter++)
    {
        m_LetterStatus[QString(letter)] = Status_NotTried;
    }

    m_Word = AllWords::Instance() -> GetWord();
    if (m_Word.isEmpty())
    {
        QMessageBox::information(this, tr("Out of words"),
            tr("I ran out of words!"));
    }

    m_LastTryFinished = false;

    update();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Action handler: Quit
void MainWindow::Quit()
{
    CALL_IN("");

    Application * a = Application::Instance();
    a -> quit();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Get key
void MainWindow::keyPressEvent(QKeyEvent * mpEvent)
{
    CALL_IN("mpEvent=...");

    if (mpEvent -> modifiers() != Qt::NoModifier)
    {
        QWidget::keyPressEvent(mpEvent);
        CALL_OUT("");
        return;
    }

    const int key = mpEvent -> key();
    if (key == Qt::Key_Return ||
        key == Qt::Key_Enter)
    {
        const QString current = m_Tries.last();
        if (current.size() == m_Word.size())
        {
            CheckNewTry();
            repaint();
        }
        CALL_OUT("");
        return;
    }
    if (key == Qt::Key_Backspace ||
        key == Qt::Key_Delete)
    {
        QString current = m_Tries.takeLast();
        if (current.size() > 0)
        {
            current = current.left(current.size() - 1);
        }
        m_Tries << current;
        repaint();
        CALL_OUT("");
        return;
    }
    const QString text = mpEvent -> text().toLower();
    if (text >= "a" && text <= "z")
    {
        QString current = m_Tries.takeLast();
        if (current.size() < m_Word.size())
        {
            current += text;
        }
        m_Tries << current;
        repaint();
        CALL_OUT("");
        return;
    }

    // Otherwise, let base class handle it.
    QWidget::keyPressEvent(mpEvent);
    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Redraw
void MainWindow::paintEvent(QPaintEvent * mpEvent)
{
    CALL_IN("mpEvent=...");

    // Accept event
    mpEvent -> accept();

    // Some prep
    QFont small_font;
    QFont large_font;
    large_font.setPixelSize(30);

    // Draw everything
    QPainter painter(this);
    int y = 20;

    // Past tries
    const int try_scale = 40;
    const int try_space = 5;
    int left =
        (width() - (m_Word.size() * (try_scale + try_space) - try_space)) / 2;
    painter.setFont(large_font);
    QFontMetrics metrics(large_font);
    for (int this_try = 0;
         this_try < m_Tries.size();
         this_try++)
    {
        const bool is_current_try =
            (!m_LastTryFinished) && (this_try == m_Tries.size() - 1);
        const QString text = m_Tries[this_try];

        int x = left;
        for (int index = 0;
             index < m_Word.size();
             index++)
        {
            QColor color(255, 255, 255);
            if (!is_current_try)
            {
                if (text[index] == m_Word[index])
                {
                    color = m_StatusToColor[Status_CorrectPosition];
                } else if (m_Word.contains(text[index]))
                {
                    color = m_StatusToColor[Status_WrongPosition];
                } else
                {
                    color = m_StatusToColor[Status_NotInWord];
                }
            }
            painter.fillRect(x,
                             y,
                             try_scale,
                             try_scale,
                             color);
            painter.drawRect(x,
                             y,
                             try_scale,
                             try_scale);
            if (index < text.size())
            {
                const QString letter = text[index].toUpper();
                const int dx =
                    (try_scale - metrics.horizontalAdvance(letter)) / 2;
                painter.drawText(x + dx,
                                 y + 30,
                                 letter);
            }
            x += try_scale + try_space;
        }
        y += try_scale + try_space;
    }

    // Alphabet
    const int letter_scale = 20;
    const int letters_per_row = 8;
    left = (width() - letters_per_row * letter_scale) / 2;
    y += 20;
    painter.setFont(small_font);
    metrics = QFontMetrics(small_font);
    for (char letter = 'a';
         letter <= 'z';
         letter++)
    {
        const int row = (letter - 'a') / letters_per_row;
        const int column = (letter - 'a') % letters_per_row;
        if (column == 0 &&
            row == ('z' - 'a') / letters_per_row)
        {
            left = (width() - ('z' - letter + 1) * letter_scale) / 2;
        }
        const QString letter_text = QString(letter);
        const Status status = m_LetterStatus[letter_text];
        QColor color(m_StatusToColor[status]);
        painter.fillRect(left + column * letter_scale,
                         y + row * letter_scale,
                         letter_scale,
                         letter_scale,
                         color);
        painter.drawRect(left + column * letter_scale,
                         y + row * letter_scale,
                         letter_scale,
                         letter_scale);
        const int dx = (letter_scale
            - metrics.horizontalAdvance(letter_text.toUpper())) / 2;
        painter.drawText(left + column * letter_scale + dx,
                         y + (row + .5) * letter_scale + 5,
                         letter_text.toUpper());
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Check if finished
void MainWindow::CheckNewTry()
{
    CALL_IN("");

    // Check if there a duplicates if that's not currently desired
    AllWords * aw = AllWords::Instance();
    const QString word = m_Tries.last();
    if (aw -> GetAvoidDuplicateLetters() &&
        aw -> HasDuplicateLetters(word))
    {
        CALL_OUT("");
        return;
    }

    // Check if that's a real word
    if (!aw -> IsValid(word))
    {
        const int response = QMessageBox::question(this,
            tr("New word"),
            tr("Is \"%1\" actually a valid word?")
                .arg(word));
        if (response == QMessageBox::Yes)
        {
            aw -> AddWord(word);
        } else
        {
            CALL_OUT("");
            return;
        }
    }

    // Update letter status
    for (int index = 0;
         index < m_Word.size();
         index++)
    {
        const QString letter = word[index];
        if (letter == m_Word[index])
        {
            m_LetterStatus[letter] = Status_CorrectPosition;
        } else if (m_Word.contains(letter) &&
                   m_LetterStatus[letter] != Status_CorrectPosition)
        {
            m_LetterStatus[letter] = Status_WrongPosition;
        } else
        {
            m_LetterStatus[letter] = Status_NotInWord;
        }
    }

    // Finished?
    if (word == m_Word)
    {
        m_LastTryFinished = true;
        repaint();
        QMessageBox::information(this, tr("You won!"),
            tr("Congratulations! You correctly guessed the word after %1 %2.")
                .arg(QString::number(m_Tries.size()),
                     m_Tries.size() == 1 ? tr("attempt") : tr("attempts")));
        NewGame();
    } else
    {
        // Next try
        m_Tries << QString();
    }

    CALL_OUT("");
}

