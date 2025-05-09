// AllWords.h
// Class definition

#ifndef ALLWORDS_H
#define ALLWORDS_H

// Qt includes
#include <QObject>
#include <QSet>
#include <QString>



// Class definition
class AllWords
    : public QObject
{
    Q_OBJECT



    // ============================================================== Lifecycle
private:
    // Constructor
    AllWords();

public:
    // Destructor
    virtual ~AllWords();

public:
    // Instanciator
    static AllWords * Instance();

private:
    // Instance
    static AllWords * m_Instance;

    // Initialize
    void InitWords();

    // Words
    QSet < QString > m_AllWords;
    QHash < int, QSet < QString > > m_WordsForLength;
    QSet < QString > m_UsedWords;
    QSet < QString > m_WordsWithDuplicateLetters;


    // ================================================================= Access
public:
    // Set word size
    void SetWordSize(const int mcNewWordSize);
private:
    int m_WordSize;

public:
    // If we want to avoid haveing letters occur multiple times
    void SetAvoidDuplicateLetters(const bool mcNewState);
    bool GetAvoidDuplicateLetters() const;
private:
    bool m_AvoidDuplicateLetters;
public:
    // Check if a word has duplicate letters
    bool HasDuplicateLetters(const QString mcWord) const;

    // Add word
    void AddWord(const QString mcNewWord);

    // Dump new words
    void DumpNewWords() const;
private:
    QList < QString > m_NewWords;

public:
    // Get a new word
    QString GetWord() const;

    // Check if a word is valid (according to the database)
    bool IsValid(const QString mcWord) const;

    // Reset usage
    void ResetUsage();
};

#endif
