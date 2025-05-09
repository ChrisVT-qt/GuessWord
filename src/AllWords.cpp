// AllWords.cpp
// Class definition

// Project includes
#include "AllWords.h"
#include "CallTracer.h"
#include "MessageLogger.h"

// Qt includes
#include <QDebug>
#include <QFile>
#include <QRandomGenerator>
#include <QRegularExpression>



// ================================================================== Lifecycle



///////////////////////////////////////////////////////////////////////////////
// Constructor
AllWords::AllWords()
{
    CALL_IN("");

    // Initialize words
    InitWords();

    // Some other settings
    m_WordSize = 5;
    m_AvoidDuplicateLetters = true;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Destructor
AllWords::~AllWords()
{
    CALL_IN("");

    // Nothing to do.

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Instanciator
AllWords * AllWords::Instance()
{
    CALL_IN("");

    if (!m_Instance)
    {
        m_Instance = new AllWords();
    }

    CALL_OUT("");
    return m_Instance;
}



///////////////////////////////////////////////////////////////////////////////
// Instance
AllWords * AllWords::m_Instance = nullptr;



///////////////////////////////////////////////////////////////////////////////
// Initialize available words
void AllWords::InitWords()
{
    CALL_IN("");

    // Words we know
    m_AllWords.clear();

    QFile word_file(":/resources/Words.txt");
    if (!word_file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        const QString reason = tr("Could not open \"Words.txt\".");
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    // Read all words
    QSet < QString > known_words;
    static const QRegularExpression valid_format("[a-zA-Z]+");
    while (!word_file.atEnd())
    {
        QString new_word = word_file.readLine().trimmed().toLower();

        // Ignore empty lines
        if (new_word.isEmpty())
        {
            continue;
        }

        // Ignore potential comments
        if (new_word[0] == '#')
        {
            continue;
        }

        // Make sure there are no invalid characters
        const QRegularExpressionMatch match = valid_format.match(new_word);
        if (!match.hasMatch())
        {
            qDebug().noquote() << QString("Invalid word \"%1\" in database.")
                .arg(new_word);
            continue;
        }

        // Make sure there are no duplicates
        if (known_words.contains(new_word))
        {
            qDebug().noquote() << QString("Duplicate word \"%1\" in database.")
                .arg(new_word);
            continue;
        }

        // Otherwise, keep the word
        m_AllWords << new_word;
        m_WordsForLength[new_word.size()] << new_word;

        // Check if word has duplicates
        if (HasDuplicateLetters(new_word))
        {
            m_WordsWithDuplicateLetters << new_word;
        }

        // Remember it for duplicate word protection
        known_words << new_word;
    }

    // Words we already had
    m_UsedWords.clear();

    CALL_OUT("");
}



// ===================================================================== Access



///////////////////////////////////////////////////////////////////////////////
// Set word size
void AllWords::SetWordSize(const int mcNewWordSize)
{
    CALL_IN(QString("mcNewWordSize=%1")
        .arg(QString::number(mcNewWordSize)));

    // Check if word size is valid
    if (mcNewWordSize == -1)
    {
        // Any size.
        m_WordSize = -1;
        CALL_OUT("");
        return;
    }

    // No words below 4 letters
    if (mcNewWordSize < 4)
    {
        const QString reason = tr("Invalid new word size %1.")
            .arg(QString::number(mcNewWordSize));
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    // Check if there are any words for this size
    if (!m_WordsForLength.contains(mcNewWordSize))
    {
        const QString reason = tr("No known words for word size %1.")
            .arg(QString::number(mcNewWordSize));
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// If we want to avoid haveing letters occur multiple times
void AllWords::SetAvoidDuplicateLetters(const bool mcNewState)
{
    CALL_IN(QString("mcNewState=%1")
        .arg(mcNewState ? "true" : "false"));

    m_AvoidDuplicateLetters = mcNewState;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Avoiding duplicate letters?
bool AllWords::GetAvoidDuplicateLetters() const
{
    CALL_IN("");

    CALL_OUT("");
    return m_AvoidDuplicateLetters;
}



///////////////////////////////////////////////////////////////////////////////
// Check if a word has duplicate letters
bool AllWords::HasDuplicateLetters(const QString mcWord) const
{
    CALL_IN(QString("mcWord=\"%1\"")
        .arg(mcWord));

    QSet < QString > letters_used;
    for (int index = 0;
         index < mcWord.size();
         index++)
    {
        if (letters_used.contains(mcWord[index]))
        {
            // We do have duplicate letters
            CALL_OUT("");
            return true;
        }
        letters_used << mcWord[index];
    }

    // No duplicate letters
    CALL_OUT("");
    return false;
}



///////////////////////////////////////////////////////////////////////////////
// Add word
void AllWords::AddWord(const QString mcNewWord)
{
    CALL_IN(QString("mcNewWord=\"%1\"")
        .arg(mcNewWord));

    // Check if word is valid
    static const QRegularExpression format("[a-zA-Z]");
    const QRegularExpressionMatch match = format.match(mcNewWord);
    if (!match.hasMatch())
    {
        // Not a valid word
        const QString reason(tr("\"%1\" is not a valid word.")
            .arg(mcNewWord));
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    // Check if minimum size requirements are met
    if (mcNewWord.size() < 4)
    {
        // Nope.
        const QString reason(tr("\"%1\" is too short (minimum of 4 letters).")
            .arg(mcNewWord));
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    // Aleady in the list?
    const QString word = mcNewWord.toLower();
    if (m_AllWords.contains(word))
    {
        const QString reason(tr("\"%1\" is already known.")
            .arg(mcNewWord));
        MessageLogger::Error(CALL_METHOD,
            reason);
        CALL_OUT(reason);
        return;
    }

    // Add to the lists
    m_AllWords << word;
    m_WordsForLength[word.size()] << word;
    if (HasDuplicateLetters(word))
    {
        m_WordsWithDuplicateLetters << word;
    }
    m_NewWords << word;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Dump new words
void AllWords::DumpNewWords() const
{
    CALL_IN("");

    qDebug().noquote() << tr("Here's a list of recently added words:");
    for (const QString & word : m_NewWords)
    {
        qDebug().noquote() << word;
    }

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Get a new word
QString AllWords::GetWord() const
{
    CALL_IN("");

    // Words of the right size
    QSet < QString > available_words;
    if (m_WordSize == -1)
    {
        // Any words
        available_words = m_AllWords;
    } else if (m_WordsForLength.contains(m_WordSize))
    {
        available_words = m_WordsForLength[m_WordSize];
    }

    // Not the ones we already had
    available_words -= m_UsedWords;

    // Check if we avoid duplicate letters
    if (m_AvoidDuplicateLetters)
    {
        available_words -= m_WordsWithDuplicateLetters;
    }

    // Pick one at random if possible
    if (available_words.isEmpty())
    {
        // Cannot pick a word.
        return QString();
    }
    int pick_index =
        QRandomGenerator::global() -> bounded(0, available_words.size() - 1);
    for (auto word_iterator = available_words.begin();
         word_iterator != available_words.end();
         word_iterator++)
    {
        if (pick_index == 0)
        {
            CALL_OUT("");
            return *word_iterator;
        }
        pick_index--;
    }

    // We should not get here
    CALL_OUT("");
    return QString();
}



///////////////////////////////////////////////////////////////////////////////
// Check if a word is valid (according to the database)
bool AllWords::IsValid(const QString mcWord) const
{
    CALL_IN(QString("mcWord=\"%1\"")
        .arg(mcWord));

    CALL_OUT("");
    return m_AllWords.contains(mcWord.toLower());
}



///////////////////////////////////////////////////////////////////////////////
// Reset usage
void AllWords::ResetUsage()
{
    CALL_IN("");

    m_UsedWords.clear();

    CALL_OUT("");
}

