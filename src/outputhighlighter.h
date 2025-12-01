#ifndef OUTPUTHIGHLIGHTER_H
#define OUTPUTHIGHLIGHTER_H

#include <QObject>
#include <QRegularExpression>
#include <QString>

class OutputHighlighter : public QObject {
  Q_OBJECT

public:
  explicit OutputHighlighter(QObject *parent = nullptr);

  Q_INVOKABLE QString highlight(const QString &rawText);

private:
  // Pre-compiled regex patterns for performance
  QRegularExpression m_successPattern;
  QRegularExpression m_errorPattern;
  QRegularExpression m_warningPattern;
  QRegularExpression m_buildingPattern;
  QRegularExpression m_statusPattern;

  QString escapeHtml(const QString &text);
};

#endif // OUTPUTHIGHLIGHTER_H
