#include "outputhighlighter.h"
#include <QStringBuilder>

OutputHighlighter::OutputHighlighter(QObject *parent) : QObject(parent) {
  // Pre-compile all regex patterns for better performance
  m_successPattern.setPattern(
      "✓|Done\\. The new configuration is|Process finished with exit code: "
      "0|successfully|Success|completed successfully|Build succeeded");
  m_successPattern.setPatternOptions(QRegularExpression::CaseInsensitiveOption);

  m_errorPattern.setPattern(
      "✗|error:|Error:|ERROR|failed|Failed|FAILED|Process finished with exit "
      "code: [1-9]|Process crashed|fatal:|Fatal:|FATAL|exception|Exception|"
      "EXCEPTION");

  m_warningPattern.setPattern("warning:|Warning:|WARNING|warn:|Warn:");

  m_buildingPattern.setPattern(
      "building|Building|Running:|Copying|copying|Cloning|evaluating");
  m_buildingPattern.setPatternOptions(
      QRegularExpression::CaseInsensitiveOption);

  m_statusPattern.setPattern("===.*===");

  // Optimize all patterns
  m_successPattern.optimize();
  m_errorPattern.optimize();
  m_warningPattern.optimize();
  m_buildingPattern.optimize();
  m_statusPattern.optimize();
}

QString OutputHighlighter::escapeHtml(const QString &text) {
  QString result;
  result.reserve(text.size() * 1.1); // Reserve slightly more space

  for (const QChar &ch : text) {
    switch (ch.unicode()) {
    case '&':
      result.append("&amp;");
      break;
    case '<':
      result.append("&lt;");
      break;
    case '>':
      result.append("&gt;");
      break;
    default:
      result.append(ch);
      break;
    }
  }
  return result;
}

QString OutputHighlighter::highlight(const QString &rawText) {
  if (rawText.isEmpty()) {
    return QString();
  }

  QStringList lines = rawText.split('\n');
  QStringList processedLines;
  processedLines.reserve(lines.size());

  for (const QString &line : lines) {
    if (line.isEmpty()) {
      processedLines.append(line);
      continue;
    }

    QString escaped = escapeHtml(line);
    QString processed;

    // Test patterns in order of priority
    if (m_successPattern.match(line).hasMatch()) {
      processed = QStringLiteral(
                      "<span style=\"color: #3fb950; font-weight: bold;\">") %
                  escaped % QStringLiteral("</span>");
    } else if (m_errorPattern.match(line).hasMatch()) {
      processed = QStringLiteral(
                      "<span style=\"color: #f85149; font-weight: bold;\">") %
                  escaped % QStringLiteral("</span>");
    } else if (m_warningPattern.match(line).hasMatch()) {
      processed = QStringLiteral("<span style=\"color: #d29922;\">") % escaped %
                  QStringLiteral("</span>");
    } else if (m_buildingPattern.match(line).hasMatch()) {
      processed = QStringLiteral("<span style=\"color: #79c0ff;\">") % escaped %
                  QStringLiteral("</span>");
    } else if (m_statusPattern.match(line).hasMatch()) {
      processed = QStringLiteral(
                      "<span style=\"color: #d2a8ff; font-weight: bold;\">") %
                  escaped % QStringLiteral("</span>");
    } else {
      processed = escaped;
    }

    processedLines.append(processed);
  }

  return processedLines.join(QStringLiteral("<br/>"));
}
